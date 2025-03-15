/* TODO
   - swap files

   - Line addresses command:
     - . (current line, default for everything)
     - $ (last line)
     - n (n-th line)
     - , (first through last lines = 1,$)
     - ; (current through last lines = .,$)

   - Comandi:
     - a/.
     - h
     - w
     - q!

   Urgent Stack:
   - parsing dell'address e poi del command (default = current line) -> i comandi saranno del tipo [address | .]cmd (ora non e' cosi')

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

void buffer_print(Buffer buf)
{
    if (buf.count > 0) {
        int width = log10(buf.count)+1;
        printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
        for (int i = 0; i < buf.count; i++)
            printf("%*d: %s\n", width, i+1, buf.items[i]);
        printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    }
}

bool buffer_init_from_file(Buffer *buf, char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        printf("Could not open fil `%s`\n", filename);
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
        printf("Could not open file `%s` for writing: %s\n", path, strerror(errno));
        return false; 
    }

    const char *buf = data;
    while (size > 0) {
        size_t n = fwrite(buf, 1, size, f);
        if (ferror(f)) {
            printf("Could not write into file %s: %s\n", path, strerror(errno));
            if (f) fclose(f);
            return true;
        }
        size -= n;
        buf  += n;
    }

    if (f) fclose(f);
    return true;
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
void ashed_print(int *code, char *line) {
    if (strlen(line) > 2) RETURN_CODE(1);

    if (streq(line, "p")) {
        if (ashed_buffer.items[current_line] != NULL) printf("%s\n", ashed_buffer.items[current_line]);
        RETURN_CODE(0);
    }

    if (line[1] == 'a') {
        buffer_print(ashed_buffer);
        RETURN_CODE(0);
    }

    if (isdigit(line[1])) {
        line++;
        int n = matoi(line);
        if (n > 0 && n <= ashed_buffer.count && ashed_buffer.items[n-1] != NULL) {
            printf("%s\n", ashed_buffer.items[n-1]);
            RETURN_CODE(0);
        } else RETURN_CODE(1);
    }

    RETURN_CODE(1);
}

void ashed_advance(int *code, char *line)
{
    size_t len = strlen(line);
    int n = 0;

    if (len == 1) {
        n = 1;
    } else {
        line++;
        n = matoi(line);
    }

    if (n != -1) {
        current_line = (current_line + n) % ashed_buffer.count;
        RETURN_CODE(0);
    } else RETURN_CODE(1);

    RETURN_CODE(1);
}

void ashed_retreat(int *code, char *line)
{
    size_t len = strlen(line);
    int n = 0;

    if (len == 1) {
        n = 1;
    } else {
        line++;
        n = matoi(line);
    }

    if (n != -1) {
        current_line = (current_line + ashed_buffer.count - n) % ashed_buffer.count;
        RETURN_CODE(0);
    } else RETURN_CODE(1);

    RETURN_CODE(1);
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
    if (EDDEBUG) printf("Blob: %s", blob);
    size_t write_size = total_len*sizeof(char);
    printf("%zu\n", write_size);
    if (!write_entire_file(filename, blob, write_size)) RETURN_CODE(2);
    if (!save_on_different_file) saved = true;
    if (save_and_quit) ashed_quit(code, NULL);
    else RETURN_CODE(0);
}

int ashed_main(char *ashell_filename)
{
    // TODO: questa la ricrea ogni volta o rimane?
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
            printf("Could not read line\n");
            continue;
        }
        if (!remove_newline(&line)) continue;
        if (mode == ASHED_MODE_COMMAND) {
            line = remove_spaces(line);
            if      (streq(line, "c")) ashed_clear(&ashed_code);
            else if (line[0] == 'q')   ashed_quit(&ashed_code, line);
            else if (line[0] == 'p')   ashed_print(&ashed_code, line);
            else if (line[0] == '+')   ashed_advance(&ashed_code, line);
            else if (line[0] == '-')   ashed_retreat(&ashed_code, line);
            else if (line[0] == 'i')   ashed_goto_input_mode(&ashed_code, line, &mode);
            else if (line[0] == 'w')   ashed_write_file(&ashed_code, line);
            else                       ashed_code = 1;

            if      (ashed_code == -1) break;
            else if (ashed_code == 1)  printf("huh?\n");
            else if (ashed_code != 0)  printf("[FATAL] Unknown shed exit code %d\n", ashed_code);
        } else if (mode == ASHED_MODE_INPUT) {
            if (streq(line, ".")) ashed_goto_command_mode(&ashed_code, &mode);
            else                  ashed_write_line(&ashed_code, line);
        } else {
            printf("[ERROR] Unknown shed mode (%d)\n", mode);
            buffer_free(&ashed_buffer);
            abort();
            return 1;
        }
    }
    buffer_free(&ashed_buffer);
    return 0;
}
