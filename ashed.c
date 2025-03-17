/* TODO
   - swap files

   - Line addresses command (da documentare):
     - . (current line, default for everything)
     - $ (last line)
     - n (n-th line)
     - +(n)
     - -(n)
     - , (first through last lines = 1,)
     - ; (current through last lines)
     - x,y (range)
       > x, (or x, shortened or x;) and ,y will be valid ranges

   - Comandi:
     - a/. (oppure i?)
     - h
     - w
     - q!

   Urgent Stack:
   - documentare tutto [address | .]cmd

    https://www.gnu.org/software/ed/manual/ed_manual.html
    https://www.redhat.com/en/blog/introduction-ed-editor
*/

#include "ashed.h"

// VARIABLES /////
int current_line = 0;
char filename[256] = {0};
bool saved = true;
Buffer ashed_buffer = {0};
//////////////////

void buffer_init(Buffer *buf)
{
    buf->count = 0;
    buf->capacity = BUFFER_INITIAL_CAPACITY;
    buf->items = malloc(buf->capacity*sizeof(char *));
    for (int i = 0; i < buf->capacity; i++) {
        buf->items[i] = NULL;
    }
}

void buffer_write_line(Buffer *buf, char *line_str, int line_n)
{
    assert(line_n >= 0 && line_n < buf->count+1);
    if (line_n < buf->count) {
        if (buf->items[line_n] != NULL) {
            free(buf->items[line_n]); 
        }
    } else if (buf->count == buf->capacity) {
        buf->items = realloc(buf->items, 2*buf->capacity*sizeof(char *));
        assert(buf->items != NULL && "Buy more RAM, lol.");
        buf->capacity *= 2;
        for (int i = buf->count; i < buf->capacity; i++)
            buf->items[i] = NULL;
        buf->count++;
    } else {
        buf->count++;
    }
    buf->items[line_n] = strdup(line_str);
}

void buffer_print_line(Buffer buf, int line_n)
{
    if (line_n >= 0 && line_n < buf.count)
    {
        if (buf.items[line_n] != NULL) {
            printf("%s", buf.items[line_n]);
            if (!streq(buf.items[line_n], "\n")) printf("\n");
        }
    } else shlog(SHLOG_FATAL, "Can not print line %d", line_n);
}

void buffer_print_range(Buffer buf, Range r)
{
    if (r.begin >= 0 && r.end < buf.count) {
        int width = log10(r.end)+1;
        printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
        for (int i = r.begin; i <= r.end; i++) {
            printf("%*d: ", width, i+1);
            buffer_print_line(buf, i);
        }
        printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    } else shlog(SHLOG_FATAL, "Can not print range (%d, %d)", r.begin, r.end);
}

void buffer_print(Buffer buf)
{
    //if (buf.count > 0) {
    //    int width = log10(buf.count)+1;
    //    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    //    for (int i = 0; i < buf.count; i++)
    //        printf("%*d: %s\n", width, i+1, buf.items[i]);
    //    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    //}
    buffer_print_range(buf, (Range){0, buf.count});
}

bool buffer_init_from_file(Buffer *buf, char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        shlog(SHLOG_ERROR, "Could not open file `%s`\n", filename);
        return false;
    }
    buffer_init(buf);

    char line_buf[MAX_CHARS];
    char *line = NULL;
    int i = 0;
    while ((line = fgets(line_buf, MAX_CHARS, f)) != NULL) {
        if (remove_newline(&line)) {
            buffer_write_line(buf, line, i);
            i++;
        }
    }
    return true;
}

void buffer_free(Buffer *buffer)
{
    for (int i = 0; i < buffer->count; i++) {
        if (buffer->items[i] != NULL) free(buffer->items[i]);
    }
    free(buffer->items);
}

bool write_entire_file(char *path, const void *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        shlog(SHLOG_ERROR, "Could not open file `%s` for writing: %s\n", path, strerror(errno));
        return false; 
    }

    const char *buf = data;
    while (size > 0) {
        size_t n = fwrite(buf, 1, size, f);
        if (ferror(f)) {
            shlog(SHLOG_ERROR, "Could not write into file %s: %s\n", path, strerror(errno));
            if (f) fclose(f);
            return true;
        }
        size -= n;
        buf  += n;
    }

    if (f) fclose(f);
    return true;
}

int count_num_len(char *str)
{
    int count = 0;
    while (isdigit(*str)) {
        count++;
        str++;
    }
    return count;
}

int parse_num_and_advance(char **str)
{
    if (!isdigit(**str)) return -1;
    int len = count_num_len(*str);
    char *tmp = strdup(*str);
    tmp[len] = '\0';
    int n = matoi(tmp);
    free(tmp);
    for (int i = 0; i < len; i++)
        (*str)++;
    return n;
}

