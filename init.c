#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#define GETTY "/usr/sbin/getty"

extern char **environ;

static void onterm(int sig) { _exit(0); }

int main(void) {
  struct sigaction sa = {0};
  sa.sa_handler = onterm;
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);

  pid_t pid = getpid();
  if (pid != 1) {
    fprintf(stderr, "(init: can only be run as pid=1)\n");
    return EXIT_FAILURE;
  }

  char *ttyfile = ttyname(STDIN_FILENO);
  // remove init pgrp as tty controller
  if (tcgetpgrp(STDIN_FILENO) == getpgrp()) {
    ioctl(STDIN_FILENO, TIOCNOTTY);
  }

  // usually there would be more background programs
  // spawned here, but we have no background tasks

  // spawn getty (login+shell)
  pid = fork();
  if (pid < 0) {
    fprintf(stderr, "(init: fork: %s)\n", strerror(errno));
    abort();
  }
  if (pid == 0) {
    char *const gettyargv[] = {GETTY, ttyfile, NULL};
    if (execve(GETTY, gettyargv, environ) < 0)
      fprintf(stderr, "(init: execve: %s)\n", strerror(errno));
    abort();
  }

  // wait for getty to exit (should never happen)
  int status;
  if (wait(&status) < 0) {
    fprintf(stderr, "(init: wait: %s)\n", strerror(errno));
    abort();
  }
  switch (WIFSIGNALED(status) ? WTERMSIG(status) : -1) {
  case SIGSEGV:
    fprintf(stderr, "(init: child segmentation fault)\n");
    break;
  case SIGABRT:
    fprintf(stderr, "(init: child aborted)\n");
    break;
  default:
    fprintf(stderr, "(init: child exited with status %d)\n", status);
  }

  abort();
}
