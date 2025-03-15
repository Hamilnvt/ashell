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

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>

#define DEBUG 0

// UTILITIES ///////////////////////////////////
bool streq(char *a, char *b) { return strcmp(a, b) == 0; }

int matoi(char *str)
{
    int n = 0;
    int len = strlen(str);
    int i = 0;
    while (i < len) {
        if (isdigit(str[i])) {
            n += (str[i] - '0')*((int)pow(10, len-i-1));
        } else return -1;
        i++;
    }
    return n;
}

bool does_file_exist(char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

bool remove_newline(char **str)
{
    int len = strlen(*str);
    if (len > 1) {
        (*str)[len-1] = '\0'; 
    } else false;
    return true;
}

char *remove_spaces(char *line)
{
    if (DEBUG) printf("[DEBUG]: Parsing line: `%s`\n", line);
    while (*line == ' ' || *line == '\t') line++;
    int len = strlen(line);
    while (*(line+len-1) == ' ' || *(line+len-1) == '\t') {
        len--;
    }
    *(line+len) = '\0'; 
    len = strlen(line);
    if (len == 0) return 0;
    if (DEBUG) printf("[DEBUG]: Line without spaces: `%s`\n", line);
    return line;
}

#define RETURN_CODE(n) { *code = (n); return; }
//////////////////////////////////////////////////

#define MAX_CHARS (8*1024)
#define BUFFER_INITIAL_CAPACITY 8;
typedef struct
{
    char **items;
    int count;
    int capacity;
} Buffer;
Buffer buffer = {0};

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
        //printf("Writing in an already allocated line\n");
        if (buf->items[line_n] != NULL) {
            //printf("Modifying a line\n");
            free(buf->items[line_n]); 
        }
    } else if (buf->count == buf->capacity) {
        //printf("Reallocating buffer -> %d\n", 2*buf->capacity);
        buf->items = realloc(buf->items, 2*buf->capacity*sizeof(char *));
        assert(buf->items != NULL && "Buy more RAM, lol.");
        buf->capacity *= 2;
        for (int i = buf->count; i < buf->capacity; i++)
            buf->items[i] = NULL;
        buf->count++;
    } else {
        //printf("Writing a new line\n");
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

int current_line = 0;
char filename[256] = {0};
bool saved = true;

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

void ed_quit(int *code, char *line)
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
void ed_clear(int *code) { printf("\e[1;1H\e[2J"); *code = 0; }
void ed_print(int *code, char *line) {
    if (strlen(line) > 2) RETURN_CODE(1);

    if (streq(line, "p")) {
        if (buffer.items[current_line] != NULL) printf("%s\n", buffer.items[current_line]);
        RETURN_CODE(0);
    }

    if (line[1] == 'a') {
        buffer_print(buffer);
        RETURN_CODE(0);
    }

    if (isdigit(line[1])) {
        line++;
        int n = matoi(line);
        if (n > 0 && n <= buffer.count && buffer.items[n-1] != NULL) {
            printf("%s\n", buffer.items[n-1]);
            RETURN_CODE(0);
        } else RETURN_CODE(1);
    }

    RETURN_CODE(1);
}

void ed_advance(int *code, char *line)
{
    size_t len = strlen(line);
    size_t n = 0;

    if (len == 1) {
        n = 1;
    } else {
        line++;
        n = matoi(line);
    }

    if (n != -1) {
        current_line = (current_line + n) % buffer.count;
        RETURN_CODE(0);
    } else RETURN_CODE(1);

    RETURN_CODE(1);
}
void ed_retreat(int *code, char *line)
{
    size_t len = strlen(line);
    size_t n = 0;

    if (len == 1) {
        n = 1;
    } else {
        line++;
        n = matoi(line);
    }

    if (n != -1) {
        current_line = (current_line + buffer.count - n) % buffer.count;
        RETURN_CODE(0);
    } else RETURN_CODE(1);

    RETURN_CODE(1);
}

typedef enum
{
    EDMODE_COMMAND,
    EDMODE_INPUT,
} EdMode;

void ed_goto_input_mode(int *code, char *line, EdMode *mode)
{
    int len = strlen(line);
    if (len == 1) {
        *mode = EDMODE_INPUT;
        RETURN_CODE(0);
    }

    RETURN_CODE(1);
}

void ed_goto_command_mode(int *code, EdMode *mode)
{
    *mode = EDMODE_COMMAND;
    *code = 0; 
}

void ed_write_line(int *code, char *line)
{
    buffer_write_line(&buffer, line, current_line);
    current_line++;
    saved = false;
    *code = 0;
}

void ed_write_file(int *code, char *line)
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

    char blob[buffer.count*(MAX_CHARS+1)];
    char *begin = blob;
    size_t total_len = 0;
    for (int i = 0; i < buffer.count; i++) {
        char *line = buffer.items[i];
        if (line != NULL) {
            int len = strlen(line);
            memcpy(begin, line, strlen(line)*sizeof(char));
            begin += len+1;
            total_len += len+1;
            blob[total_len-1] = '\n';
        }
    }
    if (DEBUG) printf("Blob: %s", blob);
    size_t write_size = total_len*sizeof(char);
    printf("%zu\n", write_size);
    if (!write_entire_file(filename, blob, write_size)) RETURN_CODE(2);
    if (!save_on_different_file) saved = true;
    if (save_and_quit) ed_quit(code, NULL);
    else RETURN_CODE(0);
}

