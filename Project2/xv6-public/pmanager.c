#include "types.h"
#include "stat.h"
#include "user.h"

#include "pmanager.h"

#define MAX_CMD_LEN 15
#define MAX_ARG_HELPER_LEN 35
#define MAX_HELPER_LEN 50

static char* pmanager_cmds[] = {
[PM_HELP]   "help",
[PM_LIST]   "list",
[PM_KILL]   "kill",
[PM_EXEC]   "exec",
[PM_MEMLIM] "memlim",
[PM_EXIT]   "exit",
};

static char* pmanager_cmd_args[] = {
[PM_HELP]   "",
[PM_LIST]   "",
[PM_KILL]   "<pid>",
[PM_EXEC]   "<path> <stacksize>",
[PM_MEMLIM] "<pid> <limit>",
[PM_EXIT]   "",
};

static char* pmanager_help_msgs[] = {
[PM_HELP]   "print help messages for pmanager commands",
[PM_LIST]   "list information of all processes",
[PM_KILL]   "kill target pid process",
[PM_EXEC]   "execute target program with given stack size",
[PM_MEMLIM] "set memory limit of process",
[PM_EXIT]   "exit process manager",
};

void print_help_msg_divider(char divider) {
  int i;

  printf(1, "+");
  for (i = 0; i < MAX_CMD_LEN; i++) {
    printf(1, "%c", divider);
  }
  printf(1, "+");
  for (i = 0; i < MAX_ARG_HELPER_LEN; i++) {
    printf(1, "%c", divider);
  }
  printf(1, "+\nr");
}

void print_help_msgs() {
  int i, j;

  print_help_msg_divider('=');
  
  for (i = 0; i < sizeof(pmanager_cmds) / sizeof(*pmanager_cmds); i++) {
    printf(1, "|%s", pmanager_cmds[i]);
    for (j = 0; j < MAX_CMD_LEN - strlen(pmanager_cmds[i]); j++) {
      printf(1, " ");
    }
    printf(1, "|%s", pmanager_cmd_args[i]);
    for (j = 0; j < MAX_ARG_HELPER_LEN - strlen(pmanager_cmd_args[i]); j++) {
      printf(1, " ");
    }
    printf(1, "|\n");
    print_help_msg_divider('-');
    printf(1, "|%s", pmanager_help_msgs[i]);
    for (j = 0; j < MAX_CMD_LEN + MAX_ARG_HELPER_LEN + 1 - strlen(pmanager_help_msgs[i]); j++) {
      printf(1, " ");
    }
    printf(1, "|\n");
    print_help_msg_divider('=');
  }

  printf(1, "\n");
}

int
main(int argc, char *argv[]) {
  printf(1, "\n===== Process Manager =====\n");
  print_help_msgs();

  exit();
}