// TODO:
// - possibilita' di fare ++ e avanzare di 2 (e cosi' via, anche con il -)
AshedAddress parseAddress(char **line)
{
    AshedAddress addr = {0};
    if (**line == '.') {
        addr.type = ASHED_ADDR_CURRENT;
        (*line)++;
    } else if (**line == '$') {
        addr.type = ASHED_ADDR_LAST;
        (*line)++;
    } else if (**line == ',') {
        addr.type = ASHED_ADDR_RANGE;
        (*line)++;
        addr.r = (Range){
            .begin = 0,
            .end = isdigit(**line) ? parse_num_and_advance(line)-1 : ashed_buffer.count-1
        };
    } else if (**line == ';') {
        addr.type = ASHED_ADDR_RANGE;
        addr.r = (Range){.begin=current_line, .end=ashed_buffer.count-1};
        (*line)++;
    } else if (**line == '+') {
        addr.type = ASHED_ADDR_NEXT; 
        (*line)++;
        if (isdigit(**line)) {
            addr.n = parse_num_and_advance(line); 
        } else addr.n = 1;
        if (current_line + addr.n > ashed_buffer.count-1) addr.type = ASHED_ADDR_INVALID;
    } else if (**line == '-') {
        addr.type = ASHED_ADDR_PREV; 
        (*line)++;
        if (isdigit(**line)) {
            addr.n = parse_num_and_advance(line); 
        } else addr.n = 1;
        if (current_line - addr.n < 0) addr.type = ASHED_ADDR_INVALID;
    } else if (isdigit(**line)) {
        int first = parse_num_and_advance(line);
        if (**line == ',') {
            (*line)++;
            if (isdigit(**line)) {
                int second = parse_num_and_advance(line);
                if (first >= second) addr.type = ASHED_ADDR_INVALID;
                else {
                    addr.type = ASHED_ADDR_RANGE;
                    addr.r = (Range){.begin=first-1, .end=second-1};
                }
            } else {
                addr.type= ASHED_ADDR_RANGE;
                addr.r = (Range){.begin=first-1, .end=ashed_buffer.count-1};
            }
        } else if (**line == ';') {
            addr.type= ASHED_ADDR_RANGE;
            addr.r = (Range){.begin=first-1, .end=ashed_buffer.count-1};
            (*line)++;
        } else { 
            if (first >= 1 && first <= ashed_buffer.count) {
                addr.type= ASHED_ADDR_NUMBER;
                addr.n = first;
            } else addr.type = ASHED_ADDR_INVALID; // TODO: mettere un errore del tipo "out of range"
        }
    }

    return addr;
}

void ashedAddress_print(AshedAddress addr)
{
    printf("Address: ");
    switch (addr.type)
    {
        case ASHED_ADDR_INVALID: printf("INVALID");                                  break;
        case ASHED_ADDR_CURRENT: printf("CURRENT (%d)", current_line);               break;
        case ASHED_ADDR_LAST:    printf("LAST");                                     break;
        case ASHED_ADDR_NUMBER:  printf("NUMBER (%d)", addr.n);                      break;
        case ASHED_ADDR_NEXT:    printf("NEXT (+%d)", addr.n);                       break;
        case ASHED_ADDR_PREV:    printf("PREV (-%d)", addr.n);                       break;
        case ASHED_ADDR_RANGE:   printf("RANGE (%d, %d)", addr.r.begin, addr.r.end); break;
        default:                 shlog(SHLOG_FATAL, "Unreachable");                  abort();
    }
    printf("\n");
}

void ashed_quit(int *code, char *line)
{
    if (line == NULL || streq(line, "q")) {
        if (!saved) {
            printf("unsaved session\n");
            *code = 0;
        } else {
            *code = -1;
        }
    } else if (streq(line, "q!")) *code = -1;
    else *code = 1;
}

void ashed_clear(int *code) { printf("\e[1;1H\e[2J"); *code = 0; }

void ashed_print(int *code, AshedAddress addr)
{
    int line_to_print;
    bool print_single_line = true;
    switch(addr.type)
    {
        case ASHED_ADDR_CURRENT: line_to_print = current_line;              break;
        case ASHED_ADDR_LAST:    line_to_print = ashed_buffer.count-1;      break;
        case ASHED_ADDR_NUMBER:  line_to_print = addr.n - 1;                break;
        case ASHED_ADDR_NEXT:    line_to_print = current_line + addr.n;     break; 
        case ASHED_ADDR_PREV:    line_to_print = current_line - addr.n;     break; 

        case ASHED_ADDR_RANGE:   print_single_line = false;                 break;
        case ASHED_ADDR_INVALID:
        default:                 shlog(SHLOG_FATAL, "Unreachable");                 abort();
    }

    if (print_single_line) {
        buffer_print_line(ashed_buffer, line_to_print);
    } else {
        buffer_print_range(ashed_buffer, addr.r);
    }
    
    RETURN_CODE(0);
}

void ashed_goto_input_mode(int *code, char *line, AshedMode *mode)
{
    int len = strlen(line);
    if (len == 1) {
        *mode = ASHED_MODE_INPUT;
        RETURN_CODE(0);
    }

    RETURN_CODE(1);
}

void ashed_goto_command_mode(int *code, AshedMode *mode)
{
    *mode = ASHED_MODE_COMMAND;
    *code = 0; 
}