typedef enum
{
    EDADDR_INVALID,
    EDADDR_CURRENT,
    EDADDR_LAST,
    EDADDR_NUMBER,
    EDADDR_RANGE,
    EDADDR_COUNT,
} EdAddressType;

typedef struct
{
    int begin;
    int end;
} Range;

typedef struct
{
    EdAddressType type;
    union {
        int n;
        Range r;
    };
} EdAddress;

int main(int argc, char **argv)
{
    static int ed_file_n = 0;

    if (argc > 2) {
        printf("Usage: shed [file_path]\n");
        return 1;
    }
    if (argc == 1) {
        sprintf(filename, "temped%d\0", ed_file_n++);
        while (does_file_exist(filename))
            sprintf(filename, "temped%d\0", ed_file_n++);
        buffer_init(&buffer);
    } else if (argc == 2) {
        sprintf(filename, argv[1]);
        if (does_file_exist(filename)) {
            if (!buffer_init_from_file(&buffer, filename)) return 1;
        } else buffer_init(&buffer);
    }

    printf("ashed - %s\n\n", filename);
    EdMode mode = EDMODE_COMMAND;
    int ed_code = 0;
    char input_buffer[MAX_CHARS];
    char *line = NULL;

    while (true) {
        if (mode == EDMODE_COMMAND) printf("C%d: ", current_line + 1);
        else printf("I%d: ", current_line + 1);
        line = fgets(input_buffer, MAX_CHARS, stdin);
        if (line == NULL) {
            printf("Could not read line\n");
            continue;
        }
        if (!remove_newline(&line)) continue;
        if (mode == EDMODE_COMMAND) {
            line = remove_spaces(line);
            if      (streq(line, "c")) ed_clear(&ed_code);
            else if (line[0] == 'q')   ed_quit(&ed_code, line);
            else if (line[0] == 'p')   ed_print(&ed_code, line);
            else if (line[0] == '+')   ed_advance(&ed_code, line);
            else if (line[0] == '-')   ed_retreat(&ed_code, line);
            else if (line[0] == 'i')   ed_goto_input_mode(&ed_code, line, &mode);
            else if (line[0] == 'w')   ed_write_file(&ed_code, line);
            else                       ed_code = 1;

            if      (ed_code == -1) break;
            else if (ed_code == 1)  printf("huh?\n");
            else if (ed_code != 0)  printf("[FATAL] Unknown shed exit code %d\n", ed_code);
        } else if (mode == EDMODE_INPUT) {
            if (streq(line, ".")) ed_goto_command_mode(&ed_code, &mode);
            else                  ed_write_line(&ed_code, line);
        } else {
            printf("[ERROR] Unknown shed mode (%d)\n", mode);
            buffer_free(&buffer);
            abort();
            return 1;
        }
    }
    buffer_free(&buffer);
    return 0;
}
