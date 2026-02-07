#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define LOGIN "/usr/sbin/login"

extern char **environ;

int main(void) {
  pid_t pid = getpid();
  if (pid != 1) {
    fprintf(stderr, "(init: can only be run as pid=1)\n");
    return EXIT_FAILURE;
  }

  for (;;) {
    pid = fork();
    if (pid < 0) {
      fprintf(stderr, "(init: cannot fork: %s)\n", strerror(errno));
      abort();
    }
    if (pid == 0) {
      // we are the child process, execve login
      char *const argv[] = {LOGIN, NULL};
      if (execve(LOGIN, argv, environ) < 0)
        fprintf(stderr, "(init: cannot exec: %s)\n", strerror(errno));
      abort();
    }

    // wait for child process to exit
    int status;
    if (wait(&status) < 0) {
      fprintf(stderr, "(init: cannot wait: %s)\n", strerror(errno));
      abort();
    }

    // if child exited gracefully, then we are done :)
    // FIXME: don't exit on successful child
    if (WIFEXITED(status))
      return EXIT_SUCCESS;
    // log if child exited with bad condition
    if (WIFSIGNALED(status)) {
      if (WTERMSIG(status) == SIGSEGV)
        fprintf(stderr, "(init: child segmentation fault)\n");
      if (WTERMSIG(status) == SIGABRT)
        fprintf(stderr, "(init: child aborted)\n");
    }
  }
}
