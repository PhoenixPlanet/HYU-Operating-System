#define PM_HELP     0
#define PM_LIST     1
#define PM_KILL     2
#define PM_EXEC     3
#define PM_MEMLIM   4
#define PM_EXIT     5
#define PM_ERROR    -1

#define NPROC       64

void print_help_msg_divider(char);
void print_help_msgs();
void print_error(char*);
int parse_cmd(char*, char*, char*, char*, char*);
int get_cmd_type(char*);
int run_cmd(int, char*, char*);