void ashed_write_line(int *code, char *line)
{
    buffer_write_line(&ashed_buffer, line, current_line);
    current_line++;
    saved = false;
    *code = 0;
}

void ashed_write_file(int *code, char *line)
{
    bool save_and_quit = false;
    bool save_on_different_file = false;
    if (streq(line, "w")) {
        // TODO: non succede niente in questo caso, riorganizza l'if
    } else if (streq(line, "wq")) {
        save_and_quit = true;
    } else if (strlen(line) > 1 && (line[1] == ' ' || line[1] == '\t')) {
        line++;
        line = remove_spaces(line);
        sprintf(filename, line);
        save_on_different_file = true;
    } else RETURN_CODE(1);

    char blob[ashed_buffer.count*(MAX_CHARS+1)];
    char *begin = blob;
    size_t total_len = 0;
    for (int i = 0; i < ashed_buffer.count; i++) {
        char *line = ashed_buffer.items[i];
        if (line != NULL) {
            int len = strlen(line);
            memcpy(begin, line, strlen(line)*sizeof(char));
            begin += len+1;
            total_len += len+1;
            blob[total_len-1] = '\n';
        }
    }
    if (EDDEBUG) shlog(SHLOG_DEBUG, "Blob: %s", blob);
    size_t write_size = total_len*sizeof(char);
    printf("%zu\n", write_size);
    if (!write_entire_file(filename, blob, write_size)) RETURN_CODE(2);
    if (!save_on_different_file) saved = true;
    if (save_and_quit) ashed_quit(code, NULL);
    else RETURN_CODE(0);
}

void ashed_goto_line(int *code, AshedAddress addr)
{
    switch(addr.type)
    {
        case ASHED_ADDR_CURRENT:                                                    break;
        case ASHED_ADDR_LAST:    current_line = ashed_buffer.count-1;               break;
        case ASHED_ADDR_NUMBER:  current_line =  addr.n - 1;                        break;
        case ASHED_ADDR_NEXT:    current_line += addr.n - 1;                        break; 
        case ASHED_ADDR_PREV:    current_line -= addr.n - 1;                        break; 
        case ASHED_ADDR_RANGE:   shlog(SHLOG_WARNING, "Range alone has no effect"); break;
        case ASHED_ADDR_INVALID:
        default:                 shlog(SHLOG_FATAL, "Unreachable");                 abort();
    }
    RETURN_CODE(0);
}

int ashed_main(char *ashell_filename)
{
    static int ashed_file_n = 0;

    if (ashell_filename == NULL) {
        sprintf(filename, "temped%d", ashed_file_n++);
        while (does_file_exist(filename))
            sprintf(filename, "temped%d", ashed_file_n++);
        buffer_init(&ashed_buffer);
    } else {
        sprintf(filename, ashell_filename);
        if (does_file_exist(filename)) {
            if (!buffer_init_from_file(&ashed_buffer, filename)) return 1;
        } else buffer_init(&ashed_buffer);
    }

    printf("ashed - %s\n\n", filename);
    AshedMode mode = ASHED_MODE_COMMAND;
    int ashed_code = 0;
    char input_buffer[MAX_CHARS];
    char *line = NULL;

    while (true) {
        if (mode == ASHED_MODE_COMMAND) printf("C%d: ", current_line + 1);
        else printf("I%d: ", current_line + 1);
        line = fgets(input_buffer, MAX_CHARS, stdin);
        if (line == NULL) {
            shlog(SHLOG_ERROR, "Could not read line\n");
            continue;
        }
        if (!remove_newline(&line)) continue;

        if (mode == ASHED_MODE_COMMAND) {
            line = remove_spaces(line);
            AshedAddress address = parseAddress(&line);

            ashedAddress_print(address);
            if (EDDEBUG) {
                printf("now parse command from `%s`\n", line);
            }

            if (address.type == ASHED_ADDR_INVALID) {
                ashed_code = 1;
            } else {
                if      (streq(line, "c")) ashed_clear(&ashed_code);
                else if (streq(line, "p")) ashed_print(&ashed_code, address);
                else if (streq(line, "i")) ashed_goto_input_mode(&ashed_code, &mode);
                else if (line[0] == 'w')   ashed_write_file(&ashed_code, line);
                else if (line[0] == 'q')   ashed_quit(&ashed_code, line);
                else if (streq(line, ""))  ashed_goto_line(&ashed_code, address);
                else                       ashed_code = 1;
            }

            if      (ashed_code == -1) break;
            else if (ashed_code == 1)  printf("huh?\n");
            else if (ashed_code != 0)  shlog(SHLOG_FATAL, "Unknown shed exit code %d\n", ashed_code);
        } else if (mode == ASHED_MODE_INPUT) {
            if (streq(line, ".")) ashed_goto_command_mode(&ashed_code, &mode);
            else                  ashed_write_line(&ashed_code, line);
        } else {
            shlog(SHLOG_ERROR, "Unknown shed mode (%d)\n", mode);
            buffer_free(&ashed_buffer);
            abort();
            return 1;
        }
    }
    buffer_free(&ashed_buffer);
    return 0;
}
