typedef struct _PStat {
  int pid;
  char name[16];
  int stack_page_num;
  uint sz;
  int memory_limit;
} PStat;