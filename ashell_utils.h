#ifndef ASHELL_UTILS_H
#define ASHELL_UTILS_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/stat.h>

#define UTILS_DEBUG false

bool streq(char *a, char *b);
bool remove_newline(char **str);
char *remove_spaces(char *line);
int matoi(char *str);

bool does_file_exist(char *path);
bool is_dir(char *dir_path);

#endif // ASHELL_UTILS_H
