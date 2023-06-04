#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "fs.h"

void
big_file_test(void)
{
  int i, fd, n;
  char buf[BSIZE];

  printf(2, "big files test\n");

  fd = open("big", O_CREATE|O_RDWR);
  if(fd < 0){
    printf(2, "error: creat big failed!\n");
    exit();
  }

  for(i = 0; i < 20000; i++){
    ((int*)buf)[0] = i;
    if(write(fd, buf, 512) != 512){
      printf(2, "error: write big file failed\n", i);
      exit();
    }
    if (i % 5000 == 0)
      printf(2, "write: %d / %d\n", i, 20000);
  }

  close(fd);

  fd = open("big", O_RDONLY);
  if(fd < 0){
    printf(2, "error: open big failed!\n");
    exit();
  }

  n = 0;
  for(;;){
    i = read(fd, buf, 512);
    if(i == 0){
      if(n == 20000 - 1){
        printf(2, "read only %d blocks from big", n);
        exit();
      }
      break;
    } else if(i != 512){
      printf(2, "read failed %d\n", i);
      exit();
    }
    if(((int*)buf)[0] != n){
      printf(2, "read content of block %d is %d\n",
             n, ((int*)buf)[0]);
      exit();
    }
    n++;
    if (n % 5000 == 0)
      printf(2, "read: %d / %d\n", n, 20000);
  }
  close(fd);
  if(unlink("big") < 0){
    printf(2, "unlink big failed\n");
    exit();
  }
  printf(2, "big files ok\n");
}

void get_filename_test(char* target) {
  char real[PATHSIZ];
  
  realpath(target, real);
  printf(1, "%s\n", real);
}

char* test_file = "symtest.txt";
char* test_link_file = "test.sym";
char* test_str = "hello symbolic link";

void symlink_test() {
  char buf[25];
  int fd;

  memset(buf, 0, sizeof(buf));

  fd = open(test_file, O_CREATE | O_WRONLY);
  if(fd < 0){
    printf(1, "failed to open %s (create | write)\n", test_file);
    exit();
  }

  write(fd, test_str, strlen(test_str) + 1);

  printf(1, "write <%s> finish\n", test_str);
  
  close(fd);

  symbolic_link(test_file, test_link_file);

  fd = open(test_link_file, O_RDONLY);
  if(fd < 0){
    printf(1, "failed to open %s (read)\n", test_file);
    exit();
  }

  if (read(fd, buf, strlen(test_str) + 1) <= 0) {
    printf(1, "failed to read\n");
    exit();
  }

  if (strcmp(buf, test_str) != 0) {
    printf(1, "wrong contents: %s\n", buf);
    exit();
  }

  close(fd);

  printf(1, "symbolic link test passed\n"); 
}

int
main(int argc, char *argv[])
{
  // if (argc != 2) {
  //   printf(1, "wrong input\n");
  // }

  // get_filename_test(argv[1]);

  //symlink_test();

  big_file_test();

  exit();
}