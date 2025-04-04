/* TODO
    Comandi da implementare:
    - grep (devo solo fare un parser per le regexp 👍🏼)
    - get and set env variables
    - !v verbose external command

    Urgent stack:
    - supporto per ~ ovunque
    - ? coda dei errno, oppure ogni controllo stampa gia' l'errore e non devo preoccuparmene, pero' devo decidere
    - aggiustare i segnali

    Altre cose da fare:
    - usare ctrl+D per terminare la lettura da stdin
    - creare una cartella trash in cui vanno le cose eliminate con rm
    - permissions (mostrarle in ls, anche con colori diversi dei file; se si vuole eseguire un'operazione per cui non si hanno i permessi bisogna segnalarlo all'utente)
    - previous and next command
    > probabilmente devo tenere un buffer che poi verra' ridirezionato (solitamente verso stdin)
    - config file che permette di modificare i colori, il path del trash, cose di ed e chissa' cos'altro
    - correction if a similar command is found
    - autocompletion by tabbing (chissa' se e' possible)
    - capire come usare alt+key e ctrl+key

    Idee ambiziose:
    - ed text editor https://www.gnu.org/software/ed/manual/ed_manual.html
    - neolinty (il mio neovim)
    - scripting language (!#/path/to/ashell)
*/

#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <dirent.h>

#include "ashed.h"

#define SHDEBUG false

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
char working_dir[512];
char root_dir[512];
//char output[1024]; // TODO: da pensare
///////////////////////////////////////////

#define PROMPT "=> "
void print_prompt()
{
    char prompt_buf[1024];
    if (strncmp(working_dir, root_dir, strlen(root_dir)) == 0) {
        sprintf(prompt_buf, "~%s %s", working_dir+strlen(root_dir), PROMPT);
    } else {
        sprintf(prompt_buf, "%s %s", working_dir, PROMPT);
    }
    print_fg_text(prompt_buf, PROMPT_COLOR);
}

void sigint_handler(int signo) {
    signalno = signo;
    printf("\n"); // Newline after ^C
    shlog(SHLOG_WARNING, "Command has been interrupted");
}

void test_shlog_levels()
{
    char *str = "prova";
    shlog(SHLOG_INFO, str);
    shlog(SHLOG_DOC, str);
    shlog(SHLOG_USAGE, str);
    shlog(SHLOG_FLAGS, str);
    shlog(SHLOG_TODO, str);
    shlog(SHLOG_DEBUG, str);
    shlog(SHLOG_WARNING, str);
    shlog(SHLOG_ERROR, str);
    shlog(SHLOG_FATAL, str);
}

void Start()
{
    exit_code = EXIT_NOT_SET;
    signalno = 0;
    getcwd(working_dir, sizeof(working_dir));
    sprintf(root_dir, getenv("HOME"));
    if (SHDEBUG) shlog(SHLOG_DEBUG, "Working in %s", working_dir);
    if (SHDEBUG) shlog(SHLOG_DEBUG, "Root: %s", root_dir);

    //test_shlog_levels();

    struct sigaction sigint_action = { .sa_handler = sigint_handler };
    sigaction(SIGINT, &sigint_action, NULL);
    sigaction(SIGQUIT, NULL, NULL);
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
    CMD_ASHED,
    CMD_FILE_WRITE,
    CMD_FILE_APPEND,
    CMD_DUMP,
    CMD_MOVE,
    CMD_RENAME,
    CMD_EXTERNAL,
    CMDTYPES_COUNT 
} CmdType;
static_assert(CMD_UNKNOWN==0 && CMD_DOC==1 && CMD_ECHO==2 && CMD_LS==3 && CMD_CD==4 && CMD_PWD==5 && CMD_MKDIR==6 && CMD_RM==7 && CMD_QUIT==8 && CMD_CLEAR==9 && CMD_SL==10 &&
              CMD_SIZE==11 && CMD_MKFL==12 && CMD_ASHED==13 && CMD_FILE_WRITE == 14 && CMD_FILE_APPEND == 15 && CMD_DUMP==16 && CMD_MOVE==17 && CMD_RENAME==18 && CMD_EXTERNAL==19 && CMDTYPES_COUNT==20, "Order of commands is preserved"); 

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
            char *arg = cmd.argv.items[i];
            if (arg != NULL) printf(arg);
            else printf("(null)");
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

