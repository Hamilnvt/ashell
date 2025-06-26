#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

// TODO: segfault quando l'input e' vuoto

typedef struct
{
    char **items;
    int count;
} ArrayOfStrings;

void aos_print(ArrayOfStrings aos)
{
    printf("Printing ArrayOfStrings (len = %d):\n", aos.count);
    for (int i = 0; i < aos.count; i++) {
        printf("%d: %s\n", i, aos.items[i]);
    }
}

void aos_init(ArrayOfStrings *aos, int size)
{
    assert(size > 0 && "Cannot allocate memory of size 0");
    aos->items = malloc(size*sizeof(char *));
    aos->count = size;
}

void aos_free(ArrayOfStrings *aos)
{
    for (int i = 0; i < aos->count; i++)
        free(aos->items[i]);
    free(aos->items);
}

int count_words(char *input)
{
    //printf("Count words in `%s`\n", input);
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

void tokenize_string(char *input, ArrayOfStrings *words)
{
    char *tmp = input;
    char word[256];
    //printf("Tokeninzing string: `%s`\n", input);
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

typedef struct
{
    char *name;
    int argc;
    ArrayOfStrings argv;
    int flagc;
    ArrayOfStrings flagv;
} Command;

void cmd_free(Command *cmd)
{
    if (cmd->argc > 0) aos_free(&(cmd->argv));
    if (cmd->flagc > 0) aos_free(&(cmd->flagv));
    free(cmd->name);
}

void cmd_print(Command cmd)
{
    printf("Command: {\n  name = %s (TODO: type)\n  argc = %d,\n  argv = [", cmd.name, cmd.argc);
    for (int i = 0; i < cmd.argc; i++) {
        printf(" %s", cmd.argv.items[i]);
        if (i != cmd.argc - 1) printf(", ");
    }
    printf(" ],\n");
    printf("  flagc = %d,\n  flagv = [", cmd.flagc);
    for (int i = 0; i < cmd.flagc; i++) {
        printf(" %s", cmd.flagv.items[i]);
        if (i != cmd.flagc - 1) printf(", ");
    }
    printf(" ]\n}\n");
}

Command words_to_command(ArrayOfStrings words)
{
    Command cmd = {0};
    if (words.count == 0) {
        printf("[ERROR]: no command provided\n");
        return (Command){};
    }
    bool parsing_args = false;
    int argc = 0;
    bool parsing_flags = false;
    int flagc = 0;
    for (int i = 0; i < words.count; i++) {
        if (i == 0) {
            parsing_args = true;
            // TODO: Parse command to CmdType
        } else {
            if (parsing_args) {
                if (words.items[i][0] == '-') {
                    parsing_args = false;
                    parsing_flags = true;
                } else argc++;
            }
            if (parsing_flags) {
                if (words.items[i][0] != '-') {
                    printf("[ERROR]: Arguments must preceed flags.\n");
                    return (Command){};
                } else flagc++;
            }
        }
    }
    cmd.name = strdup(words.items[0]);
    cmd.argc = argc;
    aos_init(&(cmd.argv), argc);
    for (int i = 1; i < argc+1; i++) {
        cmd.argv.items[i-1] = strdup(words.items[i]);
    }
    cmd.flagc = flagc;
    aos_init(&(cmd.flagv), flagc);
    for (int i = argc+1; i < flagc+argc+1; i++) {
        cmd.flagv.items[i-(argc+1)] = strdup(words.items[i]);
    }
    return cmd;
}

int main(void)
{
    char *input = "  name    arg   -flag  ";
    ArrayOfStrings words = {0};
    int word_count = count_words(input);
    aos_init(&words, word_count);
    tokenize_string(input, &words);
    aos_print(words);

    Command cmd = words_to_command(words);
    cmd_print(cmd);

    aos_free(&words);
    cmd_free(&cmd);
    return 0;
}
