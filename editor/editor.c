/* TODO:
    - needs_redraw per ora serve a niente

    - ref: https://viewsourcecode.org/snaptoken/kilo/index.html
*/

#include <stdbool.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <termios.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

typedef struct
{
    String_Builder *items;
    size_t count;
    size_t capacity;
} Buffer;

typedef struct
{
    char *file_name;
    Buffer file_buf;
    int dirty; // TODO: quando si esegue un'azione che modifica lo state si fa una copia (o comunque si copia parte dello state) che si inserisce in una struttura dati che permette di ciclare sulle varie 'versioni' dello state. dirty tiene conto di quale state si sta guardando e quante modifiche sono state fatte

    int cx;
    int cy;

    int n_screenrows;
    int n_cols;

    int n_rows;
    int n_curr_row;
    int n_pages;
    int n_curr_page;
    int offset;

    int N;
    bool needs_redraw;

} EditorState;

#define N_DEFAULT -1 
#define N_OR_DEFAULT_VALUE(n) (state.N == N_DEFAULT ? (n) : state.N)
#define QUIT_DEFAULT 2
#define CRNL "\r\n"
#define CTRL_MASK 0x1F
#define CTRL_KEY(k) ((k) & CTRL_MASK)
#define WELCOME "NEOLINTY"
#define BUF_ROW(i) state.file_buf.items[i]

// Global variables ////////////////////////////// 
EditorState state = {0};
String_Builder screen_buf = {0};
String_Builder msg_buf = {0};
bool msg_needs_redraw = true;
int quit_times = QUIT_DEFAULT;
//////////////////////////////////////////////////

void dump_on_file(char *filename, char *format, ...)
{
    FILE *f = fopen(filename, "a");
    if (f == NULL) return;
    va_list fmt; 
    va_start(fmt, format);
    vfprintf(f, format, fmt);
    va_end(fmt);
    fclose(f);
}

struct termios terminal_old, terminal_new;
void kill_terminal(const char *s)
{
    write(STDOUT_FILENO, "\x1B[H", 3);
    write(STDOUT_FILENO, "\x1b[2J", 4);
    perror(s);
    exit(1);
}
void reset_terminal(void) { if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal_old) == -1) kill_terminal("tcsetattr"); }
void init_terminal() {
    if (tcgetattr(STDIN_FILENO, &terminal_old) == -1) kill_terminal("tcgetattr"); 
    terminal_new = terminal_old; 
    terminal_new.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    terminal_new.c_oflag &= ~(OPOST);
    terminal_new.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    terminal_new.c_cflag |= (CS8);
    terminal_new.c_cc[VMIN] = 0;
    terminal_new.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal_new) == -1) kill_terminal("tcsetattr");
    atexit(reset_terminal);
}

char read_key() {
    int read_n;
    char c = '\0';
    while ((read_n = read(STDIN_FILENO, &c, 1)) != 1) {
        if (read_n == -1 && errno != EAGAIN) kill_terminal("read");
    }

    return c;
}

bool get_cursor_pos(int *rows, int *cols)
{
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return false;
    char buf[5] = {0};
    int i = 0;
    while (i < 4) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) return false;
        if (buf[i] == 'R') return false; // TODO: perche'?
        i++;
    }
    buf[i] = '\0';
    if (buf[0] != '\x1b' || buf[1] != '[') return false;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return false;
    return true;
}

bool get_window_size(int *rows, int *cols)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return false;
        else return get_cursor_pos(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
    }
    return true;
}

void EditorState_init()
{
    state.file_buf = (Buffer){0};

    state.n_curr_page = 0;
    state.n_curr_row = 0;

    if (!get_window_size(&(state.n_screenrows), &(state.n_cols))) {
        perror("window size");
        exit(1);
    }
    state.n_screenrows -= 2; // status bar e message bar

    state.needs_redraw = true;
    state.N = N_DEFAULT;
}

void open_file(char *filename)
{
    state.n_rows = 0;
    state.file_buf.count = 0;
    state.n_pages = 0;

    if (filename == NULL) return;

    FILE *file = fopen(filename, "r");

    if (strchr(filename, '/')) {
        char *tmp = filename + strlen(filename) - 1;
        while (*tmp != '/') tmp--;
        filename = tmp+1;
    }
    state.file_name = strdup(filename);

    if (file == NULL) return;

    ssize_t res; 
    size_t len;
    int n_rows = 0;
    char *line = NULL;
    while ((res = getline(&line, &len, file)) != -1) {
        String_Builder row = {0};
        sb_append_buf(&row, line, strlen(line)-1);
        sb_append_null(&row);
        da_append(&state.file_buf, row);
        n_rows++;
    }
    state.n_rows = n_rows;
    state.n_pages = n_rows / state.n_screenrows;
    free(line);
}

