#include "types.h"
#include "stat.h"
#include "user.h"

#include "pmanager.h"

static char* pmanager_cmds[] = {
[PM_HELP]   "help",
[PM_LIST]   "list",
[PM_KILL]   "kill",
[PM_EXEC]   "exec",
[PM_MEMLIM] "memlim",
[PM_EXIT]   "exit",
};

static char* pmanager_help_msgs[] = {
[PM_HELP]   "print help messages for pmanager commands",
[PM_LIST]   "list information of all processes",
[PM_KILL]   "kill target pid process",
[PM_EXEC]   "execute target program with given stack size",
[PM_MEMLIM] "memlim",
[PM_EXIT]   "exit",
};

int
main(int argc, char *argv[]) {
  printf(1, "\nProcess Manager\n");

  exit();
}