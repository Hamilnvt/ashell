/* TODO
   - swap files
   - major and minor modes (differenziate da lettere maiuscole e minuscole)
   - autosave (every n commands [newlines] inserted)
   - colori per ogni mode

   - Comandi:
     - i (data la riga dell'address, la sposta giu' di 1 e comincia a scrivere in quella riga)
       > servirebbe poterlo usare con gli addresses cosi' se vuoi inserire a linea 5 fai 5i oppure +i etc..., ovviamente no range
     - h
     - u (undo, dovrebbe essere molto difficile e forse devo anche enumerare i vari comandi, che comunque non sarebbe una cattiva idea)
     - m (move -> <addr>m<addr>)
     - x (mark, per poi tornarci, magari mettendo anche un identificativo)
     - F/f (find)

   Urgent Stack:
   - problema con remove_newline, non aumenta la capacita' se trova una riga vuota (con solo '\n')

    https://www.gnu.org/software/ed/manual/ed_manual.html
    https://www.redhat.com/en/blog/introduction-ed-editor
*/

#include "ashed.h"

// VARIABLES /////
int current_line = 0;
char filename[256] = {0};
bool saved = true;
Buffer ashed_buffer = {0};
AshedAddress *saved_addr = NULL;
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

void buffer_double_capacity(Buffer *buf)
{
    buf->items = realloc(buf->items, 2*buf->capacity*sizeof(char *));
    assert(buf->items != NULL && "Buy more RAM, lol.");
    buf->capacity *= 2;
    shlog(SHLOG_DEBUG, "New capacity: %d\n", buf->capacity);
    for (int i = buf->count; i < buf->capacity; i++)
        buf->items[i] = NULL;
}

