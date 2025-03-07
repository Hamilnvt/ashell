/* TODO
    Comandi da implementare:
    - move (mv, ma sposta solamente il file, non lo rinomina)
    - rename (come mv, ma sicuro)
      > dump file (stampa il contenuto del file sul terminale)
      > dump file > (come sopra)
      > dump file > out (come sopra)
      > dump file1 > file2 (sovrascrive file2 con il contenuto di file1) (sarebbe cp, potrei fare degli alias)
      > dump file1 >> file2 (appende il contenuto di file1 a file2)      (sarebbe cat, potrei fare degli alias)
    - grep

    Urgent stack:
    - usa ovunque errno
    - documentare la sintassi dell'usage <.>, [.], <...>, etc.
    - free buffer quando si fanno le getline

    Altre cose da fare:
    - inserire la linea del codice (__LINE__) in shlog DEBUG e TODO
    - creare una cartella trash in cui vanno le cose eliminate con rm
    - permissions (mostrarle in ls, anche con colori diversi dei file; se si vuole eseguire un'operazione per cui non si hanno i permessi bisogna segnalarlo all'utente)
    - ed text editor https://www.gnu.org/software/ed/manual/ed_manual.html
    - previous and next command
      > probabilmente devo tenere un buffer che poi verra' ridirezionato (solitamente verso stdin)
    - config file che permette di modificare i colori e chissa' cos'altro
    - correction if a similar command is found
    - autocompletion by tabbing (chissa' se e' possible)
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>

#define ROOT_DIR "/home/mathieu/" // TODO: hard coded (metti una variabile root)

// TEXT COLORS and STYLES //////////////////////////////////////// 
#define COLOR_FG_BEGIN "\x1B[38;2;" // foreground plain style
#define COLOR_BG_BEGIN "\x1B[48;2;" // background plain style
#define COLOR_END "\x1B[0m"

#define WHITE "255;255;255"
#define BLACK "0;0;0"
#define RED "255;0;0"
#define GREEN "0;255;0"
#define BLUE "0;0;255"

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

void print_fg_text(char *text, char *fg) { printf("%s%sm%s%s", COLOR_FG_BEGIN, fg, text, COLOR_END); }
void print_bg_text(char *text, char *bg) { printf("%s%sm%s%s", COLOR_BG_BEGIN, bg, text, COLOR_END); }
void print_color_text(char *text, char *fg, char *bg) { printf("%s%sm%s%sm%s%s", COLOR_BG_BEGIN, bg, COLOR_FG_BEGIN, fg, text, COLOR_END); }

void printn_fg_text(char *text, char *fg) { printf("%s%sm%s%s\n", COLOR_FG_BEGIN, fg, text, COLOR_END); }
void printn_bg_text(char *text, char *bg) { printf("%s%sm%s%s\n", COLOR_BG_BEGIN, bg, text, COLOR_END); }
void printn_color_text(char *text, char *fg, char *bg) { printf("%s%sm%s%sm%s%s\n", COLOR_BG_BEGIN, bg, COLOR_FG_BEGIN, fg, text, COLOR_END); }

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
} Shlog_Levels;

static char shlog_buffer[1024];

void shlog(Shlog_Levels lvl, char *format, ...); // Forward declaration
void shlog_NO_NEWLINE(Shlog_Levels lvl, char *format, ...)
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
void shlog(Shlog_Levels lvl, char *format, ...)
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
void shlog_unknown_command(char *cmd_name) {
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
//////////////////////////////////////////////////

#define PROMPT "-> "

#define DEBUG false
typedef enum
{
    EXIT_NOT_SET,
    EXIT_OK,
    EXIT_READ_ERROR,
    EXIT_SIGNAL,
} EXIT_CODE;

// GLOBAL VARIABLES ////////////////////////
EXIT_CODE exit_code;
int signalno;
char working_directory[512];
//char output[1024]; // TODO: da pensare
///////////////////////////////////////////

void sigint_handler(int signo) {
    signalno = signo;
    printf("\n"); // Newline after ^C
    shlog(SHLOG_WARNING, "Shell has been interrupted");
}

void Start()
{
    exit_code = EXIT_NOT_SET;
    signalno = 0;
    getcwd(working_directory, sizeof(working_directory));
    if (DEBUG) shlog(SHLOG_DEBUG, "Working in %s", working_directory);

    struct sigaction sigint_action = { .sa_handler = sigint_handler };
    sigaction(SIGINT, &sigint_action, NULL);
}

typedef struct
{
    char **items;
    int count;
} ArrayOfStrings;

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
    aos->count = size;
}

void aos_free(ArrayOfStrings *aos)
{
    for (int i = 0; i < aos->count; i++)
        free(aos->items[i]);
    free(aos->items);
}

typedef enum
{
    CMD_UNKNOWN,
    CMD_DOC,
    CMD_ECHO,
    CMD_LS,
    CMD_CD,
    CMD_PWD,
    CMD_MKDIR,
    CMD_RM,
    CMD_QUIT,
    CMD_CLEAR,
    CMD_SL,
    CMD_SIZE,
    CMD_MKFL,
    CMD_SHED,
    CMD_FILE_WRITE,
    CMD_FILE_APPEND,
    CMD_DUMP,
    CMD_MOVE,
    CMD_RENAME,
    CMDTYPES_COUNT 
} CmdType;
static_assert(CMD_UNKNOWN==0 && CMD_DOC==1 && CMD_ECHO==2 && CMD_LS==3 && CMD_CD==4 && CMD_PWD==5 && CMD_MKDIR==6 && CMD_RM==7 && CMD_QUIT==8 && CMD_CLEAR==9 && CMD_SL==10 &&
              CMD_SIZE==11 && CMD_MKFL==12 && CMD_SHED==13 && CMD_FILE_WRITE == 14 && CMD_FILE_APPEND == 15 && CMD_DUMP==16 && CMD_MOVE==17 && CMD_RENAME==18 && CMDTYPES_COUNT==19, "Order of commands is preserved"); 

int count_words(char *input)
{
    if (DEBUG) shlog(SHLOG_DEBUG, "Count words in `%s`", input);
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
    if (DEBUG) shlog(SHLOG_DEBUG, "Tokeninzing string: `%s`", input);
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
    CmdType type;
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
    shlog(SHLOG_DEBUG, "Command: {");
    shlog(SHLOG_DEBUG, "  name = %s (%d),", cmd.name, cmd.type);
    shlog(SHLOG_DEBUG, "  argc = %d,", cmd.argc);
    if (cmd.argc == 0) shlog(SHLOG_DEBUG, "  argv = [],");
    else  {
        shlog_NO_NEWLINE(SHLOG_DEBUG, "  argv = [ ");
        for (int i = 0; i < cmd.argc; i++) {
            printf(cmd.argv.items[i]);
            if (i != cmd.argc - 1) printf(", ");
        }
        printf(" ]\n");
    }
    shlog(SHLOG_DEBUG, "  flagc = %d,", cmd.flagc);
    if (cmd.flagc == 0) shlog(SHLOG_DEBUG, "  flagv = []");
    else {
        shlog_NO_NEWLINE(SHLOG_DEBUG, "  flagv = [ ");
        for (int i = 0; i < cmd.flagc; i++) {
            printf(cmd.flagv.items[i]);
            if (i != cmd.flagc - 1) printf(", ");
        }
        printf(" ]\n");
    }
    shlog(SHLOG_DEBUG, "}");
}

bool streq(char *s1, char *s2) { return (strcmp(s1, s2) == 0); }

CmdType getCmdType(char *cmd_name)
{
    static_assert(CMDTYPES_COUNT == 18+1, "Exhaustive parsed command types in getCmdType");
    if      (streq(cmd_name, "doc"))                           return CMD_DOC;
    if      (streq(cmd_name, "echo"))                          return CMD_ECHO;
    else if (streq(cmd_name, "quit")  || streq(cmd_name, "q")) return CMD_QUIT;
    else if (streq(cmd_name, "ls"))                            return CMD_LS;
    else if (streq(cmd_name, "sl"))                            return CMD_SL;
    else if (streq(cmd_name, "cd"))                            return CMD_CD;
    else if (streq(cmd_name, "pwd"))                           return CMD_PWD;
    else if (streq(cmd_name, "mkdir"))                         return CMD_MKDIR;
    else if (streq(cmd_name, "rm"))                            return CMD_RM;
    else if (streq(cmd_name, "clear") || streq(cmd_name, "c")) return CMD_CLEAR;
    else if (streq(cmd_name, "size"))                          return CMD_SIZE;
    else if (streq(cmd_name, "mkfl"))                          return CMD_MKFL;
    else if (streq(cmd_name, "shed"))                          return CMD_SHED;
    else if (streq(cmd_name, ">"))                             return CMD_FILE_WRITE;
    else if (streq(cmd_name, ">>"))                            return CMD_FILE_APPEND;
    else if (streq(cmd_name, "dump"))                          return CMD_DUMP;
    else if (streq(cmd_name, "move"))                          return CMD_MOVE; // oppure mv? TODO
    else if (streq(cmd_name, "rename"))                        return CMD_RENAME;
    else                                                       return CMD_UNKNOWN;
}

bool words_to_command(ArrayOfStrings words, Command *cmd)
{
    if (words.count == 0) {
        shlog(SHLOG_FATAL, "no command provided");
        return false;
    }
    bool parsing_args = false;
    int argc = 0;
    bool parsing_flags = false;
    int flagc = 0;
    for (int i = 0; i < words.count; i++) {
        if (i == 0) {
            cmd->name = strdup(words.items[0]);
            cmd->type = getCmdType(words.items[0]);
            if (cmd->type == CMD_UNKNOWN) {
                shlog_unknown_command(cmd->name);
                return false;
            }
            parsing_args = true;
        } else {
            if (parsing_args) {
                if (words.items[i][0] == '-') {
                    parsing_args = false;
                    parsing_flags = true;
                } else argc++;
            }
            if (parsing_flags) {
                if (words.items[i][0] != '-') {
                    shlog(SHLOG_ERROR, "Arguments must preceed flags.");
                    free(cmd->name);
                    return false;
                } else flagc++;
            }
        }
    }
    cmd->argc = argc;
    if (argc > 0) {
        aos_init(&(cmd->argv), argc);
        for (int i = 1; i < argc+1; i++) {
            cmd->argv.items[i-1] = strdup(words.items[i]);
        }
    }
    cmd->flagc = flagc;
    if (flagc > 0) {
        aos_init(&(cmd->flagv), flagc);
        for (int i = argc+1; i < flagc+argc+1; i++) {
            cmd->flagv.items[i-(argc+1)] = strdup(words.items[i]);
        }
    }
    return true;
}

bool is_dir(char *dir_path)
{
    struct stat st;
    int stat_res = stat(dir_path, &st);
    if (stat_res != 0) {
        shlog(SHLOG_ERROR, "Directory `%s` does not exist.", dir_path);
        return false;
    }
    if ((st.st_mode & S_IFMT) != S_IFDIR) {
        shlog(SHLOG_ERROR, "`%s` is not a directory", dir_path);
        return false;
    }
    return true;
}

// Usage, flags and doc ///////////////////////////
static_assert(CMDTYPES_COUNT == 18+1, "Exhaustive usage for all commands");
#define USAGE_DOC         "doc [command] [...flags]"
#define USAGE_ECHO        "echo <...args>"
#define USAGE_LS          "ls [path] [...flags]"
#define USAGE_CD          "cd <dir>"
#define USAGE_PWD         "pwd"
#define USAGE_MKDIR       "mkdir <dir_path>"
#define USAGE_RM          "rm <file_path>"
#define USAGE_CLEAR       "c(lear)"
#define USAGE_QUIT        "q(uit)"
#define USAGE_SIZE        "size [path]"
#define USAGE_MKFL        "mkfl <file_path>"
#define USAGE_SHED        "shed [file_path]"
#define USAGE_FILE_WRITE  "> <file>"
#define USAGE_FILE_APPEND ">> <file>"
#define USAGE_DUMP        "dump <src> [[operator | >] [...dst]]"
#define USAGE_MOVE        "move <old_path> <new_path>"
#define USAGE_RENAME      "rename <path/to/old/name> <new_name>"

#define NO_FLAGS "No flags for this command."
static_assert(CMDTYPES_COUNT == 18+1, "Exhaustive flags for all commands");
#define FLAGS_DOC         ("-d\t\tPrint (d)ocumentation for the shell\n"\
                           "-c\t\tPrint list of available (c)ommands\n"\
                           "<command> -d\tPrint (d)oc info for <command>\n"\
                           "<command> -u\tPrint (u)sage info for <command>\n"\
                           "<command> -f\tPrint (f)lags info for <command>\n")
#define FLAGS_LS          "-h\tShow (h)idden files"
#define FLAGS_CD          NO_FLAGS
#define FLAGS_PWD         NO_FLAGS
#define FLAGS_MKDIR       NO_FLAGS
#define FLAGS_RM          NO_FLAGS
#define FLAGS_ECHO        NO_FLAGS
#define FLAGS_CLEAR       NO_FLAGS
#define FLAGS_QUIT        NO_FLAGS
#define FLAGS_SIZE        NO_FLAGS
#define FLAGS_MKFL        NO_FLAGS
#define FLAGS_SHED        NO_FLAGS
#define FLAGS_FILE_WRITE  NO_FLAGS
#define FLAGS_FILE_APPEND NO_FLAGS
#define FLAGS_DUMP        NO_FLAGS
#define FLAGS_MOVE        NO_FLAGS
#define FLAGS_RENAME      NO_FLAGS

static_assert(CMDTYPES_COUNT == 18+1, "Exhaustive doc string for all commands");
#define DOC_DOC         "Print the documentation for [command]. If none print the documentation of the shell and the documentation for all the commands."
#define DOC_LS          "List [path] content. If none list current directory content."
#define DOC_CD          "Change the working directory to <dir>."
#define DOC_PWD         "Print working directory."
#define DOC_MKDIR       "Create the directory at <dir_path>."
#define DOC_RM          "Remove the file located at <file_path>."
#define DOC_ECHO        "Print <...args> on the terminal."
#define DOC_CLEAR       "Clear the screen of the terminal."
#define DOC_QUIT        "Exit the shell safely."
#define DOC_SIZE        "Print the size of [path]. If none, print the size of the current directory."
#define DOC_MKFL        "Create the file at <file_path>."
#define DOC_SHED        "Open [file_path] with the builtin `shed` text editor. If none open an empty unsaved file"
#define DOC_FILE_WRITE  "Overwrite <file> with input (stdin)."
#define DOC_FILE_APPEND "Append input (stdin) to <file>."
#define DOC_DUMP        "Dump <file>: TODO..."
#define DOC_MOVE        "Move the file or directory (maybe with a flag? TODO) in <old_path> to <new_path>.\n"\
                        "Does not override names. Use `rename` command if you want to do so."
#define DOC_RENAME      "Rename the file at <path/to/old/name> to <new_name>."

static_assert(CMDTYPES_COUNT == 18+1, "Exhaustive documentation");
#define DOC ("DOCUMENTATION\n \n"\
             "Commands:\n "\
             "    Commands have a name, a series of arguments and a series of flags: name [...args] [...flags]\n"\
             "    - Command with no arguments:   name\n"\
             "    - Command with one argument:   name arg\n"\
             "    - Command with more arguments: name arg1 arg2\n"\
             " \n"\
             "Flags:\n "\
             "    Flags ought to go after the arguments of the command.\n"\
             "    - Flags with no arguments:    -flag\n"\
             "    - Flags with one argument:    -flag=arg\n"\
             "    - Flags with more arguments:  -flag=arg1,arg2\n"\
             "    - Flags with args with flags: -flag=[arg1 -f1,arg2 -f2]"\
     )

static_assert(CMDTYPES_COUNT == 18+1, "Exhaustive commands list");
#define CMDS_LIST  ("AVAILABLE COMMANDS\n \n"\
                    "  Utility:\n"\
                    "------------------------------------------------------------------------------------------------------\n"\
                    "  - doc     \t" DOC_DOC "\n"\
                    "  - ls      \t" DOC_LS "\n"\
                    "  - cd      \t" DOC_CD "\n"\
                    "  - pwd     \t" DOC_PWD "\n"\
                    "  - mkdir   \t" DOC_MKDIR "\n"\
                    "  - rm      \t" DOC_RM "\n"\
                    "  - echo    \t" DOC_ECHO "\n"\
                    "  - size    \t" DOC_SIZE "\n"\
                    "  - mkfl    \t" DOC_MKFL "\n"\
                    "  - shed    \t" DOC_SHED "\n"\
                    "  - >       \t" DOC_FILE_WRITE "\n"\
                    "  - >>      \t" DOC_FILE_APPEND "\n"\
                    "  - dump    \t" DOC_DUMP "\n"\
                    "  - move    \t" DOC_MOVE "\n"\
                    "  - rename    \t" DOC_RENAME "\n"\
                    "------------------------------------------------------------------------------------------------------\n \n"\
                    "  Screen management:\n"\
                    "------------------------------------------------------------------------------------------------------\n"\
                    "  - clear   \t" DOC_CLEAR "\n"\
                    "------------------------------------------------------------------------------------------------------\n \n"\
                    "  Quitting out of the shell:\n"\
                    "------------------------------------------------------------------------------------------------------\n"\
                    "  - quit    \t" DOC_QUIT  "\n"\
                    "  - `ctrl+C`\tExit the shell with a SIGINT.\n"\
                    "------------------------------------------------------------------------------------------------------"\
        )
typedef struct
{
    char *doc;
    char *usage;
    char *flags;
} DocStrings;

static_assert(CMDTYPES_COUNT == 18+1, "Exhaustive doc_strings");
DocStrings doc_strings[CMDTYPES_COUNT+1] = {
    {},                                                                               // unknown,
    { .doc=DOC_DOC,   .usage=USAGE_DOC,   .flags=FLAGS_DOC   },                       // doc,
    { .doc=DOC_ECHO,  .usage=USAGE_ECHO,  .flags=FLAGS_ECHO  },                       // echo,
    { .doc=DOC_LS,    .usage=USAGE_LS,    .flags=FLAGS_LS    },                       // ls,
    { .doc=DOC_CD,    .usage=USAGE_CD,    .flags=FLAGS_CD    },                       // cd,
    { .doc=DOC_PWD,   .usage=USAGE_PWD,   .flags=FLAGS_PWD   },                       // pwd,
    { .doc=DOC_MKDIR, .usage=USAGE_MKDIR, .flags=FLAGS_MKDIR },                       // mkdir,
    { .doc=DOC_RM,    .usage=USAGE_RM,    .flags=FLAGS_RM    },                       // rm,
    { .doc=DOC_QUIT,  .usage=USAGE_QUIT,  .flags=FLAGS_QUIT  },                       // quit,
    { .doc=DOC_CLEAR, .usage=USAGE_CLEAR, .flags=FLAGS_CLEAR },                       // clear,
    {},                                                                               // sl,
    { .doc=DOC_SIZE, .usage=USAGE_SIZE, .flags=FLAGS_SIZE },                          // size,
    { .doc=DOC_MKFL, .usage=USAGE_MKFL, .flags=FLAGS_MKFL },                          // mkfl,
    { .doc=DOC_SHED,   .usage=USAGE_SHED,   .flags=FLAGS_SHED },                      // ed,
    { .doc=DOC_FILE_WRITE,   .usage=USAGE_FILE_WRITE,   .flags=FLAGS_FILE_WRITE },    // >,
    { .doc=DOC_FILE_APPEND,   .usage=USAGE_FILE_APPEND,   .flags=FLAGS_FILE_APPEND }, // >>,
    { .doc=DOC_DUMP,   .usage=USAGE_DUMP,   .flags=FLAGS_DUMP },                      // dump,
    { .doc=DOC_MOVE,   .usage=USAGE_MOVE,   .flags=FLAGS_MOVE },                      // move,
    { .doc=DOC_RENAME,   .usage=USAGE_RENAME,   .flags=FLAGS_RENAME },                // rename,
    {}                                                                                // cmdtypes_count 
};
//////////////////////////////////////////////////

#define DOC_FLAG_DOC   (1 << 3)
#define DOC_FLAG_CMDS  (1 << 2)
#define DOC_FLAG_USAGE (1 << 1)
#define DOC_FLAG_FLAGS (1 << 0)
#define DOC_FLAG_ALL (DOC_FLAG_DOC | DOC_FLAG_CMDS | DOC_FLAG_USAGE | DOC_FLAG_FLAGS)

int exec_doc(Command *cmd)
{
    int doc_flags = cmd->flagc == 0 ? DOC_FLAG_ALL : 0;

    for (int i = 0; i < cmd->flagc; i++) {
        char *flag = cmd->flagv.items[i];
        if (streq(flag, "-d")) doc_flags |= DOC_FLAG_DOC; 
        else if (cmd->argc == 0) {
            if (streq(flag, "-c")) doc_flags |= DOC_FLAG_CMDS; 
            else {
                shlog_unknown_flag(flag, cmd->name);
                return 1;
            }
        } else {
            if      (streq(flag, "-u")) doc_flags |= DOC_FLAG_USAGE;
            else if (streq(flag, "-f")) doc_flags |= DOC_FLAG_FLAGS;
            else {
                shlog_unknown_flag_for_single_arg(flag, cmd->name);
                return 1;
            }
        }
    }

    //shlog(SHLOG_DEBUG, "doc:   %d", DOC_FLAG_DOC); 
    //shlog(SHLOG_DEBUG, "cmds:  %d", DOC_FLAG_CMDS); 
    //shlog(SHLOG_DEBUG, "usage: %d", DOC_FLAG_USAGE); 
    //shlog(SHLOG_DEBUG, "flags: %d", DOC_FLAG_FLAGS); 
    //shlog(SHLOG_DEBUG, "all:   %d", DOC_FLAG_ALL); 
    //shlog(SHLOG_DEBUG, "Input: %d", doc_flags);

    if (cmd->argc == 0) {
        if (doc_flags & DOC_FLAG_DOC)  shlog(SHLOG_DOC, DOC);
        if (doc_flags & (~DOC_FLAG_DOC & ~DOC_FLAG_CMDS)) shlog(SHLOG_DOC, " \n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
        if (doc_flags & DOC_FLAG_CMDS) shlog(SHLOG_DOC, CMDS_LIST);
    } else if (cmd->argc == 1) {
        char *cmd_to_doc_name = cmd->argv.items[0];
        CmdType type = getCmdType(cmd_to_doc_name);
        if      (type == CMD_UNKNOWN) shlog_unknown_command(cmd_to_doc_name);
        else if (type == CMD_SL) { } // magari metto un easter anche qua
        else if (type == CMDTYPES_COUNT || type > CMDTYPES_COUNT) {
            shlog(SHLOG_FATAL, "Unreachable");
            abort();
        } else {
            if (doc_flags & DOC_FLAG_DOC  ) shlog(SHLOG_DOC,   doc_strings[type].doc);
            if (doc_flags & DOC_FLAG_USAGE) shlog(SHLOG_USAGE, doc_strings[type].usage);   
            if (doc_flags & DOC_FLAG_FLAGS) shlog(SHLOG_FLAGS, doc_strings[type].flags);   
        }
    } else shlog_too_many_arguments(cmd->name);
    return 0;
}

int exec_echo(Command *cmd)
{
    for (int i = 0; i < cmd->argc; i++) printf("%s ", cmd->argv.items[i]);
    printf("\n");
    return 0;
}

int exec_pwd() { printf("%s\n", working_directory); return 0; }

int exec_quit() { exit_code = EXIT_OK; return 0; }

int exec_clear() { printf("\e[1;1H\e[2J"); return 0; }

int exec_sl() { printf("Non annusarmi l'ashell.\n"); return 0; }

// TODO:
// - support for ~ (= /home/mathieu/ -> devo ottenere il nome dell'utente, dovrebbe essere facile)
// - ls su cartella vuota stampa un avviso
// - flag per la ricerca
// - Cose da stampare (che forse diventeranno flags):
//   > dimensiona totale della cartella
//   > dimensione di ogni file
//   > permessi dei file?
int exec_ls(Command *cmd)
{
    bool ls_flag_show_hidden_files = false;

    char *path = (cmd->argc == 0 || streq(cmd->argv.items[0], ".")) ? working_directory : cmd->argv.items[0]; //TODO: da sistemare
    for (int i = 0; i < cmd->flagc; i++) {
        char *flag = cmd->flagv.items[i];
        if (streq(flag, "-h")) ls_flag_show_hidden_files = true;
        else {
            shlog_unknown_flag(flag, cmd->name);
            return 1;
        }
    }
    if (!is_dir(path)) return 1;
    DIR *d;
    struct dirent *dire;
    d = opendir(path);
    if (d == NULL) {
        shlog(SHLOG_ERROR, "Could not open directory `%s`", path);
        return 1;
    }
    char file_path[1024];
    char *color = WHITE;
    struct stat st;
    int stat_res;
    while ((dire = readdir(d)) != NULL) {
        if (streq(dire->d_name, ".")) continue;
        else if (streq(dire->d_name, "..")) continue;
        else if (!ls_flag_show_hidden_files && dire->d_name[0] == '.') continue;
        else {
            sprintf(file_path, "%s/%s", path, dire->d_name);
            stat_res = stat(file_path, &st);
            if (stat_res == 0) {
                switch(st.st_mode & S_IFMT) {
                    case S_IFDIR: color = DIRECTORY_COLOR; break;
                    default:      color = WHITE;           break;
                }
                if (streq(color, WHITE)) {
                    int permissions = st.st_mode & ~S_IFMT;
                    //printf("%d\t", permissions); 
                    switch(permissions) { // in decimal number, bleah. TODO in octal or binary
                        case 493: color = EXECUTABLE_COLOR; break;
                        default:  color = WHITE;            break;
                    }
                }
            }
            printn_fg_text(dire->d_name, color);
        }
    }

    closedir(d);
    return 0;
}

int exec_cd(Command *cmd)
{
    if (cmd->argc > 1) {
        shlog_too_many_arguments(cmd->name);
        return 1;
    }
    char *new_directory = cmd->argc == 0 ? ROOT_DIR : cmd->argv.items[0];
    if (DEBUG) shlog(SHLOG_DEBUG, "Current working directory: %s", working_directory);
    if (!is_dir(new_directory)) return 1;
    chdir(new_directory);
    getcwd(working_directory, sizeof(working_directory));
    if (DEBUG) shlog(SHLOG_DEBUG, "New working directory: %s", working_directory);
    return 0;
}

// TODO: recursive creation flag
int exec_mkdir(Command *cmd)
{
    if (cmd->argc < 1) {
        shlog(SHLOG_ERROR, "Directory path has not been provided.");
        shlog(SHLOG_USAGE, USAGE_MKDIR);
        return 1;
    }
    for (int i = 0; i < cmd->argc; i++)
        mkdir(cmd->argv.items[i], 0777);
    return 0;
}

int exec_rm(Command *cmd)
{
    bool rm_flag_recursive = false;
    if (cmd->argc == 0) {
        shlog(SHLOG_ERROR, "File path has not been provided.");
        shlog(SHLOG_USAGE, USAGE_RM);
        return 1;
    }
    for (int i = 0; i < cmd->flagc; i++) {
        char *flag = cmd->flagv.items[i];
        if (streq(flag, "-r")) rm_flag_recursive = true;
        else {
            shlog_unknown_flag(flag, cmd->name);
            return 1;
        }
    }
    int err = 0;
    for (int i = 0; i < cmd->argc; i++) {
        if (streq(cmd->argv.items[i], "-r")) continue;

        if (remove(cmd->argv.items[i]) != 0) {
            if (is_dir(cmd->argv.items[i])) {
                if (rm_flag_recursive) shlog(SHLOG_WARNING, "flag -r is not yet implemented.");
                else shlog(SHLOG_ERROR, "Could not remove not empty directory `%s`. Consider using recursive flag -r.", cmd->argv.items[i]);
            }
            else shlog(SHLOG_ERROR, "Could not remove file `%s`.", cmd->argv.items[i]);
            err = 1;
        }
    }
    return err;
}

bool calculate_file_size(char *file_path, size_t *size)
{
    shlog(SHLOG_TODO, "Ovviamente non funziona ancora");
    return false;

    if (DEBUG) shlog_NO_NEWLINE(SHLOG_DEBUG, "Calculating size of `%s`", file_path);
    struct stat st;
    if (stat(file_path, &st) != 0) {
        shlog(SHLOG_ERROR, "Could not open `%s`", file_path);
        return false;
    }
    *size += st.st_size;
    if (DEBUG) printf(": `%zu`\n", st.st_size);
    if ((st.st_mode & S_IFMT) == S_IFDIR) {
        DIR *d;
        struct dirent *dire;
        d = opendir(file_path);
        if (d == NULL) {
            shlog(SHLOG_ERROR, "Could not open directory `%s`", file_path);
            return false;
        }
        char child_path[1024];
        while ((dire = readdir(d)) != NULL) {
            if      (streq(dire->d_name, "."))  continue;
            else if (streq(dire->d_name, "..")) continue;
            sprintf(child_path, "%s/%s", file_path, dire->d_name);
            bool child_ok = calculate_file_size(child_path, size);
            if (!child_ok) {
                //shlog(SHLOG_ERROR, "Could not read size of `%s` in directory `%s`", child_path, file_path);
                closedir(d);
                return false;
            }
        }
        closedir(d);
    }
    return true;
}

char *size_units[] = {"", "k", "M", "G"};
int exec_size(Command *cmd)
{
    if (cmd->argc > 1) {
        shlog_too_many_arguments(cmd->name);
        return 1;
    } else if (cmd->flagc > 0) {
        shlog_no_flags_for_this_command(cmd->name);
        return 1;
    }
    char *file_path = cmd->argc == 0 ? working_directory : cmd->argv.items[0];
    size_t size = 0;
    bool ok = calculate_file_size(file_path, &size);
    if (!ok) {
        shlog(SHLOG_ERROR, "Could not determine size of `%s`", file_path);
        return 1;
    }
    int i = 0;
    while (i < 3 && size / 1000 > 1) {
        size /= 1000;
        i++;
    }
    shlog(SHLOG_INFO, "%zu %sB", size, size_units[i]);
    return 0;
}

int exec_mkfl(Command *cmd)
{
    if (cmd->argc == 0) {
        shlog(SHLOG_ERROR, "No path to file provided");
        return 1;
    }
    if (cmd->argc > 1) {
        shlog_too_many_arguments(cmd->name);
        return 1;
    }

    char *file_path = cmd->argv.items[0];
    FILE *f = fopen(file_path, "a");
    if (f == NULL) {
        shlog(SHLOG_ERROR, "Could not create file `%s`", file_path);
        return 1;
    }
    fclose(f);

    return 0;
}

int exec_shed(Command *cmd)
{
    (void)cmd;
    if (cmd->argc > 1) {
        shlog_too_many_arguments(cmd->name);
        return 1;
    }
    if (cmd->argc == 0) shlog(SHLOG_DEBUG, "Opening empty file");
    else shlog(SHLOG_DEBUG, "Opening file `%s`", cmd->argv.items[0]);
    shlog(SHLOG_TODO, "Not implemented yet");
    return 0;
}

// TODO
// - per poter scrivere la stringa `eof` basta scrivere `\eof`
// - per cambiare l'fd da cui leggere / su cui scrivere basta cambiare stdin in getline e f in fprintf (quindi conviene usare i fd)
#define SHEOF "eof"
int exec_file_write(Command *cmd)
{
    if (cmd->argc == 0) {
        shlog(SHLOG_ERROR, "No file name provided");
        shlog_just_use_doc_for_args(cmd->name);
        return 1;
    } else if (cmd->argc > 1) {
        shlog_too_many_arguments(cmd->name);
        return 1;
    }
    char *file_name = cmd->argv.items[0];
    struct stat s;
    if (stat(file_name, &s) != 0) {
        shlog(SHLOG_INFO, "Creating file `%s`", file_name);
    }
    FILE *f = cmd->type == CMD_FILE_WRITE ? fopen(file_name, "w") : fopen(file_name, "a");
    if (f == NULL) {
        shlog(SHLOG_ERROR, "Could not open file `%s`", file_name);
        return 1;
    }
    char *buffer = NULL;
    size_t _ = 0;
    int read = getline(&buffer, &_, stdin);
    size_t len = strlen(buffer);
    if (len > 0) buffer[len-1] = '\0';
    while(read > 0 && !streq(buffer, SHEOF)) {
        if (DEBUG) shlog(SHLOG_DEBUG, "`%s`", buffer);
        fprintf(f, "%s\n", buffer);
        read = getline(&buffer, &_, stdin);
        len = strlen(buffer);
        if (len > 0) buffer[len-1] = '\0';
    }
    if (read <= 0) {
        shlog(SHLOG_ERROR, "Read error");
        shlog(SHLOG_TODO, "Come lo gestisco?");
        return 1;
    } 
    fclose(f);
    return 0;
}

int exec_dump(Command *cmd)
{
    if (cmd->argc == 0) {
        shlog(SHLOG_ERROR, "No file provided");
        shlog_just_use_doc_for_args(cmd->name);
        return 1;
    }
    char *file_name = cmd->argv.items[0];
    FILE *fin = fopen(file_name, "r");
    if (fin == NULL) {
        shlog(SHLOG_ERROR, "Could not open file `%s`. %s.", file_name, strerror(errno));
        errno = 0;
        return 1;
    }

    char *buffer = NULL;
    size_t _size = 0;
    ssize_t read = getline(&buffer, &_size, fin);
    if (cmd->argc == 1) {
        while(read > 0) {
            printf(buffer);
            read = getline(&buffer, &_size, fin);
        }
        if (read == -1 && errno) {
            shlog(SHLOG_ERROR, strerror(errno));
            fclose(fin);
            return 1;
        }
    } else {
        char *operator = cmd->argv.items[1];
        if (DEBUG) shlog(SHLOG_DEBUG, "Operator: %s", operator);
        if (streq(operator, ">")) {
            shlog(SHLOG_TODO, "dump file write: Not implemented yet");
        } else if (streq(operator, ">>")) {
            shlog(SHLOG_TODO, "dump file append: Not implemented yet");
        } else {
            shlog(SHLOG_ERROR, "Unknown operator `%s` for command `%s`", operator, cmd->name);
            shlog_just_use_doc_for_args(cmd->name);
            return 1;
        }
    }
    fclose(fin);
    return 0;
}

// TODO
// x controllo che il primo file esista
// x controllo che il secondo path esista e sia una directory
// - controllo che nel secondo path non esista un file con lo stesso nome del primo
// - eseguo rename con i due path
int exec_move(Command *cmd)
{
    if (cmd->argc < 2) {
        shlog_too_few_arguments(cmd->name);
        return 1;
    } else if (cmd->argc > 2) {
        shlog_too_many_arguments(cmd->name);
        return 1;
    }
    char *old_path = cmd->argv.items[0];
    struct stat st;
    if (stat(old_path, &st) != 0) {
        shlog(SHLOG_ERROR, "File `%s` does not exist", old_path);
        return 1;
    }
    char *new_path = cmd->argv.items[1];
    if (!is_dir(new_path)) {
        shlog(SHLOG_ERROR, "`%s` is not a directory", new_path);
        return 1;
    }

    char *old_name = strchr(old_path, '/');
    if (old_name == NULL) old_name = old_path;
    else old_name++;
    DIR *d;
    d = opendir(new_path);
    if (d == NULL) {
        shlog(SHLOG_FATAL, "Unreachable");
        return 1;
    }
    struct dirent *dire;
    while ((dire = readdir(d)) != NULL) {
        if (streq(old_name, dire->d_name)) {
            shlog(SHLOG_ERROR, "There already exists a file named `%s` in `%s`", old_name, new_path);
            return 1;
        }
    }
    char new_path_complete[1024];
    sprintf(new_path_complete, "%s/%s", new_path, old_name);
    shlog(SHLOG_DEBUG, "Moving `%s` to `%s`", old_path, new_path_complete);
    rename(old_path, new_path_complete);
    return 0;
}

// x estraggo il path del primo nome
// x controllo che il file esista
// x controllo che il secondo nome non presenti '/'
// x controllo che nel path non esista un'altro file/dir con lo stesso nome (conto il numero dei file con lo stesso nome)
// x eseguo rename con gli stessi path
int exec_rename(Command *cmd)
{
    if (cmd->argc < 2) {
        shlog_too_few_arguments(cmd->name);
        return 1;
    } else if (cmd->argc > 2) {
        shlog_too_many_arguments(cmd->name);
        return 1;
    }

    char *old_name = cmd->argv.items[0];
    struct stat st;
    if (stat(old_name, &st) != 0) {
        shlog(SHLOG_ERROR, "File `%s` does not exist", old_name);
        return 1;
    }
    char *new_name = cmd->argv.items[1];
    if (strchr(new_name, '/') != NULL) {
        shlog(SHLOG_ERROR, "The new name cannot contain '/' (`%s`)", new_name);
        return 1;
    }
    bool this_dir = false;
    char *path = strdup(old_name);
    size_t len = strlen(path);
    while (len > 0 && *(path+len-1) != '/')
        len--;
    if (len > 0) path[len] = '\0';
    else this_dir = true;
    char new_path[1024];
    sprintf(new_path, "%s/%s", this_dir ? "." : path, new_name);

    if (!is_dir(this_dir ? "." : path)) {
        shlog(SHLOG_FATAL, "Bug in rename, trying `%s` -> `%s`", old_name, new_path);
        free(path);
        return 1;
    } 
    DIR *d;
    d = opendir(this_dir ? "." : path);
    if (d == NULL) {
        shlog(SHLOG_ERROR, "`%s` is not a directory or does not exist", this_dir ? "." : path);
        free(path);
        return 1;
    }
    struct dirent *dire;
    while ((dire = readdir(d)) != NULL) {
        if (streq(new_name, dire->d_name)) {
            shlog(SHLOG_ERROR, "There already exists a file named `%s` in `%s`", new_name, this_dir ? "this directory" : path);
            free(path);
            return 1;
        }
    }
    rename(old_name, new_path);
    free(path);
    return 0;
}

int main(void)
{
    Start();

    char *line = NULL;
    size_t line_len = 0;
    int len = 0;
    int read;
    int cmd_n = 0;
    bool isCommandValid;
    Command command = {0};
    while (exit_code == EXIT_NOT_SET)
    {
        char prompt_buf[1024];
        sprintf(prompt_buf, "%s/ %s", working_directory, PROMPT);
        print_fg_text(prompt_buf, PROMPT_COLOR);
        read = getline(&line, &line_len, stdin);
        if (read <= 0) {
            if (signalno != 0) {
                exit_code = EXIT_SIGNAL;
            } else {
                exit_code = EXIT_READ_ERROR;
            }
            break;
        }
        len = strlen(line) - 1;
        if (len == 0) continue;
        line[len] = '\0';
        cmd_n++;
        if (DEBUG) shlog(SHLOG_DEBUG, "%d (%u): '%s'", cmd_n, len, line);
        ArrayOfStrings words = {0};
        int word_count = count_words(line);
        aos_init(&words, word_count);
        tokenize_string(line, &words);
        Command command;
        isCommandValid = words_to_command(words, &command);
        aos_free(&words);
        if (isCommandValid) {
            if (DEBUG) cmd_print(command);
            switch (command.type)
            {
                case CMD_DOC:         exec_doc(&command);         break;
                case CMD_ECHO:        exec_echo(&command);        break;
                case CMD_LS:          exec_ls(&command);          break;
                case CMD_CD:          exec_cd(&command);          break;
                case CMD_PWD:         exec_pwd();                 break;
                case CMD_MKDIR:       exec_mkdir(&command);       break;
                case CMD_RM:          exec_rm(&command);          break;
                case CMD_QUIT:        exec_quit();                break;
                case CMD_CLEAR:       exec_clear();               break;
                case CMD_SL:          exec_sl();                  break;
                case CMD_SIZE:        exec_size(&command);        break;
                case CMD_MKFL:        exec_mkfl(&command);        break;
                case CMD_SHED:        exec_shed(&command);        break;
                case CMD_FILE_WRITE:  exec_file_write(&command);  break;
                case CMD_FILE_APPEND: exec_file_write(&command);  break;
                case CMD_DUMP:        exec_dump(&command);        break;
                case CMD_MOVE:        exec_move(&command);        break;
                case CMD_RENAME:      exec_rename(&command);      break;
                case CMDTYPES_COUNT: shlog(SHLOG_WARNING, "`%s` command is not implemented yet.", command.name); break;
                case CMD_UNKNOWN:
                default: shlog(SHLOG_FATAL, "Unreachable"); abort();
            }
            cmd_free(&command);
        }
    }

    if (exit_code == EXIT_OK) shlog(SHLOG_INFO, "Shell exited successfully with code 0");
    else {
        switch (exit_code)
        {
            case EXIT_OK:         shlog(SHLOG_FATAL, "Unreachable");                                     break;
            case EXIT_NOT_SET:    shlog(SHLOG_FATAL, "This is a bug in the shell");                      break;
            case EXIT_READ_ERROR: shlog(SHLOG_FATAL, "Could not read input. Please restart the shell."); break;
            case EXIT_SIGNAL:                                                                            break;
            default:              shlog(SHLOG_FATAL, "Unhandled exit code %d", exit_code);               break;
        }
        shlog(SHLOG_ERROR, "Shell exited abnormally with code %d", exit_code);
    }
    if (isCommandValid) cmd_free(&command);
    return 0;
}
