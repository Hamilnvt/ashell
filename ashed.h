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
void buffer_free(Buffer *buffer);
bool write_entire_file(char *path, const void *data, size_t size);
void ashed_quit(int *code, char *line);
void ashed_clear(int *code);
void ashed_print(int *code, char *line);
void ashed_advance(int *code, char *line);
void ashed_retreat(int *code, char *line);

typedef enum
{
    ASHED_MODE_COMMAND,
    ASHED_MODE_INPUT,
} AshedMode;

void ashed_goto_input_mode(int *code, char *line, AshedMode *mode);
void ashed_goto_command_mode(int *code, AshedMode *mode);
void ashed_write_line(int *code, char *line);
void ashed_write_file(int *code, char *line);

typedef enum
{
    ASHED_ADDR_INVALID,
    ASHED_ADDR_CURRENT,
    ASHED_ADDR_LAST,
    ASHED_ADDR_NUMBER,
    ASHED_ADDR_RANGE,
    ASHED_ADDR_COUNT,
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

#endif // ASHED_H_

