#include "types.h"
#include "stat.h"
#include "user.h"

#include "pmanager.h"
#include "pstat.h"

#define MAX_CMD_LEN 15
#define MAX_ARG_LEN 60
#define MAX_ARG_HELPER_LEN 35
#define MAX_HELPER_LEN 50

#define MAX_INT_FIELD_LEN 15
#define MAX_PROC_NAME_LEN 18

static char* pmanager_cmds[] = {
[PM_HELP]   "help",
[PM_LIST]   "list",
[PM_KILL]   "kill",
[PM_EXEC]   "execute",
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

char whitespace[] = " \t\r\n\v";

PStat pstat_list[NPROC];

int
main(int argc, char *argv[]) {
  static char buf[150];
  static char cmd[MAX_CMD_LEN];
  static char arg1[MAX_ARG_LEN];
  static char arg2[MAX_ARG_LEN];
  int cmd_len;
  int cmd_type;

  printf(2, "Process Manager\n");
  print_help_msgs();

  while (getline(buf, sizeof(buf)) >= 0) {
    cmd_len = strlen(buf);
    if (parse_cmd(buf, buf + cmd_len, cmd, arg1, arg2) < 0) {
      continue;
    }

    cmd_type = get_cmd_type(cmd);
    run_cmd(cmd_type, arg1, arg2);
  }

  exit();
}

void print_help_msg_divider(char divider) {
  int i;

  printf(2, "+");
  for (i = 0; i < MAX_CMD_LEN; i++) {
    printf(2, "%c", divider);
  }
  printf(2, "+");
  for (i = 0; i < MAX_ARG_HELPER_LEN; i++) {
    printf(2, "%c", divider);
  }
  printf(2, "+\n");
}

void print_help_msgs() {
  int i, j;

  print_help_msg_divider('=');
  
  for (i = 0; i < sizeof(pmanager_cmds) / sizeof(*pmanager_cmds); i++) {
    printf(2, "|%s", pmanager_cmds[i]);
    for (j = 0; j < MAX_CMD_LEN - strlen(pmanager_cmds[i]); j++) {
      printf(2, " ");
    }
    printf(2, "|%s", pmanager_cmd_args[i]);
    for (j = 0; j < MAX_ARG_HELPER_LEN - strlen(pmanager_cmd_args[i]); j++) {
      printf(2, " ");
    }
    printf(2, "|\n");
    print_help_msg_divider('-');
    printf(2, "|%s", pmanager_help_msgs[i]);
    for (j = 0; j < MAX_CMD_LEN + MAX_ARG_HELPER_LEN + 1 - strlen(pmanager_help_msgs[i]); j++) {
      printf(2, " ");
    }
    printf(2, "|\n");
    print_help_msg_divider('=');
  }
}

void print_error(char* errstr) {
  printf(2, "error: %s\n", errstr);
}

int getline(char *buf, int nbuf)
{
  printf(2, "\n>>> ");
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

// Inspired from HYU Computer Architecture Class Project (assembler.c)
// Simillar with parsing assembly language opcode and args in assembler
// Also brought codes from sh.c
int parse_cmd(char* buf, char* buf_end, char* cmd, char* arg1, char* arg2) {
  char* target_args[3] = {cmd, arg1, arg2};
  char* s = buf;
  char* start;
  int i;
  
  cmd[0] = arg1[0] = arg2[0] = '\0';

  if (strchr(buf, '\n') == 0) {
    print_error("line is too long");
    
    return -1;
  }

  while (s < buf_end && strchr(whitespace, *s)) {
    s++;
  }

  if (s == buf_end) {
    print_error("invalid format");

    return -1;
  }

  for (i = 0; i < 3; i++) {
    for (start = s; s < buf_end && !strchr(whitespace, *s); s++) {
      target_args[i][s - start] = *s;
    }
    target_args[i][s - start] = '\0';

    while (s < buf_end && strchr(whitespace, *s)) {
      s++;
    }

    if (s == buf_end) {
      return 0;
    }
  }

  return 0;
}

int get_cmd_type(char* cmd_str) {
  int i;
  
  for (i = 0; i < sizeof(pmanager_cmds) / sizeof(*pmanager_cmds); i++) {
    if (!strcmp(cmd_str, pmanager_cmds[i])) {
      return i;
    }
  }

  return PM_ERROR;
}

int get_int_arg(char* arg_str, int* arg) {
  int arg_val;

  if (*arg_str < '0' || *arg_str > '9') {
    return -1;
  }

  arg_val = atoi(arg_str);

  if (arg_val < 0 || arg_val > 1000000000) {
    return -1;
  }

  *arg = arg_val;
  return 0;
}

int get_intlen(int i) {
  int len;
  
  for (len = 1; i >= 10; i /= 10, len++);

  return len;
}

void print_proc_list_divider(char divider) {
  int i, j;

  printf(2, "+");
  for (i = 0; i < MAX_INT_FIELD_LEN; i++) {
    printf(2, "%c", divider);
  }
  printf(2, "+");
  for (i = 0; i < MAX_PROC_NAME_LEN; i++) {
    printf(2, "%c", divider);
  }
  for (i = 0; i < 3; i++) {
    printf(2, "+");
    for (j = 0; j < MAX_INT_FIELD_LEN; j++) {
      printf(2, "%c", divider);
    }
  }
  printf(2, "+\n");
}

void print_proc_list() {
  int proc_num, i, j, k;
  int int_fields[3];

  if (proclist(pstat_list, &proc_num) < 0) {
    print_error("getting process list failed");
  } else {
    printf(2, "\n");

    print_proc_list_divider('=');
    printf(2, "|pid");
    for (j = 0; j < (MAX_INT_FIELD_LEN - strlen("pid")); j++) {
      printf(2, " ");
    }
    printf(2, "|name");
    for (j = 0; j < (MAX_PROC_NAME_LEN - strlen("name")); j++) {
      printf(2, " ");
    }
    printf(2, "|stack pages");
    for (j = 0; j < (MAX_INT_FIELD_LEN - strlen("stack pages")); j++) {
      printf(2, " ");
    }
    printf(2, "|memory size");
    for (j = 0; j < (MAX_INT_FIELD_LEN - strlen("memory size")); j++) {
      printf(2, " ");
    }
    printf(2, "|memory limit");
    for (j = 0; j < (MAX_INT_FIELD_LEN - strlen("memory limit")); j++) {
      printf(2, " ");
    }
    printf(2, "|\n");

    for (i = 0; i < proc_num; i++) {
      int_fields[0] = pstat_list[i].stack_page_num;
      int_fields[1] = pstat_list[i].sz;
      int_fields[2] = pstat_list[i].memory_limit;

      print_proc_list_divider('-');

      printf(2, "|%d", pstat_list[i].pid);
      for (j = 0; j < (MAX_INT_FIELD_LEN - get_intlen(pstat_list[i].pid)); j++) {
        printf(2, " ");
      }
      printf(2, "|%s", pstat_list[i].name);
      for (j = 0; j < (MAX_PROC_NAME_LEN - strlen(pstat_list[i].name)); j++) {
        printf(2, " ");
      }

      for (j = 0; j < 3; j++) {
        printf(2, "|%d", int_fields[j]);
        for (k = 0; k < (MAX_INT_FIELD_LEN - get_intlen(int_fields[j])); k++) {
          printf(2, " ");
        }
      }
      printf(2, "|\n");
    }
    print_proc_list_divider('=');
  }
}

void kill_wrapper(int pid) {
  if (kill(pid) < 0) {
    print_error("kill failed");
  } else {
    printf(2, "Successfully killed Process %d\n", pid);
  }

  sleep(10);
}

void execute_process(char* path, int stacksize) {
  char* argv[1] = {path};

  if (fork() == 0) {
    if (fork() == 0) {
      if (exec2(path, argv, stacksize) < 0) {
        print_error("execute failed");
        exit();
      }
    }
    exit();
  }
  wait();
}

void setmemorylimit_wrapper(int pid, int limit) {
  if (setmemorylimit(pid, limit) < 0) {
    print_error("set memory limit failed");
  } else {
    printf(2, "Successfully set memory limit of Process %d\n", pid);
  }
}

void run_cmd(int cmd_type, char* arg1_str, char* arg2_str) {
  int arg1;
  int arg2;

  switch (cmd_type) {
    case PM_HELP:
      print_help_msgs();
      break;
    
    case PM_LIST:
      print_proc_list();
      break;

    case PM_KILL:
      if (get_int_arg(arg1_str, &arg1) < 0) {
        print_error("kill - wrong format");
        break;
      }
      printf(2, "Killing Process %d\n", arg1);
      kill_wrapper(arg1);
      break;

    case PM_EXEC:
      if (get_int_arg(arg2_str, &arg2) < 0) {
        print_error("execute - wrong format");
        break;
      }
      printf(2, "Executing Process at %s with stack size %d\n", arg1_str, arg2);
      execute_process(arg1_str, arg2);
      break;

    case PM_MEMLIM:
      if (get_int_arg(arg1_str, &arg1) < 0 || get_int_arg(arg2_str, &arg2) < 0) {
        print_error("execute - wrong format");
        break;
      }
      printf(2, arg2 != 0 ? "Set memory limit of Process %d to %d bytes\n" : "Remove memory limit of Process %d\n", arg1, arg2);
      setmemorylimit_wrapper(arg1, arg2);
      break;

    case PM_EXIT:
      printf(2, "Exit Process Manager\n");
      exit();

    default:
      print_error("Invalid Command!");
      break;
  }
}