void draw_message_bar()
{
    if (msg_needs_redraw) sb_append_buf(&screen_buf, "\x1b[K", 3);
    sb_append_buf(&screen_buf, "\x1b[7m", 4);
    char msg[80] = {0};
    int len = snprintf(msg, msg_buf.count, "%s", msg_buf.items);
    if (len > state.n_cols) len = state.n_cols; // truncate

    sb_append_buf(&screen_buf, " ", 1);
    sb_append_buf(&screen_buf, msg, len);

    for (int i = len+1; i < state.n_cols; i++)
        sb_append_buf(&screen_buf, " ", 1);

    sb_append_buf(&screen_buf, "\x1b[m", 3);
    sb_append_buf(&screen_buf, CRNL, 2);
    msg_needs_redraw = false;
}

void set_message(char *s)
{
    msg_buf.count = 0;
    sb_append_buf(&msg_buf, s, strlen(s));
    sb_append_null(&msg_buf);
    msg_needs_redraw = true;
}

void draw_welcome()
{
    int padding = (state.n_cols - strlen(WELCOME)) / 2;
    while (padding > 0) { sb_append_buf(&screen_buf, " ", 1); padding--; }
    sb_append_buf(&screen_buf, WELCOME, strlen(WELCOME));
    sb_append_buf(&screen_buf, CRNL, 2);
}

void draw_rows()
{
    for (int i = 0; i < state.n_screenrows; i++) {
        if (i < state.n_rows) {
            char *row = BUF_ROW(state.n_curr_page*state.n_screenrows+state.offset+i).items;
            sb_append_buf(&screen_buf, row, strlen(row));    
        }
        sb_append_buf(&screen_buf, "\x1b[K", 3);
        sb_append_buf(&screen_buf, CRNL, 2);
    }
}

int curr_row_percentile() { return (int)(((float)state.n_curr_row/(state.n_rows))*100); }

// TODO: la status bar viene ridisegnata quando needs_redraw e' true, forse devo distinguere diverse variabili (quindi inserire status_bar_needs_redraw)
void draw_status_bar()
{
    sb_append_buf(&screen_buf, "\x1b[7m", 4);

    char perc_buf[32] = {0};
    if (state.n_curr_row == 0) sprintf(perc_buf, "%s", "Top");
    else if (state.n_curr_row == state.n_rows-1) sprintf(perc_buf, "%s", "Bottom");
    else sprintf(perc_buf, "%d%%", curr_row_percentile());

    char status[128] = {0};
    int len = snprintf(status, sizeof(status), " %s - (%d, %d) - %d lines (%s) - [page %d/%d]", 
            state.file_name == NULL ? "unnamed" : state.file_name, 
            state.cx+1, 
            state.n_curr_row+1, 
            state.n_rows, 
            perc_buf,
            state.n_curr_page+1, 
            state.n_pages
    ); // TODO: percentuale
    if (len > state.n_cols) len = state.n_cols;
    sb_append_buf(&screen_buf, status, len);
    while (len < state.n_cols) {
        sb_append_buf(&screen_buf, " ", 1);
        len++;
    }
    sb_append_buf(&screen_buf, "\x1b[m", 3);
}

void draw_screen()
{
    sb_append_buf(&screen_buf, "\x1b[?25l", 6);
    write(STDOUT_FILENO, "\x1B[H", 3);

    if (state.n_rows == 0) draw_welcome(); // n_rows e' file_buf.count
    draw_rows();
    draw_message_bar();
    draw_status_bar();

    char cursor_buf[32] = {0};
    snprintf(cursor_buf, sizeof(cursor_buf), "\x1b[%d;%dH", state.cy + 1, state.cx + 1);
    sb_append_buf(&screen_buf, cursor_buf, strlen(cursor_buf));

    //sb_append_buf(&screen_buf, "\x1B[H", 3); // TODO: forse non ce n'e' bisogno
    sb_append_buf(&screen_buf, "\x1b[?25h", 6);
    sb_append_null(&screen_buf);
    write(STDOUT_FILENO, screen_buf.items, screen_buf.count);
    screen_buf.count = 0;
}