void buffer_write_line(Buffer *buf, char *line_str, int line_n)
{
    assert(line_n >= 0 && line_n < buf->count+1);
    if (line_n < buf->count) {
        if (buf->items[line_n] != NULL)
            free(buf->items[line_n]); 
    } else {
        if (buf->count == buf->capacity) buffer_double_capacity(buf);
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
    buffer_print_range(buf, (Range){0, buf.count-1});
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
    fclose(f);
    return true;
}

void buffer_free(Buffer *buffer)
{
    for (int i = 0; i < buffer->count; i++) {
        if (buffer->items[i] != NULL) free(buffer->items[i]);
    }
    free(buffer->items);
}

void buffer_shift_right(Buffer *buf, int line_n)
{
    if (line_n >= 0 && line_n < buf->count) {
        buf->count++;
        shlog(SHLOG_DEBUG, "%d/%d", buf->count, buf->capacity);
        if (buf->count == buf->capacity)
            buffer_double_capacity(buf);
        char *tmp = NULL;
        for (int i = buf->count-1; i > line_n; i--) {
            tmp = strdup(buf->items[i-1]);
            free(buf->items[i-1]);
            buf->items[i-1] = NULL;
            buf->items[i] = strdup(tmp);
            free(tmp);
        }
    } else shlog(SHLOG_FATAL, "Can not shift right %d", line_n);
}

// TODO: possibilita' farlo con un numero arbitrario (utile quando si elimina un range di righe)
void buffer_shift_left(Buffer *buf, int line_n)
{
    if (line_n >= 0 && line_n < buf->count) {
        for (int i = line_n; i < buf->count; i++) {
            free(buf->items[i]);
            buf->items[i] = NULL;
            if (i != buf->count-1)
                buf->items[i] = strdup(buf->items[i+1]);
        }
        buf->count--;
    } else shlog(SHLOG_FATAL, "Can not shift left %d", line_n);
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
// - funziona, ma c'e' troppo codice ripetuto aaaa, ma adesso non ho voglia di sistemarlo
AshedAddress parseAddress(char **line)
{
    AshedAddress addr = {0};
    if (**line == '.') {
        addr.type = ASHED_ADDR_CURRENT;
        (*line)++;
        if (**line == ',') {
            addr.type= ASHED_ADDR_RANGE;
            (*line)++;
            if (**line == '.') addr.type = ASHED_ADDR_INVALID;
            else if (**line == '$') {
                addr.r = (Range){.begin=current_line, .end=ashed_buffer.count-1};
                (*line)++;
            } else if (isdigit(**line)) {
                int second = parse_num_and_advance(line);
                if (second-1 == current_line) addr.type = ASHED_ADDR_INVALID;
                else addr.r = (Range){.begin=current_line, .end=second-1};
            } else if (current_line == ashed_buffer.count-1) addr.type = ASHED_ADDR_INVALID;
            else addr.r = (Range){.begin=current_line, .end=ashed_buffer.count-1};
        }
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
        } else if (**line == '+') {
            int count = 2;
            (*line)++;
            while (**line == '+') {
                (*line)++;
                count++;
            }
            printf("%d +\n", count);
            addr.n = count;
        } else addr.n = 1;
        if (current_line + addr.n >= ashed_buffer.count) addr.type = ASHED_ADDR_INVALID;
    } else if (**line == '-') {
        addr.type = ASHED_ADDR_PREV; 
        (*line)++;
        if (isdigit(**line)) {
            addr.n = parse_num_and_advance(line); 
        } else if (**line == '-') { 
            int count = 2;
            (*line)++;
            while (**line == '-') {
                (*line)++;
                count++;
            }
            addr.n = count;
        } else addr.n = 1;
        if (current_line - addr.n < 0) addr.type = ASHED_ADDR_INVALID;
    } else if (isdigit(**line)) {
        int first = parse_num_and_advance(line);
        if (**line == ',') {
            addr.type= ASHED_ADDR_RANGE;
            (*line)++;
            if (**line == '.') {
                addr.r = (Range){.begin=first-1, .end=current_line};
                (*line)++;
            } else if (**line == '$') {
                addr.r = (Range){.begin=first-1, .end=ashed_buffer.count-1};
                (*line)++;
            } else if (isdigit(**line)) {
                int second = parse_num_and_advance(line);
                if (first >= second) addr.type = ASHED_ADDR_INVALID;
                else addr.r = (Range){.begin=first-1, .end=second-1};
            } else addr.r = (Range){.begin=first-1, .end=ashed_buffer.count-1};
        } else { 
            if (first >= 1 && first <= ashed_buffer.count) {
                addr.type= ASHED_ADDR_NUMBER;
                addr.n = first;
            } else addr.type = ASHED_ADDR_INVALID;
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
        default:                 shlog(SHLOG_FATAL, "Unreachable: address print");   abort();
    }
    printf("\n");
}

static_assert(ASHED_CMDTYPES_COUNT == 14, "Exhaustive number of ashed commands in getAshedCmdType");
AshedCmdType getAshedCmdType(char **line)
{
    AshedCmdType type;
    bool need_to_advance = true;
    if      (streq(*line, "c")) type = ASHED_CMD_CLEAR;
    else if (streq(*line, "p")) type = ASHED_CMD_PRINT;
    else if (streq(*line, "A")) type = ASHED_CMD_APPEND_MAJOR;
    else if (streq(*line, "a")) type = ASHED_CMD_APPEND_MINOR;
    else if (streq(*line, "I")) type = ASHED_CMD_INSERT_MAJOR;
    else if (streq(*line, "i")) type = ASHED_CMD_INSERT_MINOR;
    else if (streq(*line, "R")) type = ASHED_CMD_REPLACE_MAJOR;
    else if (streq(*line, "r")) type = ASHED_CMD_REPLACE_MINOR;
    else if (streq(*line, "d")) type = ASHED_CMD_DELETE;
    else if (*line[0] == 'h')   type = ASHED_CMD_HELP;
    else if (*line[0] == 'w')   type = ASHED_CMD_WRITE;
    else if (*line[0] == 'q')   type = ASHED_CMD_QUIT;
    else if (streq(*line, ""))  { type = ASHED_CMD_GOTO; need_to_advance = false; }
    else                        type = ASHED_CMD_UNKNOWN;

    if (need_to_advance) (*line)++;
    return type;
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
        default:                 shlog(SHLOG_FATAL, "Unreachable: ashed print"); abort();
    }

    if (print_single_line) {
        buffer_print_line(ashed_buffer, line_to_print);
    } else {
        buffer_print_range(ashed_buffer, addr.r);
    }
    
    RETURN_CODE(0);
}

void ashed_goto_append_mode(int *code, AshedMode *mode) 
{
    *mode = ASHED_MODE_APPEND;
    current_line = ashed_buffer.count;
    ashed_buffer.count++;
    RETURN_CODE(0);
}

void ashed_goto_insert_mode(int *code, AshedMode *mode, AshedAddress addr) 
{
    *mode = ASHED_MODE_INSERT;
    saved_addr = malloc(sizeof(AshedAddress));
    *saved_addr = addr;
    RETURN_CODE(0);
}

void ashed_goto_replace_mode(int *code, AshedMode *mode, AshedAddress addr) 
{
    *mode = ASHED_MODE_REPLACE;
    saved_addr = malloc(sizeof(AshedAddress));
    *saved_addr = addr;
    RETURN_CODE(0);
}

void ashed_goto_command_mode(int *code, AshedMode *mode) { *mode = ASHED_MODE_COMMAND; RETURN_CODE(0); }

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
        case ASHED_ADDR_NEXT:    current_line += addr.n;                            break; // TODO: e' giusto?
        case ASHED_ADDR_PREV:    current_line -= addr.n;                            break; 
        case ASHED_ADDR_RANGE:   shlog(SHLOG_WARNING, "Range alone has no effect"); break;
        case ASHED_ADDR_INVALID:
        default:                 shlog(SHLOG_FATAL, "Unreachable: goto line");      abort();
    }
    if (addr.type != ASHED_ADDR_RANGE)
        buffer_print_line(ashed_buffer, current_line);
    RETURN_CODE(0);
}