CmdType getCmdType(char *name)
{
    static_assert(CMDTYPES_COUNT == 19+1, "Exhaustive parsed command types in getCmdType");
    if      (streq(name, "doc"))                                                 return CMD_DOC;
    if      (streq(name, "echo"))                                                return CMD_ECHO;
    else if (streq(name, "quit")  || streq(name, "q"))                           return CMD_QUIT;
    else if (streq(name, "ls"))                                                  return CMD_LS;
    else if (streq(name, "sl"))                                                  return CMD_SL;
    else if (streq(name, "cd"))                                                  return CMD_CD;
    else if (streq(name, "pwd"))                                                 return CMD_PWD;
    else if (streq(name, "mkdir"))                                               return CMD_MKDIR;
    else if (streq(name, "rm"))                                                  return CMD_RM;
    else if (streq(name, "clear") || streq(name, "c"))                           return CMD_CLEAR;
    else if (streq(name, "size") || streq(name, "sz"))                           return CMD_SIZE;
    else if (streq(name, "mkfl"))                                                return CMD_MKFL;
    else if (streq(name, "ashed") || streq(name, "shed") || streq(name, "ed"))   return CMD_ASHED;
    else if (streq(name, ">"))                                                   return CMD_FILE_WRITE;
    else if (streq(name, ">>"))                                                  return CMD_FILE_APPEND;
    else if (streq(name, "dump") || streq(name, "dp"))                           return CMD_DUMP;
    else if (streq(name, "move") || streq(name, "mv"))                           return CMD_MOVE;
    else if (streq(name, "rename") || streq(name, "rn"))                         return CMD_RENAME;
    else if (streq(name, "!"))                                                   return CMD_EXTERNAL;
    else                                                                         return CMD_UNKNOWN;
}

bool words_to_command(ArrayOfStrings words, Command *cmd)
{
    if (words.count == 0) {
        shlog(SHLOG_FATAL, "no command provided");
        return false;
    }

    cmd->name = strdup(words.items[0]);
    cmd->type = getCmdType(words.items[0]);
    if (cmd->type == CMD_UNKNOWN) {
        shlog_unknown_command(cmd->name);
        return false;
    }

    if (cmd->type != CMD_EXTERNAL) {
        int argc = 0;
        int flagc = 0;
        for (int i = 1; i < words.count; i++) {
            if (words.items[i][0] == '-') flagc++;
            else argc++;
        }

        cmd->argc = argc;
        if (argc > 0) aos_init(&(cmd->argv), argc);
        int arg_i = 0;

        cmd->flagc = flagc;
        if (flagc > 0) aos_init(&(cmd->flagv), flagc);
        int flag_i = 0;

        int word_i = 1;
        while (word_i <= argc + flagc) {
            char *word = words.items[word_i];
            if (SHDEBUG) shlog(SHLOG_DEBUG, "%d: %s", word_i, word);
            if (word[0] == '-') {
                cmd->flagv.items[flag_i] = strdup(word);
                flag_i++;
            } else {
                cmd->argv.items[arg_i] = strdup(word);
                arg_i++;
            }
            word_i++;
        }
    } else {
        cmd->argc = words.count;
        aos_init(&(cmd->argv), words.count);
        for (int i = 1; i < words.count; i++) {
            cmd->argv.items[i-1] = words.items[i];    
        }
        cmd->argv.items[words.count-1] = NULL;
    }

    if (SHDEBUG) cmd_print(*cmd);

    return true;
}

// Usage, flags and doc ///////////////////////////
static_assert(CMDTYPES_COUNT == 19+1, "Exhaustive usage for all commands");
#define USAGE_DOC         "doc [COMMAND] [...FLAGS]"
#define USAGE_ECHO        "echo <...ARGS>"
#define USAGE_LS          "ls [PATH | .] [...FLAGS]"
#define USAGE_CD          "cd <DIR>"
#define USAGE_PWD         "pwd"
#define USAGE_MKDIR       "mkdir <DIR_PATH>"
#define USAGE_RM          "rm <FILE_PATH>"
#define USAGE_CLEAR       "clear (c)"
#define USAGE_QUIT        "quit (q)"
#define USAGE_SIZE        "size (sz) [PATH | .]"
#define USAGE_MKFL        "mkfl <FILE_PATH>"
#define USAGE_ASHED       "ashed (shed or ed) [FILE_PATH | ./temped#]"
#define USAGE_FILE_WRITE  "> <FILE> [INPUT | stdin]"
#define USAGE_FILE_APPEND ">> <FILE> [INPUT | stdin]"
#define USAGE_DUMP        "dump (dp) <SRC> [[OPERATOR | >] [...DST]]"
#define USAGE_MOVE        "move (mv) <OLD_PATH> <NEW_PATH>"
#define USAGE_RENAME      "rename (rn) <PATH/TO/OLD/NAME> <NEW_NAME>"
#define USAGE_EXTERNAL    "! <CMD>"

