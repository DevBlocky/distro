#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define SH "/usr/bin/sh"

extern char **environ;

int main(void) {
  pid_t pid;
  int status;

  pid = getpid();
  if (pid != 1) {
    puts("can only be run as pid=1");
    return EXIT_FAILURE;
  }

  // TODO: add login to shell
  char *const argv[] = {SH, NULL};
  status = execve(SH, argv, environ);
  return EXIT_FAILURE;
}
