#include "ashell_utils.h"

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

bool is_dir(char *dir_path)
{
    if (!does_file_exist(dir_path)) return false;

    struct stat st;
    stat(dir_path, &st);
    return ((st.st_mode & S_IFMT) == S_IFDIR);
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
