#ifndef ASHED_H_
#define ASHED_H_

#include "ashell_utils.h"

#define EDDEBUG 0
#define RETURN_CODE(n) { *code = (n); return; }

#define MAX_CHARS (8*1024)
#define BUFFER_INITIAL_CAPACITY 8;
typedef struct
{
    char **items;
    int count;
    int capacity;
} Buffer;

int ashed_main(char *ashell_filename);

void buffer_init(Buffer *buf);
void buffer_write_line(Buffer *buf, char *line_str, int line_n);
void buffer_print(Buffer buf);
bool buffer_init_from_file(Buffer *buf, char *filename);
void buffer_free(Buffer *buf);
void buffer_shift_right(Buffer *buf, int line_n);
void buffer_shift_left(Buffer *buf, int line_n);

bool write_entire_file(char *path, const void *data, size_t size);

typedef enum
{    
    ASHED_MODE_COMMAND,
    ASHED_MODE_APPEND,
    ASHED_MODE_INSERT,
    ASHED_MODE_REPLACE,
} AshedMode;

typedef enum
{
    ASHED_ADDR_INVALID = -1,
    ASHED_ADDR_CURRENT,
    ASHED_ADDR_LAST,
    ASHED_ADDR_NUMBER,
    ASHED_ADDR_NEXT,
    ASHED_ADDR_PREV,
    ASHED_ADDR_RANGE,
} AshedAddressType;

typedef struct
{
    int begin;
    int end;
} Range;

typedef struct
{
    AshedAddressType type;
    union {
        int n;
        Range r;
    };
} AshedAddress;

void ashed_goto_command_mode(int *code, AshedMode *mode);
void ashed_goto_append_mode(int *code, AshedMode *mode);
void ashed_goto_replace_mode(int *code, AshedMode *mode, AshedAddress addr);
void ashed_write_line(int *code, char *line);
void ashed_write_file(int *code, char *line);

void ashed_quit(int *code, char *line);
void ashed_clear(int *code);
void ashed_print(int *code, AshedAddress addr);
void ashed_advance(int *code, AshedAddress addr);
void ashed_retreat(int *code, AshedAddress addr);
void ashed_replace_line(int *code, char *line);
void ashed_insert_line(int *code, char *line);
void ashed_delete(int *code, AshedAddress addr);

#endif // ASHED_H_