#define NO_FLAGS "No flags for this command."
static_assert(CMDTYPES_COUNT == 19+1, "Exhaustive flags for all commands");
#define FLAGS_DOC         ("-d\t\tPrint Documentation for the shell\n"\
                           "-c\t\tPrint list of available Commands\n"\
                           "<COMMAND> -d\tPrint Doc info for COMMAND\n"\
                           "<COMMAND> -u\tPrint Usage info for COMMAND\n"\
                           "<COMMAND> -f\tPrint Flags info for COMMAND\n")
#define FLAGS_LS          "-h\tShow Hidden files"
#define FLAGS_CD          NO_FLAGS
#define FLAGS_PWD         NO_FLAGS
#define FLAGS_MKDIR       NO_FLAGS
#define FLAGS_RM          NO_FLAGS
#define FLAGS_ECHO        NO_FLAGS
#define FLAGS_CLEAR       NO_FLAGS
#define FLAGS_QUIT        NO_FLAGS
#define FLAGS_SIZE        NO_FLAGS
#define FLAGS_MKFL        NO_FLAGS
#define FLAGS_ASHED       NO_FLAGS
#define FLAGS_FILE_WRITE  NO_FLAGS
#define FLAGS_FILE_APPEND NO_FLAGS
#define FLAGS_DUMP        NO_FLAGS
#define FLAGS_MOVE        NO_FLAGS
#define FLAGS_RENAME      NO_FLAGS
#define FLAGS_EXTERNAL    NO_FLAGS

static_assert(CMDTYPES_COUNT == 19+1, "Exhaustive doc string for all commands");
#define DOC_DOC         "Print the documentation for COMMAND. If none print the documentation of the shell and the documentation for all the commands."
#define DOC_LS          "List PATH content. If none list current directory content."
#define DOC_CD          "Change the working directory to DIR."
#define DOC_PWD         "Print working directory."
#define DOC_MKDIR       "Create the directory at DIR_PATH."
#define DOC_RM          "Remove the file located at FILE_PATH."
#define DOC_ECHO        "Print ARGS on the terminal."
#define DOC_CLEAR       "Clear the screen of the terminal."
#define DOC_QUIT        "Exit the shell safely."
#define DOC_SIZE        "Print the size of PATH. If none, print the size of the current directory."
#define DOC_MKFL        "Create the file at FILE_PATH."
#define DOC_ASHED       "Open FILE_PATH with the builtin `ashed` text editor. If none open temped#, an empty unsaved numbered (#) file, in the working directory"
#define DOC_FILE_WRITE  "Write INPUT (default is stdin) to FILE. FILE will be overwritten."
#define DOC_FILE_APPEND "Append INPUT (default is stdin) to FILE."
#define DOC_DUMP        "Dump FILE: TODO..."
#define DOC_MOVE        "Move the file or directory in OLD_PATH to NEW_PATH. Does not change names, if you want to do so use `rename`."
#define DOC_RENAME      "Rename the file at PATH/TO/OLD/NAME to NEW_NAME."
#define DOC_EXTERNAL    "execute external command CMD"

static_assert(CMDTYPES_COUNT == 19+1, "Exhaustive documentation");
#define DOC ("DOCUMENTATION\n \n"\
             "Syntax:\n "\
             "    Command usage uses the following syntax:\n"\
             "    - some\t\tsome is a simple name, can be anything and is often the name of the command at the beginning of the usage.\n"\
             "    - (some)\t\tis an alternative, often shorter, version of a command. e.g. clear (c) means `clear` and `c` will execute the same command.\n"\
             "    - <some>\t\tsome in <> is mandatory.\n"\
             "    - [some | default]\t\tsome in [] is optional, if not specified it will take the default value.\n"\
             "    - ...some\t\tsome is a list (e.g. [...args] is an optional list of arguments)\n"\
             "    - -some\t\tsome is a flag (see `Flags` section).\n"\
             " \n"\
             "Commands:\n "\
             "    Commands have a name, a series of arguments and a series of flags: name [...args] [...flags]\n"\
             "    - Command with no arguments:   name\n"\
             "    - Command with one argument:   name arg\n"\
             "    - Command with more arguments: name arg1 arg2\n"\
             " \n"\
             "Flags:\n "\
             "    Flags ought to go after the arguments of the command.\n"\
             "    - Flags with no arguments:        -flag\n"\
             "    - Flags with one argument:        -flag=arg\n"\
             "    - Flags with more arguments:      -flag=arg1,arg2\n"\
             "    - Flags arguments can have flags: -flag=[arg1 -f1,arg2 -f2]"\
     )