void quit()
{
    if (state.dirty == 0 || quit_times == 0) {
        write(STDOUT_FILENO, "\x1B[H", 3);
        write(STDOUT_FILENO, "\x1b[2J", 4);
        exit(0);
    } else {
        char buf[128] = {0};
        sprintf(buf, "Session is not saved. If you really want to quit press CTRL-q %d more time%s.", quit_times, quit_times == 1 ? "" : "s");
        set_message(buf);
        quit_times--;
    }
}
void move_cursor_up() 
{
    if (state.cy == 0 && state.n_curr_page != 0) state.offset--; // TODO: gestisci meglio
    if (state.cy > N_OR_DEFAULT_VALUE(0)) state.cy -= N_OR_DEFAULT_VALUE(1);
    else state.cy = 0;
    if (state.n_curr_row > N_OR_DEFAULT_VALUE(0)) state.n_curr_row -= N_OR_DEFAULT_VALUE(1);
    else state.n_curr_row = 0;
    state.needs_redraw = true;
}
void move_cursor_down()
{ 
    //if ((state.n_curr_row+N_OR_DEFAULT_VALUE(1) / state.n_pages) > state.n_curr_page) state.n_curr_page += (state.n_curr_row+N_OR_DEFAULT_VALUE(1)) / state.n_pages; // TODO: No, non funziona cosi' devo modificare l'offset in questo caso
    if (state.cy == state.n_screenrows-1 && state.n_curr_page != state.n_pages-1) state.offset++; // TODO: gestisci meglio
    if (state.cy < state.n_screenrows-N_OR_DEFAULT_VALUE(0)-1) state.cy += N_OR_DEFAULT_VALUE(1); 
    else state.cy = state.n_screenrows-1;
    if (state.n_curr_row < state.n_rows-N_OR_DEFAULT_VALUE(0)-1) state.n_curr_row += N_OR_DEFAULT_VALUE(1); 
    else state.n_curr_row = state.n_rows-1;
    state.needs_redraw = true;
}
void move_cursor_left()  { if (state.cx > N_OR_DEFAULT_VALUE(0))                      state.cx -= N_OR_DEFAULT_VALUE(1); else state.cx = 0;}
void move_cursor_right() { if (state.cx < state.n_cols-N_OR_DEFAULT_VALUE(0)-1)       state.cx += N_OR_DEFAULT_VALUE(1); else state.cx = state.n_cols-1;}

void scroll_up() // TODO: aggiornare current row e cy
{ 
    if (state.n_curr_row > N_OR_DEFAULT_VALUE(0)) state.offset -= N_OR_DEFAULT_VALUE(1);
}                

void scroll_down() // TODO: aggiornare current row e cy
{
    if (state.n_curr_row < state.n_rows-N_OR_DEFAULT_VALUE(0)-1) state.offset += N_OR_DEFAULT_VALUE(1);
}

void move_page_up()
{
    if (state.n_curr_page > N_OR_DEFAULT_VALUE(0)) state.n_curr_page -= N_OR_DEFAULT_VALUE(1);
    else state.n_curr_page = 0;
    state.n_curr_row = state.n_curr_page*state.n_pages + state.cy;
    state.needs_redraw = true;
}             
void move_page_down()
{
    if (state.n_curr_page < state.n_pages-N_OR_DEFAULT_VALUE(0)-1) state.n_curr_page += N_OR_DEFAULT_VALUE(1);
    else state.n_curr_page = state.n_pages-1;
    state.n_curr_row = state.n_curr_page*state.n_pages + state.cy;
    state.needs_redraw = true;
}           

void move_cursor_begin_of_screen()
{
    set_message("TODO: move_cursor_begin_of_screen");
}        

void move_cursor_end_of_screen() 
{
    set_message("TODO: move_cursor_end_of_screen");
}

void move_cursor_begin_of_file()
{
    state.n_curr_page = 0;
    state.n_curr_row = 0;
    state.cy = 0;
    state.needs_redraw = true;
}        
void move_cursor_end_of_file() 
{
    state.n_curr_page = state.n_pages-1;
    state.n_curr_row = state.n_rows-1;
    state.cy = state.n_screenrows-1;
    state.needs_redraw = true;
}

void move_cursor_begin_of_line() { state.cx = 0; }
void move_cursor_end_of_line()   { state.cx = state.n_cols-1; }  

void move_cursor_first_non_space()
{
    String_Builder row = BUF_ROW(state.n_curr_row);
    char *str = row.items;
    int i = 0;
    int len = strlen(str);
    while (i < len && isspace(str[i])) {
        i++;
    }
    if (i == 0) state.cx = 0;
    else state.cx = i;
}

