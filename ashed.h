#ifndef ASHED_H_
#define ASHED_H_

#include "ashell_utils.h"

#define EDDEBUG 0

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
    ASHED_MODE_APPEND_MAJOR,
    ASHED_MODE_APPEND_MINOR,
    ASHED_MODE_INSERT_MAJOR,
    ASHED_MODE_INSERT_MINOR,
    ASHED_MODE_REPLACE_MAJOR,
    ASHED_MODE_REPLACE_MINOR,
} AshedMode;

#define ASHED_MODE_COLOR_COMMAND       WHITE
#define ASHED_MODE_COLOR_APPEND_MAJOR  "50;150;255"
#define ASHED_MODE_COLOR_APPEND_MINOR  ASHED_MODE_COLOR_APPEND_MAJOR
#define ASHED_MODE_COLOR_INSERT_MAJOR  "100;255;100"
#define ASHED_MODE_COLOR_INSERT_MINOR  ASHED_MODE_COLOR_INSERT_MAJOR
#define ASHED_MODE_COLOR_REPLACE_MAJOR "255;100;100"
#define ASHED_MODE_COLOR_REPLACE_MINOR ASHED_MODE_COLOR_REPLACE_MAJOR

char *getAshedModeColor(AshedMode mode);

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

typedef enum
{
    ASHED_CMD_UNKNOWN,
    ASHED_CMD_HELP,
    ASHED_CMD_PRINT,
    ASHED_CMD_APPEND_MAJOR,
    ASHED_CMD_APPEND_MINOR,
    ASHED_CMD_INSERT_MAJOR,
    ASHED_CMD_INSERT_MINOR,
    ASHED_CMD_REPLACE_MAJOR,
    ASHED_CMD_REPLACE_MINOR,
    ASHED_CMD_DELETE,
    ASHED_CMD_WRITE,
    ASHED_CMD_GOTO,
    ASHED_CMD_CLEAR,
    ASHED_CMD_QUIT,
    ASHED_CMDTYPES_COUNT 
} AshedCmdType;

AshedCmdType getAshedCmdType(char *line);

typedef enum
{
    ASHED_CODE_OK,
    ASHED_CODE_UNKNOWN_CMD,
    ASHED_CODE_ERROR,
    ASHED_CODE_QUIT,
} AshedCode;

// MODES
void ashed_set_mode_command();
void ashed_set_mode_append_major();
void ashed_set_mode_append_minor();
void ashed_set_mode_insert_major(AshedAddress addr);
void ashed_set_mode_insert_minor(AshedAddress addr);
void ashed_set_mode_replace_major(AshedAddress addr);
void ashed_set_mode_replace_minor(AshedAddress addr);
void ashed_set_mode_find_major(); // TODO
void ashed_set_mode_find_minor();

void ashed_write_line(char *line);
AshedCode ashed_write_file(char *line);

AshedCode ashed_quit(char *line);
void ashed_clear();
void ashed_print(AshedAddress addr);
AshedCode ashed_advance(AshedAddress addr);
AshedCode ashed_retreat(AshedAddress addr);

void ashed_insert_line(char *line);
void ashed_append_line(char *line);
void ashed_replace_line(char *line);
void ashed_delete(AshedAddress addr);

// ASHED DOCUMENTATION //////////////////// 
#define ASHED_DOC   "ashed is a mode line oriented text editor inspired by GNU's `ed` (https://www.gnu.org/software/ed/) ...TODO"
#define ASHED_USAGE ("ashed changes behaviour depending on which MODE it is set to.\n"\
                     "    - Command: execute the majority of the commands and switch between modes.\n"\
                     "    - Append:  write at the end of the file\n"\
                     "    - Insert:  write between lines\n"\
                     "    - Replace: replace lines\n"\
                     "    - Find:    search for string patterns in the file (TODO - da pensare)\n"\
                     "A . (dot) quits the current mode and goes in Command Mode.\n"\
                     "    Modes:\n\n"\
                     "    Command Mode:\n\n"\
                     "    A command generally executes an action on the line of an address.\n"\
                     "    It has the following form: [ADDRESS | .]COMMAND\n"\
                     "        ADDRESS\n\n"\
                     "        An address specifies the line(s) to which apply the command (TODO: non so l'inglese).\n"\
                     "        There are many types of addresses:\n"\
                     "            - Current line:       . (it's the default for every command, even though some of them do not require an address)\n"\
                     "            - Last line:          $\n"\
                     "            - n-th line:          n\n"\
                     "            - Next     n-th line: +[n | 1]\n"\
                     "            - Previous n-th line: -[n | 1]\n"\
                     "            - Range of lines:     x,y (command applies from line x to line y included).\n"\
                     "                > . and $ are also valid in a range. (TODO: mi sa che . non e' ancora valido)\n"\
                     "                > Some ranges have a shortcut:\n"\
                     "                    - ,  is 1,$\n"\
                     "                    - ;  is .,$\n"\
                     "                    - ,x is 1,x\n"\
                     "                    - x, is x,$ (TODO: togliere x;)\n"\
                     "            COMMANDS\n\n"\
                     "            A\tgo in Append Mode\n"\
                     "            I\tgo in Insert Mode\n"\
                     "            R\tgo in Replace Mode\n\n"\
                     "            a\tappend a single line at the end of the file\n"\
                     "            i\tinsert a single line at ADDRESS\n"\
                     "            r\treplace the line(s) at ADDRESS\n\n"\
                     "            c\tclear the screen of the terminal\n"\
         )
///////////////////////////////////////////

#endif // ASHED_H_

