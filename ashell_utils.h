#ifndef ASHELL_UTILS_H
#define ASHELL_UTILS_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>

#define UTILS_DEBUG false

// Files and Dirs ////////////////////////////// 
bool does_file_exist(char *path);
bool is_dir(char *dir_path);
//////////////////////////////////////////////////

// TEXT COLORS and STYLES //////////////////////////////////////// 
#define COLOR_FG_BEGIN "\x1B[38;2;" // foreground plain style
#define COLOR_BG_BEGIN "\x1B[48;2;" // background plain style
#define COLOR_END "\x1B[0m"

#define WHITE "255;255;255"

#define PROMPT_COLOR     "120;120;120"
#define DIRECTORY_COLOR  "50;150;255"
#define EXECUTABLE_COLOR "50;255;50"

// Shlog colors
#define SHLOG_INFO_COLOR    "50;180;0"
#define SHLOG_DOC_COLOR     "180;180;0" 
#define SHLOG_USAGE_COLOR   "180;180;180"
#define SHLOG_FLAGS_COLOR   "50;180;180"
#define SHLOG_TODO_COLOR    "30;100;255"
#define SHLOG_DEBUG_COLOR   SHLOG_DOC_COLOR
#define SHLOG_WARNING_COLOR "180;100;0"
#define SHLOG_ERROR_COLOR   "180;30;30"
#define SHLOG_FATAL_COLOR   "200;50;200"

void print_fg_text(char *text, char *fg);
void print_bg_text(char *text, char *bg);
void print_color_text(char *text, char *fg, char *bg);

void printn_fg_text(char *text, char *fg);
void printn_bg_text(char *text, char *bg);
void printn_color_text(char *text, char *fg, char *bg);
///////////////////////////////////////////////////////////

// SHLOG ////////////////////////////////////////////////// 
typedef enum
{
    SHLOG_INFO,
    SHLOG_DOC,
    SHLOG_USAGE,
    SHLOG_FLAGS,
    SHLOG_TODO,
    SHLOG_DEBUG,
    SHLOG_WARNING,
    SHLOG_ERROR,
    SHLOG_FATAL
} ShlogLevel;

void shlog_NO_NEWLINE(ShlogLevel lvl, char *format, ...);
void shlog(ShlogLevel lvl, char *format, ...);

void shlog_error_and_reset_errno(char *arg);
void shlog_just_use_doc_for_args(char *cmd_name);
void shlog_just_use_doc_for_flags(char *cmd_name);
void shlog_unknown_flag(char *flag, char *cmd_name);
void shlog_unknown_flag_for_single_arg(char *flag, char *cmd_name);
void shlog_unknown_flag_for_multiple_args(char *flag, char *cmd_name);
void shlog_unknown_command(char *cmd_name);
void shlog_no_flags_for_this_command(char *cmd_name);
void shlog_too_many_arguments(char *cmd_name);
void shlog_too_few_arguments(char *cmd_name);
void shlog_not_a_dir(char *path);
void shlog_file_does_not_exist(char *file_name);
//////////////////////////////////////////////////

/// Checking macros  //////////////////// 
#define CHECK_TOO_MANY_ARGUMENTS(cmd, max) if ((cmd).argc > (max)) { shlog_too_many_arguments((cmd).name); return 1; }
#define CHECK_TOO_FEW_ARGUMENTS(cmd, min)  if ((cmd).argc < (min)) { shlog_too_few_arguments((cmd).name);  return 1; }
#define CHECK_NOT_EXACT_ARGUMENTS(cmd, exact)  if ((cmd).argc != (exact)) { shlog_too_few_arguments((cmd).name);  return 1; }
/////////////////////////////////////////

// ARRAY OF STRINGS /////////////////////
typedef struct
{
    char **items;
    int count;
} ArrayOfStrings;

void aos_init(ArrayOfStrings *aos, unsigned int size);
void aos_free(ArrayOfStrings *aos);
void aos_print(ArrayOfStrings aos);
//////////////////////////////////////////////////

// Strings //////////////////////////////////////
bool streq(char *a, char *b);
bool remove_newline(char **str);
char *remove_spaces(char *line);
int matoi(char *str);
int count_words(char *input);
void tokenize_string(char *input, ArrayOfStrings *words);
//////////////////////////////////////////////////

#endif // ASHELL_UTILS_H