void move_cursor_last_non_space()
{
    String_Builder row = BUF_ROW(state.n_curr_row);
    char *str = row.items;
    int len = strlen(str);
    int i = len - 1;
    while (i >= 0 && isspace(str[i]))
        i--;
    if (i == len - 1) state.cx = len;
    else state.cx = i;
}

char *key_to_name(char key)
{
    if (key == 27)  return "ALT?";
    if (key == 32)  return "` `";
    if (key == 127) return "DEL";
    if (key == 16)  return "shift";

    char buf[16];
    if (isgraph((unsigned char) key)) sprintf(buf, "%c", key);
    else sprintf(buf, "%d", key);
    return strdup(buf);
}

void itoa(int n, char *buf)
{
    if (n == 0) {
        buf[0] = '0';
        return;
    }
    char tmp[64] = {0};
    int i = 0;
    while (n > 0) {
        tmp[i] = n % 10 + '0';
        i++;
        n /= 10;
    }
    int len = strlen(tmp);
    for (int i = 0; i < len; i++) buf[i] = tmp[len-i-1];
}

void process_key(char key) {
    if (isdigit(key)) {
        if (state.N == N_DEFAULT) {
            if (key == '0') {
                state.N = N_DEFAULT;
                return;
            } else state.N = key - '0';
        } else {
            state.N *= 10;
            state.N += key - '0';
        }
        char bufN[64] = {0};
        itoa(state.N, bufN);
        char msg[64];
        sprintf(msg, "number: %s", bufN);
        set_message(msg);
    } else {
        char key_name[32];
        sprintf(key_name, "key: %s", key_to_name(key));
        set_message(key_name);

        switch (key)
        {
            case CTRL_KEY('q'): quit();                        break;

            case 'k': move_cursor_up();                        break;
            case 'K': move_page_up();                          break;
            //case CTRL_KEY('k'): move_cursor_begin_of_screen(); break;
            case CTRL_KEY('K'): move_cursor_begin_of_file();   break; // TODO: capire come distingueree ctrl-k e ctrl-K (probabilmente controllando se e' premuto anche shift e poi switchare)

            case 'j': move_cursor_down();                      break;
            case 'J': move_page_down();                        break;
            //case CTRL_KEY('j'): move_cursor_end_of_screen();   break;
            case CTRL_KEY('J'): move_cursor_end_of_file();     break;

            case 'h': move_cursor_left();                      break;
            case 'H': move_cursor_first_non_space();           break;
            case CTRL_KEY('h'): move_cursor_begin_of_line();   break;

            case 'l': move_cursor_right();                     break;
            case 'L': move_cursor_last_non_space();            break;
            case CTRL_KEY('L'): move_cursor_end_of_line();     break;

            case 'p': scroll_up();                             break;
            case 'n': scroll_down();                           break;


            default:
                ;                        ;
        }
        state.N = N_DEFAULT;
        if (key != CTRL_KEY('q')) quit_times = QUIT_DEFAULT;
    }
}

int main(int argc, char **argv)
{
    char *filename = NULL;
    if (argc == 2) filename = argv[1];

    init_terminal();
    EditorState_init(&state);
    open_file(filename);
    set_message("");

    char key;
    while (true) {
        //if (state.needs_redraw) {
            draw_screen();
        //}
        key = read_key();
        process_key(key);

        if (key == '\0') set_message("nothing");
    }

    sb_free(screen_buf);
    return 0;
}

//#define INPUT_SIZE 8
//int main2(void)
//{
//    init_terminal();
//
//    char input[INPUT_SIZE];
//    while (true) {
//        memset(input, 0, INPUT_SIZE);
//        read(STDIN_FILENO, input, INPUT_SIZE);
//        if (feof(stdin)) break;
//        if (strcmp(input, "q") == 0) break;
//
//        int input_len = strlen(input);
//        if (input_len == 1) {
//            unsigned char key = input[0];
//            if (isprint(key)) {
//                da_append(&input_line, key);
//            } else if (key == 127) {
//                if (input_line.count > 0) input_line.count--;
//            } else {
//                print_char(key);
//                printf("\n");
//            }
//            // print if charachter has been added
//            if (input_line.count >= 0) {
//                printf("\x1B[F\x1B[2K");
//                InputLine_print(input_line);
//            }
//        } else {
//            printf("Multiple input: ");
//            for (int i = 0; i < input_len; i++) {
//                printf("%d ", input[i]);
//            }
//            printf("-> ");
//            for (int i = 0; i < input_len; i++) {
//                print_char(input[i]);
//            }
//            printf("\n");
//        }
//    }
//
//    printf("Quit\n");
//    da_free(input_line);
//    reset_terminal();
//    return 0;
//}