static_assert(CMDTYPES_COUNT == 19+1, "Exhaustive commands list");
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
                    "  - ashed   \t" DOC_ASHED "\n"\
                    "  - >       \t" DOC_FILE_WRITE "\n"\
                    "  - >>      \t" DOC_FILE_APPEND "\n"\
                    "  - dump    \t" DOC_DUMP "\n"\
                    "  - move    \t" DOC_MOVE "\n"\
                    "  - rename  \t" DOC_RENAME "\n"\
                    "  - !       \t" DOC_EXTERNAL "\n"\
                    "------------------------------------------------------------------------------------------------------\n \n"\
                    "  Screen management:\n"\
                    "------------------------------------------------------------------------------------------------------\n"\
                    "  - clear   \t" DOC_CLEAR "\n"\
                    "------------------------------------------------------------------------------------------------------\n \n"\
                    "  Quitting out of the shell:\n"\
                    "------------------------------------------------------------------------------------------------------\n"\
                    "  - quit    \t" DOC_QUIT  "\n"\
                    "  - `ctrl+D`\tExit the shell with a SIGQUIT.\n"\
                    "------------------------------------------------------------------------------------------------------"\
        )
typedef struct
{
    char *doc;
    char *usage;
    char *flags;
} DocStrings;

static_assert(CMDTYPES_COUNT == 19+1, "Exhaustive doc_strings");
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
    { .doc=DOC_ASHED,   .usage=USAGE_ASHED,   .flags=FLAGS_ASHED },                   // ashed,
    { .doc=DOC_FILE_WRITE,   .usage=USAGE_FILE_WRITE,   .flags=FLAGS_FILE_WRITE },    // >,
    { .doc=DOC_FILE_APPEND,   .usage=USAGE_FILE_APPEND,   .flags=FLAGS_FILE_APPEND }, // >>,
    { .doc=DOC_DUMP,   .usage=USAGE_DUMP,   .flags=FLAGS_DUMP },                      // dump,
    { .doc=DOC_MOVE,   .usage=USAGE_MOVE,   .flags=FLAGS_MOVE },                      // move,
    { .doc=DOC_RENAME,   .usage=USAGE_RENAME,   .flags=FLAGS_RENAME },                // rename,
    { .doc=DOC_EXTERNAL,   .usage=USAGE_EXTERNAL,   .flags=FLAGS_EXTERNAL },          // external,
    {}                                                                                // cmdtypes_count 
};
//////////////////////////////////////////////////

#define DOC_FLAG_DOC   (1 << 3)
#define DOC_FLAG_CMDS  (1 << 2)
#define DOC_FLAG_USAGE (1 << 1)
#define DOC_FLAG_FLAGS (1 << 0)
#define DOC_FLAG_ALL (DOC_FLAG_DOC | DOC_FLAG_CMDS | DOC_FLAG_USAGE | DOC_FLAG_FLAGS)

