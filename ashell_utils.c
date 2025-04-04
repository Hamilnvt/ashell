#include "ashell_utils.h"

// Strings manipulation //////////////////////////////////////
bool streq(char *a, char *b) { return strcmp(a, b) == 0; }

int count_words(char *input)
{
    if (UTILS_DEBUG) shlog(SHLOG_DEBUG, "Count words in `%s`", input);
    int count = 0;
    bool in_word = true;
    while (*input == ' ') input++;
    if (*input != '\0') count = 1;
    while (*input != '\0') {
        if (in_word) {
            while(*input != ' ' && *input != '\0') input++;
            in_word = false;
        } else {
            while(*input == ' ' && *input != '\0') input++;
            if (*input != '\0') {
                in_word = true;
                count++;
            }
        }
    }
    return count;
}

// Words has already allocated enough memory for all the words.
void tokenize_string(char *input, ArrayOfStrings *words)
{
    char *tmp = input;
    char word[256];
    if (UTILS_DEBUG) shlog(SHLOG_DEBUG, "Tokeninzing string: `%s`", input);
    int index = 0;
    while (*tmp != '\0') {
        while (*tmp == ' ') tmp++;
        if (*tmp == '\0') break;

        int len = 0;
        char *begin_word = tmp;
        while (*tmp != ' ' && *tmp != '\0') {
            len++;
            tmp++;
        } 
        if (len > 0) {
            sprintf(word, "%.*s", len+1, begin_word);
            word[len] = '\0';
            words->items[index] = strdup(word);
            index++;
        }
    }
}

int matoi(char *str)
{
    int n = 0;
    int len = strlen(str);
    int i = 0;
    while (i < len && str[i] != '\0') {
        if (isdigit(str[i])) {
            n += (str[i] - '0')*((int)pow(10, len-i-1));
        } else return -1;
        i++;
    }
    return n;
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
    if (UTILS_DEBUG) printf("[DEBUG]: Parsing line: `%s`\n", line);
    while (*line == ' ' || *line == '\t') line++;
    int len = strlen(line);
    while (*(line+len-1) == ' ' || *(line+len-1) == '\t') {
        len--;
    }
    *(line+len) = '\0'; 
    len = strlen(line);
    if (len == 0) return 0;
    if (UTILS_DEBUG) printf("[DEBUG]: Line without spaces: `%s`\n", line);
    return line;
}
//////////////////////////////////////////////////

