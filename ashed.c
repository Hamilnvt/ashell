/* TODO
   - swap files
   - major and minor modes (differenziate da lettere maiuscole e minuscole)
   - autosave (every n commands [newlines] inserted)
   - colori per ogni mode

   - Comandi da implementare:
     - u (undo, dovrebbe essere molto difficile e forse devo anche enumerare i vari comandi, che comunque non sarebbe una cattiva idea)
     - m (move -> <addr>m<addr>)
     - x (mark, per poi tornarci, magari mettendo anche un identificativo)
     - F/f (find)

   - Comandi da sistemare:
     - i (data la riga dell'address, la sposta giu' di 1 e comincia a scrivere in quella riga)
       > servirebbe poterlo usare con gli addresses cosi' se vuoi inserire a linea 5 fai 5i oppure +i etc..., ovviamente no range
     - h
       > ogni comando ha la sua stringa help

   Urgent Stack:
   - problema con remove_newline, non aumenta la capacita' se trova una riga vuota (con solo '\n')
   - enum degli ashed exit codes

    https://www.gnu.org/software/ed/manual/ed_manual.html
    https://www.redhat.com/en/blog/introduction-ed-editor
*/

#include "ashed.h"

// VARIABLES /////
int curr_line_n = 0;
char filename[256] = {0};
bool saved = true;
Buffer ashed_buffer = {0};
AshedAddress *saved_addr = NULL;
AshedMode ashed_mode = ASHED_MODE_COMMAND;
int ashed_code = 0;
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
                addr.r = (Range){.begin=curr_line_n, .end=ashed_buffer.count-1};
                (*line)++;
            } else if (isdigit(**line)) {
                int second = parse_num_and_advance(line);
                if (second-1 == curr_line_n) addr.type = ASHED_ADDR_INVALID;
                else addr.r = (Range){.begin=curr_line_n, .end=second-1};
            } else if (curr_line_n == ashed_buffer.count-1) addr.type = ASHED_ADDR_INVALID;
            else addr.r = (Range){.begin=curr_line_n, .end=ashed_buffer.count-1};
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
        addr.r = (Range){.begin=curr_line_n, .end=ashed_buffer.count-1};
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
        if (curr_line_n + addr.n >= ashed_buffer.count) addr.type = ASHED_ADDR_INVALID;
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
        if (curr_line_n - addr.n < 0) addr.type = ASHED_ADDR_INVALID;
    } else if (isdigit(**line)) {
        int first = parse_num_and_advance(line);
        if (**line == ',') {
            addr.type= ASHED_ADDR_RANGE;
            (*line)++;
            if (**line == '.') {
                addr.r = (Range){.begin=first-1, .end=curr_line_n};
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
    shlog_NO_NEWLINE(SHLOG_DEBUG, "Address: ");
    switch (addr.type)
    {
        case ASHED_ADDR_INVALID: printf("INVALID");                                  break;
        case ASHED_ADDR_CURRENT: printf("CURRENT (%d)", curr_line_n);               break;
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
AshedCmdType getAshedCmdType(char *line)
{
    if      (streq(line, "c")) return ASHED_CMD_CLEAR;
    else if (streq(line, "p")) return ASHED_CMD_PRINT;
    else if (streq(line, "A")) return ASHED_CMD_APPEND_MAJOR;
    else if (streq(line, "a")) return ASHED_CMD_APPEND_MINOR;
    else if (streq(line, "I")) return ASHED_CMD_INSERT_MAJOR;
    else if (streq(line, "i")) return ASHED_CMD_INSERT_MINOR;
    else if (streq(line, "R")) return ASHED_CMD_REPLACE_MAJOR;
    else if (streq(line, "r")) return ASHED_CMD_REPLACE_MINOR;
    else if (streq(line, "d")) return ASHED_CMD_DELETE;
    else if (line[0] == 'h')   return ASHED_CMD_HELP;
    else if (line[0] == 'w')   return ASHED_CMD_WRITE;
    else if (line[0] == 'q')   return ASHED_CMD_QUIT;
    else if (streq(line, ""))  return ASHED_CMD_GOTO;
    else                       return ASHED_CMD_UNKNOWN;
}

void ashed_CmdType_print(AshedCmdType type)
{
    shlog_NO_NEWLINE(SHLOG_DEBUG, "Command: ");
    switch (type)
    {
        case ASHED_CMD_UNKNOWN:       printf("UNKNOWN");       break;
        case ASHED_CMD_HELP:          printf("HELP");          break;
        case ASHED_CMD_PRINT:         printf("PRINT");         break;
        case ASHED_CMD_APPEND_MAJOR:  printf("APPEND MAJOR");  break;
        case ASHED_CMD_APPEND_MINOR:  printf("append minor");  break;
        case ASHED_CMD_INSERT_MAJOR:  printf("INSERT MAJOR");  break;
        case ASHED_CMD_INSERT_MINOR:  printf("insert minor");  break;
        case ASHED_CMD_REPLACE_MAJOR: printf("REPLACE MAJOR"); break;
        case ASHED_CMD_REPLACE_MINOR: printf("replace minor"); break;
        case ASHED_CMD_DELETE:        printf("DELETE");        break;
        case ASHED_CMD_WRITE:         printf("WRITE");         break;
        case ASHED_CMD_GOTO:          printf("GOTO LINE");     break;
        case ASHED_CMD_CLEAR:         printf("CLEAR");         break;
        case ASHED_CMD_QUIT:          printf("QUIT");          break;
        case ASHED_CMDTYPES_COUNT:
        default: shlog(SHLOG_FATAL, "Unreachable: command type print");               abort();
    }
    printf("\n");    
}

int ashed_quit(char *line)
{
    if (line == NULL || strlen(line) == 0) {
        if (!saved) {
            printf("unsaved session\n");
            return ASHED_CODE_OK;
        } else return ASHED_CODE_QUIT;
    } else if (streq(line, "!")) return ASHED_CODE_QUIT;
    else return ASHED_CODE_UNKNOWN_CMD;
}

int ashed_clear() { printf("\e[1;1H\e[2J"); return ASHED_CODE_OK; }

int ashed_print(AshedAddress addr)
{
    int line_to_print;
    bool print_single_line = true;
    switch(addr.type)
    {
        case ASHED_ADDR_CURRENT: line_to_print = curr_line_n;              break;
        case ASHED_ADDR_LAST:    line_to_print = ashed_buffer.count-1;      break;
        case ASHED_ADDR_NUMBER:  line_to_print = addr.n - 1;                break;
        case ASHED_ADDR_NEXT:    line_to_print = curr_line_n + addr.n;     break; 
        case ASHED_ADDR_PREV:    line_to_print = curr_line_n - addr.n;     break; 
        case ASHED_ADDR_RANGE:   print_single_line = false;                 break;
        case ASHED_ADDR_INVALID:
        default:                 shlog(SHLOG_FATAL, "Unreachable: ashed print"); abort();
    }

    if (print_single_line) {
        buffer_print_line(ashed_buffer, line_to_print);
    } else {
        buffer_print_range(ashed_buffer, addr.r);
    }
    
    return ASHED_CODE_OK;
}

int ashed_set_mode_command() { ashed_mode = ASHED_MODE_COMMAND; return ASHED_CODE_OK; }

// TODO: perche' aumenta il buffer count?
int ashed_set_mode_append_major() 
{
    ashed_mode = ASHED_MODE_APPEND_MAJOR;
    curr_line_n = ashed_buffer.count;
    ashed_buffer.count++;
    return ASHED_CODE_OK;
}

int ashed_set_mode_append_minor()
{
    ashed_mode = ASHED_MODE_APPEND_MINOR;
    return ASHED_CODE_OK;
}

int ashed_set_mode_insert_major(AshedAddress addr) 
{
    ashed_mode = ASHED_MODE_INSERT_MAJOR;
    saved_addr = malloc(sizeof(AshedAddress));
    *saved_addr = addr;
    return ASHED_CODE_OK;
}

int ashed_set_mode_insert_minor(AshedAddress addr)
{
    (void)addr;
    ashed_mode = ASHED_MODE_INSERT_MINOR;
    return ASHED_CODE_OK;
}

int ashed_set_mode_replace_major(AshedAddress addr) 
{
    ashed_mode = ASHED_MODE_REPLACE_MAJOR;
    saved_addr = malloc(sizeof(AshedAddress));
    *saved_addr = addr;
    return ASHED_CODE_OK;
}

int ashed_set_mode_replace_minor(AshedAddress addr)
{
    (void) addr;
    ashed_mode = ASHED_MODE_REPLACE_MINOR;
    return ASHED_CODE_OK;
}

int ashed_write_line(char *line)
{
    buffer_write_line(&ashed_buffer, line, curr_line_n);
    curr_line_n++;
    saved = false;
    return ASHED_CODE_OK;
}

int ashed_write_file(char *line)
{
    bool save_and_quit = false;
    bool save_on_different_file = false;
    if (strlen(line) > 0) {
        if (streq(line, "q")) {
            save_and_quit = true;
        } else if (strlen(line) > 1 && (line[1] == ' ' || line[1] == '\t')) {
            line++;
            line = remove_spaces(line);
            sprintf(filename, line);
            save_on_different_file = true;
        } else return 1;
    }

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
    // TODO save on different file with > or >>
    if (EDDEBUG) shlog(SHLOG_DEBUG, "Blob: %s", blob);
    size_t write_size = total_len*sizeof(char);
    printf("%zu\n", write_size);
    if (!write_entire_file(filename, blob, write_size)) return ASHED_CODE_ERROR;
    if (!save_on_different_file) saved = true;
    if (save_and_quit) return ashed_quit(NULL);
    return ASHED_CODE_OK;
}

int ashed_goto_line(AshedAddress addr)
{
    switch(addr.type)
    {
        case ASHED_ADDR_CURRENT:                                                    break;
        case ASHED_ADDR_LAST:    curr_line_n = ashed_buffer.count-1;               break;
        case ASHED_ADDR_NUMBER:  curr_line_n =  addr.n - 1;                        break;
        case ASHED_ADDR_NEXT:    curr_line_n += addr.n;                            break;
        case ASHED_ADDR_PREV:    curr_line_n -= addr.n;                            break; 
        case ASHED_ADDR_RANGE:   shlog(SHLOG_WARNING, "Range alone has no effect"); break;
        case ASHED_ADDR_INVALID:
        default:                 shlog(SHLOG_FATAL, "Unreachable: goto line");      abort();
    }
    if (addr.type != ASHED_ADDR_RANGE)
        buffer_print_line(ashed_buffer, curr_line_n);
    return ASHED_CODE_OK;
}

int ashed_insert_line(char *line_str)
{
    int line_n;
    switch(saved_addr->type)
    {
        case ASHED_ADDR_CURRENT: line_n = curr_line_n;                          break;
        case ASHED_ADDR_LAST:    line_n = ashed_buffer.count-1;                  break;
        case ASHED_ADDR_NUMBER:  line_n = saved_addr->n - 1;                     break;
        case ASHED_ADDR_NEXT:    line_n = curr_line_n + saved_addr->n;          break; 
        case ASHED_ADDR_PREV:    line_n = curr_line_n - saved_addr->n;          break; 
        case ASHED_ADDR_RANGE:   { shlog(SHLOG_WARNING, "Cannot insert a range"); return 1; }
        case ASHED_ADDR_INVALID:
        default:                 shlog(SHLOG_FATAL, "Unreachable: insert line"); abort();
    }
    buffer_shift_right(&ashed_buffer, line_n);
    buffer_write_line(&ashed_buffer, line_str, line_n);
    saved = false;

    saved_addr->type = ASHED_ADDR_NEXT;
    saved_addr->n = 1;
    curr_line_n++;
    return ASHED_CODE_OK;
}

// TODO: replace range cancella tutte le righe del range e ne riscrive solamente una
int ashed_replace_line(char *line_str)
{
    int line_n;
    switch(saved_addr->type)
    {
        case ASHED_ADDR_CURRENT: line_n = curr_line_n;                          break;
        case ASHED_ADDR_LAST:    line_n = ashed_buffer.count-1;                  break;
        case ASHED_ADDR_NUMBER:  line_n = saved_addr->n - 1;                     break;
        case ASHED_ADDR_NEXT:    line_n = curr_line_n + saved_addr->n;          break; 
        case ASHED_ADDR_PREV:    line_n = curr_line_n - saved_addr->n;          break; 
        case ASHED_ADDR_RANGE:   { shlog(SHLOG_WARNING, "Cannot replace a range"); return 1; } // TODO: in realta' si puo'
        case ASHED_ADDR_INVALID:
        default:                 shlog(SHLOG_FATAL, "Unreachable: replace line"); abort();
    }
    buffer_write_line(&ashed_buffer, line_str, line_n);
    saved = false;

    free(saved_addr);
    saved_addr = NULL;
    return ASHED_CODE_OK;
}

int ashed_delete(AshedAddress addr)
{
    int line_n;
    bool delete_single_line = true;
    switch(addr.type)
    {
        case ASHED_ADDR_CURRENT: line_n = curr_line_n;           break;
        case ASHED_ADDR_LAST:    line_n = ashed_buffer.count-1;   break;
        case ASHED_ADDR_NUMBER:  line_n = addr.n - 1;            break;
        case ASHED_ADDR_NEXT:    line_n = curr_line_n + addr.n; break; 
        case ASHED_ADDR_PREV:    line_n = curr_line_n - addr.n; break; 
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
    while (curr_line_n >= ashed_buffer.count)
        curr_line_n--;

    saved = false;
    return ASHED_CODE_OK;
}

int ashed_help(char *line)
{
    if (strlen(line) == 0) {
        printf(ASHED_USAGE);
        return ASHED_CODE_OK;
    }
    AshedCmdType help_type = getAshedCmdType(line);
    if (help_type == ASHED_CMD_UNKNOWN) {
        shlog(SHLOG_ERROR, "Unknown command %s", line);
        shlog(SHLOG_TODO, "Print advice to go wath documentation of available commands");
    } else {
        shlog(SHLOG_TODO, "Get help for");
        ashed_CmdType_print(help_type);
    }
    return ASHED_CODE_OK;
}

void ashed_print_mode_prompt(char *prompt)
{
    printf("%s%d%s: ", prompt, curr_line_n + 1, saved ? "" : "*");
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
    char input_buffer[MAX_CHARS];
    char *line = NULL;

    while (true) {
        switch(ashed_mode)
        {
            case ASHED_MODE_COMMAND:  ashed_print_mode_prompt("C"); break;
            case ASHED_MODE_APPEND_MAJOR:   ashed_print_mode_prompt("A"); break;
            case ASHED_MODE_APPEND_MINOR:   ashed_print_mode_prompt("a"); break;
            case ASHED_MODE_INSERT_MAJOR:   ashed_print_mode_prompt("I"); break;
            case ASHED_MODE_INSERT_MINOR:   ashed_print_mode_prompt("i"); break;
            case ASHED_MODE_REPLACE_MAJOR:  ashed_print_mode_prompt("R"); break;
            case ASHED_MODE_REPLACE_MINOR:  ashed_print_mode_prompt("r"); break;
            default: shlog(SHLOG_FATAL, "Unknow ashed mode %d", ashed_mode); abort();
        }
        line = fgets(input_buffer, MAX_CHARS, stdin);
        if (line == NULL) {
            shlog(SHLOG_ERROR, "Could not read line\n");
            continue;
        }
        if (!remove_newline(&line)) continue;

        switch (ashed_mode)
        {
            case ASHED_MODE_COMMAND:
                {
                    line = remove_spaces(line);
                    AshedAddress address = parseAddress(&line);

                    ashedAddress_print(address);
                    if (EDDEBUG) {
                        printf("now parse command from `%s`\n", line);
                    }

                    if (address.type == ASHED_ADDR_INVALID) {
                        ashed_code = 1;
                    } else {
                        AshedCmdType ashed_cmd_type = getAshedCmdType(line);
                        if (ashed_cmd_type != ASHED_CMD_GOTO || ashed_cmd_type != ASHED_CMD_UNKNOWN) line++;
                        ashed_CmdType_print(ashed_cmd_type);
                        switch (ashed_cmd_type)
                        {
                            case ASHED_CMD_UNKNOWN:       ashed_code = 1;                                     break;
                            case ASHED_CMD_HELP:          ashed_code = ashed_help(line);                      break;
                            case ASHED_CMD_PRINT:         ashed_code = ashed_print(address);                  break;
                            case ASHED_CMD_APPEND_MAJOR:  ashed_code = ashed_set_mode_append_major();         break;
                            case ASHED_CMD_APPEND_MINOR:  ashed_code = ashed_set_mode_append_minor();         break;
                            case ASHED_CMD_INSERT_MAJOR:  ashed_code = ashed_set_mode_insert_major(address);  break;
                            case ASHED_CMD_INSERT_MINOR:  ashed_code = ashed_set_mode_insert_minor(address);  break;
                            case ASHED_CMD_REPLACE_MAJOR: ashed_code = ashed_set_mode_replace_major(address); break;
                            case ASHED_CMD_REPLACE_MINOR: ashed_code = ashed_set_mode_replace_minor(address); break;
                            case ASHED_CMD_DELETE:        ashed_code = ashed_delete(address);                 break;
                            case ASHED_CMD_WRITE:         ashed_code = ashed_write_file(line);                break;
                            case ASHED_CMD_GOTO:          ashed_code = ashed_goto_line(address);              break;
                            case ASHED_CMD_CLEAR:         ashed_code = ashed_clear();                         break;
                            case ASHED_CMD_QUIT:          ashed_code = ashed_quit(line);                      break;
                            case ASHED_CMDTYPES_COUNT:
                            default: shlog(SHLOG_FATAL, "Unreachable: ashed command type");                   abort();
                        }
                    }
                } break;

            case ASHED_MODE_APPEND_MAJOR:
                {
                    if (streq(line, ".")) {
                        curr_line_n--;
                        ashed_code = ashed_set_mode_command();
                    }
                    else ashed_code = ashed_write_line(line);
                } break;

            case ASHED_MODE_APPEND_MINOR:
                {
                    shlog(SHLOG_TODO, "append minor mode");
                    ashed_code = ashed_set_mode_command();
                } break;

            case ASHED_MODE_INSERT_MAJOR:
                {
                    if (saved_addr == NULL) {
                        shlog(SHLOG_FATAL, "No saved address found");
                        abort();
                    }
                    if (streq(line, ".")) {
                        free(saved_addr);
                        saved_addr = NULL;
                        ashed_code = ashed_set_mode_command();
                    } else {
                        ashed_code = ashed_insert_line(line);
                    }
                } break;

            case ASHED_MODE_INSERT_MINOR:
                {
                    shlog(SHLOG_TODO, "insert minor mode");
                    ashed_code = ashed_set_mode_command();
                } break;

            case ASHED_MODE_REPLACE_MAJOR:
                {
                    if (saved_addr == NULL) {
                        shlog(SHLOG_FATAL, "No saved address found");
                        abort();
                    }
                    if (streq(line, ".")) {
                        free(saved_addr);
                        saved_addr = NULL;
                    } else {
                        ashed_code = ashed_replace_line(line);
                    }
                    ashed_code = ashed_set_mode_command();
                } break;

            case ASHED_MODE_REPLACE_MINOR:
                {
                    shlog(SHLOG_TODO, "replace minor mode");
                    ashed_code = ashed_set_mode_command();
                } break;

            default:
                {
                    shlog(SHLOG_FATAL, "Unknown ashed mode (%d)\n", ashed_mode);
                    buffer_free(&ashed_buffer);
                    abort();
                }
        }
        //shlog(SHLOG_DEBUG, "Printing buffer (%d/%d):\n", ashed_buffer.count, ashed_buffer.capacity);
        if (EDDEBUG) {
            //buffer_print(ashed_buffer);
        }

        if      (ashed_code == ASHED_CODE_QUIT)         break;
        else if (ashed_code == ASHED_CODE_UNKNOWN_CMD)  printf("huh?\n");
        else if (ashed_code != ASHED_CODE_OK)           shlog(SHLOG_FATAL, "Unknown shed exit code %d\n", ashed_code);
    }
    buffer_free(&ashed_buffer);
    return 0;
}