void ashed_insert_line(int *code, char *line_str)
{
    int line_n;
    switch(saved_addr->type)
    {
        case ASHED_ADDR_CURRENT: line_n = current_line;                          break;
        case ASHED_ADDR_LAST:    line_n = ashed_buffer.count-1;                  break;
        case ASHED_ADDR_NUMBER:  line_n = saved_addr->n - 1;                     break;
        case ASHED_ADDR_NEXT:    line_n = current_line + saved_addr->n;          break; 
        case ASHED_ADDR_PREV:    line_n = current_line - saved_addr->n;          break; 
        case ASHED_ADDR_RANGE:   { shlog(SHLOG_WARNING, "Cannot insert a range"); RETURN_CODE(1); }
        case ASHED_ADDR_INVALID:
        default:                 shlog(SHLOG_FATAL, "Unreachable: insert line"); abort();
    }
    buffer_shift_right(&ashed_buffer, line_n);
    buffer_write_line(&ashed_buffer, line_str, line_n);
    saved = false;

    saved_addr->type = ASHED_ADDR_NEXT;
    saved_addr->n = 1;
    current_line++;
    RETURN_CODE(0);
}

// TODO: replace range cancella tutte le righe del range e ne riscrive solamente una
void ashed_replace_line(int *code, char *line_str)
{
    int line_n;
    switch(saved_addr->type)
    {
        case ASHED_ADDR_CURRENT: line_n = current_line;                          break;
        case ASHED_ADDR_LAST:    line_n = ashed_buffer.count-1;                  break;
        case ASHED_ADDR_NUMBER:  line_n = saved_addr->n - 1;                     break;
        case ASHED_ADDR_NEXT:    line_n = current_line + saved_addr->n;          break; 
        case ASHED_ADDR_PREV:    line_n = current_line - saved_addr->n;          break; 
        case ASHED_ADDR_RANGE:   { shlog(SHLOG_WARNING, "Cannot replace a range"); RETURN_CODE(1); } // TODO: in realta' si puo'
        case ASHED_ADDR_INVALID:
        default:                 shlog(SHLOG_FATAL, "Unreachable: replace line"); abort();
    }
    buffer_write_line(&ashed_buffer, line_str, line_n);
    saved = false;

    free(saved_addr);
    saved_addr = NULL;
    RETURN_CODE(0);
}

void ashed_delete(int *code, AshedAddress addr)
{
    int line_n;
    bool delete_single_line = true;
    switch(addr.type)
    {
        case ASHED_ADDR_CURRENT: line_n = current_line;           break;
        case ASHED_ADDR_LAST:    line_n = ashed_buffer.count-1;   break;
        case ASHED_ADDR_NUMBER:  line_n = addr.n - 1;            break;
        case ASHED_ADDR_NEXT:    line_n = current_line + addr.n; break; 
        case ASHED_ADDR_PREV:    line_n = current_line - addr.n; break; 
        case ASHED_ADDR_RANGE:   delete_single_line = false;      break;
        case ASHED_ADDR_INVALID:
        default:                 shlog(SHLOG_FATAL, "Unreachable: delete line"); abort();
    }
    if (delete_single_line) {
        buffer_shift_left(&ashed_buffer, line_n);
    } else {
        // TODO: si puo' migliorare, v. buffer_shift_left todo
        int len = addr.r.end - addr.r.begin + 1;
        for (int i = 0; i < len; i++) {
            buffer_shift_left(&ashed_buffer, addr.r.begin);
        }
    }
    while (current_line >= ashed_buffer.count)
        current_line--;

    saved = false;
    RETURN_CODE(0);
}

void ashed_print_mode_prompt(char *prompt)
{
    printf("%s%d%s: ", prompt, current_line + 1, saved ? "" : "*");
}

void ashed_help(int *code, char *line)
{
    if (streq(line, "h")) {
        printf(ASHED_USAGE);
        RETURN_CODE(0);
    } else if (0) {
             
    } else RETURN_CODE(1);
}