// Files and Dirs ////////////////////////////// 
bool does_file_exist(char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

bool is_dir(char *dir_path)
{
    if (!does_file_exist(dir_path)) return false;

    struct stat st;
    stat(dir_path, &st);
    return ((st.st_mode & S_IFMT) == S_IFDIR);
}
//////////////////////////////////////////////////

// COLORS AND PRINT /////////////////////////////////////// 
void print_fg_text(char *text, char *fg) { printf("%s%sm%s%s", COLOR_FG_BEGIN, fg, text, COLOR_END); }
void print_bg_text(char *text, char *bg) { printf("%s%sm%s%s", COLOR_BG_BEGIN, bg, text, COLOR_END); }
void print_color_text(char *text, char *fg, char *bg) { printf("%s%sm%s%sm%s%s", COLOR_BG_BEGIN, bg, COLOR_FG_BEGIN, fg, text, COLOR_END); }

void printn_fg_text(char *text, char *fg) { printf("%s%sm%s%s\n", COLOR_FG_BEGIN, fg, text, COLOR_END); }
void printn_bg_text(char *text, char *bg) { printf("%s%sm%s%s\n", COLOR_BG_BEGIN, bg, text, COLOR_END); }
void printn_color_text(char *text, char *fg, char *bg) { printf("%s%sm%s%sm%s%s\n", COLOR_BG_BEGIN, bg, COLOR_FG_BEGIN, fg, text, COLOR_END); }
/////////////////////////////////////////////////////////// 

// SHLOG ////////////////////////////////////////////////// 
char shlog_buffer[1024];

void shlog_NO_NEWLINE(ShlogLevel lvl, char *format, ...)
{
    va_list msg_fmt; 
    va_start(msg_fmt, format);
    vsprintf(shlog_buffer, format, msg_fmt);
    va_end(msg_fmt);

    switch(lvl) {
        case SHLOG_INFO:    print_fg_text("[INFO] ",    SHLOG_INFO_COLOR);    break; 
        case SHLOG_DOC:     print_fg_text("[DOC] ",     SHLOG_DOC_COLOR);     break; 
        case SHLOG_USAGE:   print_fg_text("[USAGE] ",   SHLOG_USAGE_COLOR);   break; 
        case SHLOG_FLAGS:   print_fg_text("[FLAGS] ",   SHLOG_FLAGS_COLOR);   break; 
        case SHLOG_TODO:    print_fg_text("[TODO] ",    SHLOG_TODO_COLOR);    break; 
        case SHLOG_DEBUG:   print_fg_text("[DEBUG] ",   SHLOG_DEBUG_COLOR);   break; 
        case SHLOG_WARNING: print_fg_text("[WARNING] ", SHLOG_WARNING_COLOR); break; 
        case SHLOG_ERROR:   print_fg_text("[ERROR] ",   SHLOG_ERROR_COLOR);   break; 
        case SHLOG_FATAL:   print_fg_text("[FATAL] ",   SHLOG_FATAL_COLOR);   break; 
        default:            assert("Unreachable" && 0);
    }

    if (strchr(shlog_buffer, '\n')) {
        shlog(lvl, shlog_buffer);
    } else printf("%s", shlog_buffer);
}

void shlog(ShlogLevel lvl, char *format, ...)
{
    va_list msg_fmt; 
    va_start(msg_fmt, format);
    vsprintf(shlog_buffer, format, msg_fmt);
    va_end(msg_fmt);
    if (strchr(shlog_buffer, '\n')) {
        char *line = strtok(shlog_buffer, "\n");
        shlog(lvl, line);
        line = strtok(NULL, "\n");
        while (line) {
            shlog(lvl, line);
            line = strtok(NULL, "\n");
        }
    } else {
        shlog_NO_NEWLINE(lvl, shlog_buffer);
        printf("\n");
    }
}

void shlog_error_and_reset_errno(char *arg)
{
    if (arg != NULL) {
        shlog(SHLOG_ERROR, "%s: %s", arg, strerror(errno));
    } else {
        shlog(SHLOG_ERROR, "%s", arg, strerror(errno));
    }
    errno = 0;
}
void shlog_just_use_doc_for_args(char *cmd_name) { shlog(SHLOG_INFO, "`doc %s -u` to know more about `%s` usage", cmd_name, cmd_name); }
void shlog_just_use_doc_for_flags(char *cmd_name) { shlog(SHLOG_INFO, "`doc %s -f` to know more about `%s` flags", cmd_name, cmd_name); }
void shlog_unknown_flag(char *flag, char *cmd_name)
{
    shlog(SHLOG_ERROR, "Unknown flag `%s` for command `%s`", flag, cmd_name);
    shlog_just_use_doc_for_flags(cmd_name);
}
void shlog_unknown_flag_for_single_arg(char *flag, char *cmd_name)
{
    shlog(SHLOG_ERROR, "Unknown flag `%s` for command `%s <arg>`", flag, cmd_name);
    shlog_just_use_doc_for_flags(cmd_name);
}
void shlog_unknown_flag_for_multiple_args(char *flag, char *cmd_name)
{
    shlog(SHLOG_ERROR, "Unknown flag `%s` for command `%s <...args>`", flag, cmd_name);
    shlog_just_use_doc_for_flags(cmd_name);
}
void shlog_unknown_command(char *cmd_name)
{
    shlog(SHLOG_ERROR, "Unknown command `%s`", cmd_name);
    shlog(SHLOG_INFO, "`doc -c` for a comprehensive list of commands");
}
void shlog_no_flags_for_this_command(char *cmd_name)
{
    shlog(SHLOG_ERROR, "`%s` does not support flags", cmd_name);
    shlog_just_use_doc_for_flags(cmd_name);
}
void shlog_too_many_arguments(char *cmd_name)
{
    shlog(SHLOG_ERROR, "Too many arguments for `%s`", cmd_name);
    shlog_just_use_doc_for_args(cmd_name);
}
void shlog_too_few_arguments(char *cmd_name)
{
    shlog(SHLOG_ERROR, "Too few arguments for `%s`", cmd_name);
    shlog_just_use_doc_for_args(cmd_name);
}
void shlog_not_a_dir(char *path) { shlog(SHLOG_ERROR, "`%s` is not a directory", path); }
void shlog_file_does_not_exist(char *file_name) { shlog(SHLOG_ERROR, "`%s` does not exist", file_name); }
//////////////////////////////////////////////////

// ARRAY OF STRINGS /////////////////////
void aos_print(ArrayOfStrings aos)
{
    shlog(SHLOG_DEBUG, "Printing ArrayOfStrings (len = %d):", aos.count);
    for (int i = 0; i < aos.count; i++) {
        shlog(SHLOG_DEBUG, "%d: %s", i, aos.items[i]);
    }
}

void aos_init(ArrayOfStrings *aos, unsigned int size)
{
    if (size == 0) {
        shlog(SHLOG_FATAL, "Cannot allocate memory of size <= 0");
        abort();
    }
    aos->items = malloc(size*sizeof(char *));
    for (unsigned int i = 0; i < size; i++)
        aos->items[i] = NULL;
    aos->count = size;
}

inline void aos_free(ArrayOfStrings *aos) { if (aos != NULL && aos->items != NULL) free(aos->items); }
//////////////////////////////////////////////////
