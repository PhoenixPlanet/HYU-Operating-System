#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "fs.h"

#define BIGBLOCK 20000
#define DOUBLEBLOCK 16384
#define TRIPLEBLOCK 32768
#define SMALLBLOCK 30

void
write_test(char* name, int file_block_size, int do_sync)
{
  int i, fd;
  int buf[BSIZE];

  printf(2, "%s files test\n", name);

  fd = open(name, O_CREATE|O_RDWR);
  if(fd < 0){
    printf(2, "error: creat small failed!\n");
    exit();
  }

  for(i = 0; i < file_block_size; i++){
    buf[(BSIZE/sizeof(int)) - 1] = i;
    if(write(fd, (char*)buf, 512) != 512){
      printf(2, "error: %d of write %s file failed\n", name, i);
      exit();
    }
    if (i % (file_block_size / 10) == 0 || (i == file_block_size - 1))
      printf(2, "write: %d / %d\n", (i == file_block_size - 1) ? file_block_size : i, file_block_size);
  }
  if (do_sync) {
    printf(2, "flushed %d\n", sync());
  }
  
  close(fd);
}

void read_test(char* name, int file_block_size) {
  int i, fd, n;
  int buf[BSIZE/sizeof(int)];

  fd = open(name, O_RDONLY);
  if(fd < 0){
    printf(2, "error: open %s failed!\n", name);
    exit();
  }

  n = 0;
  for(;;){
    i = read(fd, (char*)buf, 512);
    if(i == 0){
      if(n == file_block_size - 1){
        printf(2, "read only %d blocks from %s\n", n, name);
        exit();
      }
      break;
    } else if(i != 512){
      printf(2, "read failed %d\n", i);
      exit();
    }
    if(buf[(BSIZE/sizeof(int)) - 1] != n){
      printf(2, "read content of block %d is %d\n",
             n, buf[(BSIZE/sizeof(int)) - 1]);
      exit();
    }
    n++;
    if (n % (file_block_size / 10) == 0)
      printf(2, "read: %d / %d\n", n, file_block_size);
  }
  close(fd);
  if(unlink(name) < 0){
    printf(2, "unlink %s failed\n", name);
    exit();
  }
  printf(2, "%s files ok\n", name);
}

void get_filename_test(char* target) {
  char real[PATHSIZ];
  
  realpath(target, real);
  printf(1, "%s\n", real);
}

// char* test_dir = "test";
// char* test_dir2 = "test/test2";
// char* test_file = "symtest.txt";
// char* test_target = "../test2/./symtest.txt";
// char* test_link_file = "/test.sym";
// char* test_link_open_target = "test.sym";
// char* test_str = "hello symbolic link\n";
char* test_file = "symtest.txt";
char* test_target = "symtest.txt";
char* test_link_file = "test.sym";
char* test_link_open_target = "test.sym";
char* test_str = "hello symbolic link\n";

void symlink_test() {
  char buf[25];
  int fd;

  memset(buf, 0, sizeof(buf));

  // mkdir(test_dir);
  // mkdir(test_dir2);
  // chdir(test_dir2);

  fd = open(test_file, O_CREATE | O_WRONLY);
  if(fd < 0){
    printf(2, "failed to open %s (create | write)\n", test_file);
    exit();
  }

  write(fd, test_str, strlen(test_str) + 1);

  printf(2, "write <%s> finish\n", test_str);
  //printf(2, "flushed %d\n", sync());
  
  close(fd);

  symbolic_link(test_target, test_link_file);

  //chdir("/");

  fd = open(test_link_open_target, O_RDONLY);
  if(fd < 0){
    printf(2, "failed to open %s (read)\n", test_file);
    exit();
  }

  if (read(fd, buf, strlen(test_str) + 1) <= 0) {
    printf(2, "failed to read\n");
    exit();
  }

  if (strcmp(buf, test_str) != 0) {
    printf(2, "wrong contents: %s\n", buf);
    exit();
  }

  close(fd);

  printf(2, "symbolic link test passed\n"); 
}

void
children_read(int ppid) {
    char name1[7] = "hello ";
    char buf[512];
    int fd, i;

    name1[6] = 'a' + getpid() - 1;
    for (i = 0; i < 1000; i++) {
        fd = open(name1, O_RDONLY);
        read(fd, buf, 512);
        close(fd);
    }

    printf(2, "read done %d\n", getpid() - ppid - 1);
    
    exit();
}

void many_read()
{
    int fd, i, id, ppid;
    char buf[512];
    char name[7] = "hello ";

    memset(buf, 'a', 512);

    ppid = getpid();

    for (i = getpid(); i < 10 + ppid; i++)
    {
        name[6] = 'a' + i - 3;
        fd = open(name, O_CREATE);
        write(fd, buf, 512);
        close(fd);
    }
    printf(1, "write done\n");
    for (i = ppid; i < 10 + ppid; i++)
    {
        if ((id = fork()) == 0)
        {
            break;
        }
    }

    if (getpid() != ppid) {
        sleep(10);
        children_read(ppid);
    } else {
        write_test("big", BIGBLOCK, 1);
        while (wait() != -1) {}
    }
}

int
main(int argc, char *argv[])
{
  // if (argc != 2) {
  //   printf(1, "wrong input\n");
  // }

  // get_filename_test(argv[1]);

  //symlink_test();

  //big_file_test();

  if (strcmp(argv[1], "w") == 0) {
    if (strcmp(argv[2], "t") == 0) {
      write_test("small", SMALLBLOCK, 1);
    } else if (strcmp(argv[2], "f") == 0) {
      write_test("small", SMALLBLOCK, 0);
    } else if (strcmp(argv[2], "double") == 0) {
      write_test("double", DOUBLEBLOCK, 1);
    } else if (strcmp(argv[2], "triple") == 0) {
      write_test("triple", TRIPLEBLOCK, 1);
    } 
  }

  if (strcmp(argv[1], "r") == 0) {
    if (strcmp(argv[2], "n") == 0) {
      read_test("small", SMALLBLOCK);
    } else if (strcmp(argv[2], "double") == 0) {
      read_test("double", DOUBLEBLOCK);
    } else if (strcmp(argv[2], "triple") == 0) {
      read_test("triple", TRIPLEBLOCK);
    } 
  }

  if (strcmp(argv[1], "s") == 0) {
    symlink_test();
  }

  if (strcmp(argv[1], "n") == 0) {
    get_filename_test(argv[2]);
  }

  if (strcmp(argv[1], "mr") == 0) {
    many_read();
  }

  exit();
}