int ashed_main(char *ashell_filename)
{
    static int ashed_file_n = 0;

    bool new_file = true;
    if (ashell_filename == NULL) {
        sprintf(filename, "temped%d", ashed_file_n++);
        while (does_file_exist(filename))
            sprintf(filename, "temped%d", ashed_file_n++);
        buffer_init(&ashed_buffer);
    } else {
        sprintf(filename, ashell_filename);
        if (does_file_exist(filename)) {
            if (!buffer_init_from_file(&ashed_buffer, filename)) return 1; // TODO magari shloggiamo l'errore? non so, dico cosi'
            new_file = false;
        } else buffer_init(&ashed_buffer);
    }

    printf("ashed -> %s%s\n\n", filename, new_file ? "*" : ""); // TODO documentare
    AshedMode ashed_mode = ASHED_MODE_COMMAND;
    int ashed_code = 0;
    char input_buffer[MAX_CHARS];
    char *line = NULL;

    while (true) {
        switch(ashed_mode)
        {
            case ASHED_MODE_COMMAND:  ashed_print_mode_prompt("C"); break;
            case ASHED_MODE_APPEND:   ashed_print_mode_prompt("A"); break;
            case ASHED_MODE_INSERT:   ashed_print_mode_prompt("I"); break;
            case ASHED_MODE_REPLACE:  ashed_print_mode_prompt("R"); break;
            default: shlog(SHLOG_FATAL, "Unknow ashed mode %d", ashed_mode); abort();
        }
        line = fgets(input_buffer, MAX_CHARS, stdin);
        if (line == NULL) {
            shlog(SHLOG_ERROR, "Could not read line\n");
            continue;
        }
        if (!remove_newline(&line)) continue;

        if (ashed_mode == ASHED_MODE_COMMAND) {
            line = remove_spaces(line);
            AshedAddress address = parseAddress(&line);

            ashedAddress_print(address);
            if (EDDEBUG) {
                printf("now parse command from `%s`\n", line);
            }

            if (address.type == ASHED_ADDR_INVALID) {
                ashed_code = 1;
            } else {
                AshedCmdType cmd_type = getAshedCmdType(&line);
                switch (cmd_type)
                {
                    // TODODODOODODODO
                    if      (streq(line, "c")) ashed_clear(&ashed_code);
                    else if (streq(line, "p")) ashed_print(&ashed_code, address);
                    else if (streq(line, "a")) ashed_goto_append_mode(&ashed_code, &ashed_mode);
                    else if (streq(line, "i")) ashed_goto_insert_mode(&ashed_code, &ashed_mode, address);
                    else if (streq(line, "r")) ashed_goto_replace_mode(&ashed_code, &ashed_mode, address);
                    else if (streq(line, "d")) ashed_delete(&ashed_code, address);
                    else if (line[0] == 'h')   ashed_help(&ashed_code, line);
                    else if (line[0] == 'w')   ashed_write_file(&ashed_code, line);
                    else if (line[0] == 'q')   ashed_quit(&ashed_code, line);
                    else if (streq(line, ""))  ashed_goto_line(&ashed_code, address);
                    else                       ashed_code = 1;
                }
            }

            if      (ashed_code == -1) break;
            else if (ashed_code == 1)  printf("huh?\n");
            else if (ashed_code != 0)  shlog(SHLOG_FATAL, "Unknown shed exit code %d\n", ashed_code);
        } else if (ashed_mode == ASHED_MODE_APPEND) {
            if (streq(line, ".")) {
                current_line--;
                ashed_goto_command_mode(&ashed_code, &ashed_mode);
            }
            else                  ashed_write_line(&ashed_code, line);
        } else if (ashed_mode == ASHED_MODE_INSERT) {
            if (saved_addr == NULL) {
                shlog(SHLOG_FATAL, "No saved address found");
                abort();
            }
            if (streq(line, ".")) {
                free(saved_addr);
                saved_addr = NULL;
                ashed_goto_command_mode(&ashed_code, &ashed_mode);
            } else {
                ashed_insert_line(&ashed_code, line);
            }
        } else if (ashed_mode == ASHED_MODE_REPLACE) {
            if (saved_addr == NULL) {
                shlog(SHLOG_FATAL, "No saved address found");
                abort();
            }
            if (streq(line, ".")) {
                free(saved_addr);
                saved_addr = NULL;
            } else {
                ashed_replace_line(&ashed_code, line);
            }
            ashed_goto_command_mode(&ashed_code, &ashed_mode);
        } else {
            shlog(SHLOG_ERROR, "Unknown ashed mode (%d)\n", ashed_mode);
            buffer_free(&ashed_buffer);
            abort();
            return 1;
        }
        printf("Printing buffer (%d/%d):\n", ashed_buffer.count, ashed_buffer.capacity);
        if (EDDEBUG) {
            //buffer_print(ashed_buffer);
        }
    }
    buffer_free(&ashed_buffer);
    return 0;
}