int exec_doc(Command cmd)
{
    CHECK_TOO_MANY_ARGUMENTS(cmd, 1);

    int doc_flags = cmd.flagc == 0 ? DOC_FLAG_ALL : 0;

    for (int i = 0; i < cmd.flagc; i++) {
        char *flag = cmd.flagv.items[i];
        if (streq(flag, "-d")) doc_flags |= DOC_FLAG_DOC; 
        else if (cmd.argc == 0) {
            if (streq(flag, "-c")) doc_flags |= DOC_FLAG_CMDS; 
            else {
                shlog_unknown_flag(flag, cmd.name);
                return 1;
            }
        } else {
            if      (streq(flag, "-u")) doc_flags |= DOC_FLAG_USAGE;
            else if (streq(flag, "-f")) doc_flags |= DOC_FLAG_FLAGS;
            else {
                shlog_unknown_flag_for_single_arg(flag, cmd.name);
                return 1;
            }
        }
    }

    if (cmd.argc == 0) {
        if (doc_flags & DOC_FLAG_DOC)  shlog(SHLOG_DOC, DOC);
        if (doc_flags & (~DOC_FLAG_DOC & ~DOC_FLAG_CMDS)) shlog(SHLOG_DOC, " \n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
        if (doc_flags & DOC_FLAG_CMDS) shlog(SHLOG_DOC, CMDS_LIST);
    } else {
        char *cmd_to_doc_name = cmd.argv.items[0];
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
    }
    return 0;
}

int exec_echo(Command cmd)
{
    for (int i = 0; i < cmd.argc; i++) printf("%s ", cmd.argv.items[i]);
    printf("\n");
    return 0;
}

int exec_pwd() { printf("%s\n", working_dir); return 0; }

int exec_quit() { exit_code = EXIT_OK; return 0; }

int exec_clear() { printf("\e[1;1H\e[2J"); return 0; }

int exec_sl() { printf("Non annusarmi l'ashell.\n"); return 0; }

// TODO:
// - flag per la ricerca
// - Cose da stampare (che forse diventeranno flags):
//   > dimensiona totale della cartella
//   > dimensione di ogni file
//   > permessi dei file?
int exec_ls(Command cmd)
{
    CHECK_TOO_MANY_ARGUMENTS(cmd, 1);

    char path[1024];
    if (cmd.argc == 0) sprintf(path, working_dir);
    else {
        char *path_arg = cmd.argv.items[0];
        if (streq(path_arg, ".") || streq(path_arg, "./")) sprintf(path, working_dir);
        else if (path_arg[0] == '~') sprintf(path, "%s/%s", root_dir, path_arg+1);
        else sprintf(path, path_arg);
    }

    bool ls_flag_show_hidden_files = false;

    for (int i = 0; i < cmd.flagc; i++) {
        char *flag = cmd.flagv.items[i];
        if (streq(flag, "-h")) ls_flag_show_hidden_files = true;
        else {
            shlog_unknown_flag(flag, cmd.name);
            return 1;
        }
    }
    if (!is_dir(path)) {
        shlog_not_a_dir(path);
        return 1;
    }

    DIR *d;
    struct dirent *dire;
    d = opendir(path);
    if (d == NULL) {
        shlog(SHLOG_ERROR, "Could not open directory `%s`", path);
        return 1;
    }
    char file_path[2048];
    size_t file_count = 0;
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
            } else {
                shlog_error_and_reset_errno(NULL);
            }
            printn_fg_text(dire->d_name, color);
        }
        file_count++;
    }
    if (file_count == 0) shlog(SHLOG_INFO, "Empty directory `%s`", path);
    else shlog(SHLOG_INFO, "Count: %d", file_count);

    closedir(d);
    return 0;
}

int exec_cd(Command cmd)
{
    CHECK_TOO_MANY_ARGUMENTS(cmd, 1);

    char new_dir[1024];
    if (cmd.argc == 0) sprintf(new_dir, root_dir);
    else {
        char *new_dir_arg = cmd.argv.items[0];
        if (new_dir_arg[0] == '~') sprintf(new_dir, "%s/%s", root_dir, new_dir_arg+1);
        else sprintf(new_dir, new_dir_arg);
    }

    if (SHDEBUG) shlog(SHLOG_DEBUG, "Current working directory: %s", working_dir);
    if (!is_dir(new_dir)) {
        shlog_not_a_dir(new_dir);
        return 1;
    }
    chdir(new_dir);
    getcwd(working_dir, sizeof(working_dir));
    if (SHDEBUG) shlog(SHLOG_DEBUG, "New working directory: %s", working_dir);
    return 0;
}

// TODO: recursive creation flag
int exec_mkdir(Command cmd)
{
    if (cmd.argc < 1) {
        shlog(SHLOG_ERROR, "Directory path has not been provided.");
        shlog(SHLOG_USAGE, USAGE_MKDIR);
        return 1;
    }
    for (int i = 0; i < cmd.argc; i++)
        mkdir(cmd.argv.items[i], 0777);
    return 0;
}

