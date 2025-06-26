#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "./nob.h"

Nob_Cmd cmd = {0};

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    cmd_append(&cmd, "cc", "-o", "ashell", "./ashell.c");
    cmd_append(&cmd, "./ashell_utils.c");
    cmd_append(&cmd, "./ashed.c");
    cmd_append(&cmd, "-Wall", "-Wextra", "-Wswitch-enum", "-lm");
    if (!cmd_run_sync_and_reset(&cmd)) return 1;

    //cmd_add_c_file(&cmd, "ashed");
    //if (!cmd_run_sync_and_reset(&cmd)) return 1;

    //cmd_add_c_file(&cmd, "ashell_utils");
    //if (!cmd_run_sync_and_reset(&cmd)) return 1;

    return 0;
}