int exec_rm(Command cmd)
{
    bool rm_flag_recursive = false;
    if (cmd.argc == 0) {
        shlog(SHLOG_ERROR, "File path has not been provided.");
        shlog(SHLOG_USAGE, USAGE_RM);
        return 1;
    }
    for (int i = 0; i < cmd.flagc; i++) {
        char *flag = cmd.flagv.items[i];
        if (streq(flag, "-r")) rm_flag_recursive = true;
        else {
            shlog_unknown_flag(flag, cmd.name);
            return 1;
        }
    }
    int err = 0;
    for (int i = 0; i < cmd.argc; i++) {
        if (streq(cmd.argv.items[i], "-r")) continue;

        if (remove(cmd.argv.items[i]) != 0) {
            if (is_dir(cmd.argv.items[i])) {
                if (rm_flag_recursive) shlog(SHLOG_WARNING, "flag -r is not yet implemented.");
                else shlog(SHLOG_ERROR, "Could not remove not empty directory `%s`. Consider using recursive flag -r.", cmd.argv.items[i]);
            } else shlog(SHLOG_ERROR, "Could not remove file `%s`.", cmd.argv.items[i]);
            err = 1;
        }
    }
    return err;
}

bool calculate_file_size(char *file_path, size_t *size)
{
    shlog(SHLOG_TODO, "Ovviamente non funziona ancora");
    return false;

    if (SHDEBUG) shlog_NO_NEWLINE(SHLOG_DEBUG, "Calculating size of `%s`", file_path);
    struct stat st;
    if (stat(file_path, &st) != 0) {
        shlog(SHLOG_ERROR, "Could not open `%s`", file_path);
        return false;
    }
    *size += st.st_size;
    if (SHDEBUG) printf(": `%zu`\n", st.st_size);
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
int exec_size(Command cmd)
{
    CHECK_TOO_MANY_ARGUMENTS(cmd, 1);

    if (cmd.flagc > 0) {
        shlog_no_flags_for_this_command(cmd.name);
        return 1;
    }
    char *file_path = cmd.argc == 0 ? working_dir : cmd.argv.items[0];
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

int exec_mkfl(Command cmd)
{
    if (cmd.argc == 0) {
        shlog(SHLOG_ERROR, "No path to file provided");
        return 1;
    }
    CHECK_TOO_MANY_ARGUMENTS(cmd, 1);

    char *file_path = cmd.argv.items[0];
    FILE *f = fopen(file_path, "a");
    if (f == NULL) {
        shlog(SHLOG_ERROR, "Could not create file `%s`", file_path);
        return 1;
    }
    fclose(f);

    return 0;
}

// TODO: WIP
int exec_ashed(Command cmd)
{
    shlog(SHLOG_WARNING, "No longer supported");
    CHECK_TOO_MANY_ARGUMENTS(cmd, 1);
    char *filename = NULL;
    if (cmd.argc == 1) filename = cmd.argv.items[0];
    int ashed_exit_code = ashed_main(filename);
    shlog(SHLOG_TODO, "Handle ashed exit code %d", ashed_exit_code);
    return 0;
}

// TODO
// - per cambiare l'fd da cui leggere / su cui scrivere basta cambiare stdin in getline e f in fprintf (quindi conviene usare i fd)
#define SHEOF "eof"
int exec_file_write(Command cmd)
{
    CHECK_TOO_MANY_ARGUMENTS(cmd, 1);

    if (cmd.argc == 0) {
        shlog(SHLOG_ERROR, "No file name provided");
        shlog_just_use_doc_for_args(cmd.name);
        return 1;
    }

    char *file_name = cmd.argv.items[0];
    if (!does_file_exist(file_name)) {
        shlog(SHLOG_INFO, "Creating file `%s`", file_name);
    }
    FILE *f = cmd.type == CMD_FILE_WRITE ? fopen(file_name, "w") : fopen(file_name, "a");
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
        if (SHDEBUG) shlog(SHLOG_DEBUG, "`%s`", buffer);
        if (streq(buffer, "\\eof")) fprintf(f, "eof\n");
        else fprintf(f, "%s\n", buffer);
        free(buffer);
        buffer = NULL;
        read = getline(&buffer, &_, stdin);
        len = strlen(buffer);
        if (len > 0) buffer[len-1] = '\0';
    }
    if (read <= 0) {
        shlog(SHLOG_ERROR, "Read error");
        shlog(SHLOG_TODO, "Come lo gestisco?");
        free(buffer);
        return 1;
    } 
    free(buffer);
    fclose(f);
    return 0;
}

// - dump file (stampa il contenuto del file sul terminale)
// - dump file > (come sopra)
// - dump file > out (come sopra)
// - dump file1 > file2 (sovrascrive file2 con il contenuto di file1) (sarebbe cp, potrei fare degli alias)
// - dump file1 >> file2 (appende il contenuto di file1 a file2)      (sarebbe cat, potrei fare degli alias)
// > se file2 non esiste viene creato -> TODO: chiedere se crearlo oppure no
int exec_dump(Command cmd)
{
    CHECK_TOO_MANY_ARGUMENTS(cmd, 3);

    if (cmd.argc == 0) {
        shlog(SHLOG_ERROR, "No file provided");
        shlog_just_use_doc_for_args(cmd.name);
        return 1;
    }

    char *fin_name = cmd.argv.items[0];
    if (!does_file_exist(fin_name)) {
        shlog_file_does_not_exist(fin_name);
        return 1;
    }
    if (is_dir(fin_name)) {
        shlog(SHLOG_ERROR, "Cannot dump `%s`, it's a directory.", fin_name);
        return 1;
    }
    FILE *fin = fopen(fin_name, "r");
    if (fin == NULL) {
        shlog_error_and_reset_errno(NULL);
        return 1;
    }
    if (SHDEBUG) shlog(SHLOG_DEBUG, "Dump input: %s", fin_name);

    FILE *fout = NULL;
    char *options = NULL;
    if (cmd.argc == 1) {
        fout = stdin;
        if (SHDEBUG) shlog(SHLOG_DEBUG, "Dump output: stdin");
    } else {
        char *operator = cmd.argv.items[1];
        if (SHDEBUG) shlog(SHLOG_DEBUG, "Operator: %s", operator);
        if (streq(operator, ">")) {
            options = "wb";
        } else if (streq(operator, ">>")) {
            options = "ab";
        } else {
            shlog(SHLOG_ERROR, "Unknown operator `%s` for command `%s`", operator, cmd.name);
            shlog_just_use_doc_for_args(cmd.name);
            return 1;
        }

        char *fout_name = cmd.argv.items[2];
        if (!does_file_exist(fout_name)) {
            shlog(SHLOG_INFO, "Creating file `%s`", fout_name);
        }
        if (is_dir(fout_name)) {
            shlog(SHLOG_ERROR, "Cannot dump into `%s`, it's a directory.", fout_name);
            return 1;
        }
        fout = fopen(fout_name, options);
        if (fout == NULL) {
            shlog_error_and_reset_errno(NULL);
            return 1;
        }
        if (SHDEBUG) shlog(SHLOG_DEBUG, "Dump output: %s", fout_name);
    }
    if (SHDEBUG) shlog(SHLOG_DEBUG, "Dump options: %s", options == NULL ? "none" : options);

    const int BUF_CAP = 1024;
    char buffer[1024] = {0};
    char *line;
    int i = 0;
    if (SHDEBUG) shlog(SHLOG_DEBUG, "Input file to read from: %p", fin);
    while((line = fgets(buffer, BUF_CAP, fin)) != NULL) {
        if (fout == stdin) printf(line);
        else fprintf(fout, line);
        i++;
    }
    if (!feof(fin)) {
        shlog(SHLOG_ERROR, strerror(ferror(fin)));
        fclose(fin);
        if (fout != stdin && fout != NULL) fclose(fout); 
        return 1;
    }
    shlog(SHLOG_INFO, "End of file reached: wrote %d lines", i);

    fclose(fin);
    if (fout != stdin && fout != NULL) fclose(fout);
    return 0;
}

// - controllo che il primo file esista
// - controllo che il secondo path esista e sia una directory
// - controllo che nel secondo path non esista un file con lo stesso nome del primo
// - eseguo rename con i due path
int exec_move(Command cmd)
{
    CHECK_TOO_FEW_ARGUMENTS(cmd, 2);
    CHECK_TOO_MANY_ARGUMENTS(cmd, 2);

    char *old_path = cmd.argv.items[0];
    struct stat st;
    if (stat(old_path, &st) != 0) {
        shlog(SHLOG_ERROR, "File `%s` does not exist", old_path);
        return 1;
    }
    char *new_path = cmd.argv.items[1];
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
    rename(old_path, new_path_complete);
    return 0;
}

// - estraggo il path del primo nome
// - controllo che il file esista
// - controllo che il secondo nome non presenti '/'
// - controllo che nel path non esista un'altro file/dir con lo stesso nome (conto il numero dei file con lo stesso nome)
// - eseguo rename con gli stessi path
int exec_rename(Command cmd)
{
    CHECK_TOO_FEW_ARGUMENTS(cmd, 2);
    CHECK_TOO_MANY_ARGUMENTS(cmd, 2);

    char *old_name = cmd.argv.items[0];
    struct stat st;
    if (stat(old_name, &st) != 0) {
        shlog(SHLOG_ERROR, "File `%s` does not exist", old_name);
        return 1;
    }
    char *new_name = cmd.argv.items[1];
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

int exec_external(Command cmd)
{
    pid_t child = fork();
    switch (child)
    {
        case 0:
            if (execvp(cmd.argv.items[0], cmd.argv.items) != -1) return EXIT_SUCCESS;
            else {
                shlog(SHLOG_ERROR, "Error executing external command");
                return 1;
            }
        case -1:
            if (SHDEBUG) perror("fork");
            shlog(SHLOG_FATAL, "Could not execute external command");
            exit(EXIT_FAILURE);
        default:
            int wexit;
            struct rusage rusage;
            pid_t wpid;

            do {
                wpid = wait4(child, &wexit, 0, &rusage);
                if (wpid == -1) {
                    if (SHDEBUG) perror("waitpid");
                    shlog(SHLOG_FATAL, "Could not execute external command");
                    exit(EXIT_FAILURE);
                }

                if (WIFEXITED(wexit)) {
                    int code = WEXITSTATUS(wexit);
                    if (code == EXIT_SUCCESS) shlog(SHLOG_INFO, "External command terminated with code %d", EXIT_SUCCESS);
                    else shlog(SHLOG_ERROR, "External command terminated with code %d", code);
                } else if (WIFSIGNALED(wexit)) {
                    shlog(SHLOG_TODO, "WIFSIGNALED: not implemented");    
                } else if (WIFSTOPPED(wexit)) {
                    shlog(SHLOG_TODO, "WIFSTOPPED: not implemented");    
                } else if (WIFCONTINUED(wexit)) {
                    shlog(SHLOG_TODO, "WIFCONTINUED: not implemented");    
                }
            } while (!WIFEXITED(wexit) && !WIFSIGNALED(wexit));
    }
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
        print_prompt();
        read = getline(&line, &line_len, stdin);
        if (feof(stdin)) {
            exit_code = EXIT_OK;
            break;
        } else if (read <= 0) {
            if (SHDEBUG) shlog(SHLOG_DEBUG, "What's the matter? %s", strerror(errno));
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
        if (SHDEBUG) shlog(SHLOG_DEBUG, "%d (%u): '%s'", cmd_n, len, line);
        ArrayOfStrings words = {0};
        int word_count = count_words(line);
        aos_init(&words, word_count);
        tokenize_string(line, &words);
        free(line);
        line = NULL;
        Command command;
        isCommandValid = words_to_command(words, &command);
        aos_free(&words);
        if (isCommandValid) {
            if (SHDEBUG) cmd_print(command);
            switch (command.type)
            {
                case CMD_DOC:         exec_doc(command);         break;
                case CMD_ECHO:        exec_echo(command);         break;
                case CMD_LS:          exec_ls(command);          break;
                case CMD_CD:          exec_cd(command);          break;
                case CMD_PWD:         exec_pwd();                 break;
                case CMD_MKDIR:       exec_mkdir(command);       break;
                case CMD_RM:          exec_rm(command);          break;
                case CMD_QUIT:        exec_quit();                break;
                case CMD_CLEAR:       exec_clear();               break;
                case CMD_SL:          exec_sl();                  break;
                case CMD_SIZE:        exec_size(command);        break;
                case CMD_MKFL:        exec_mkfl(command);        break;
                case CMD_ASHED:        exec_ashed(command);      break;
                case CMD_FILE_WRITE:  exec_file_write(command);  break;
                case CMD_FILE_APPEND: exec_file_write(command);  break;
                case CMD_DUMP:        exec_dump(command);        break;
                case CMD_MOVE:        exec_move(command);        break;
                case CMD_RENAME:      exec_rename(command);      break;
                case CMD_EXTERNAL:    exec_external(command);    break;
                case CMDTYPES_COUNT: shlog(SHLOG_WARNING, "`%s` command is not implemented yet.", command.name); break;
                case CMD_UNKNOWN:
                default: shlog(SHLOG_FATAL, "Unreachable"); abort();
            }
            cmd_free(&command);
        }
    }

    if (exit_code == EXIT_OK) {
        printf("\n");
        shlog(SHLOG_INFO, "Shell exited successfully with code 0");
    }
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
