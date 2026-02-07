#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define KILL "/usr/sbin/kill"

extern char **environ;

int main(int argc, char **argv) {
  if (geteuid() != 0) {
    fprintf(stderr, "(%s: must be run as root)\n", argv[0]);
    return EXIT_FAILURE;
  }

  // kill -p15 1 (send SIGTERM to pid=1)
  char *const killargv[] = {KILL, "-p15", "1", NULL};
  execve(KILL, killargv, environ);
  fprintf(stderr, "(%s: exec %s: %s)\n", argv[0], KILL, strerror(errno));
  abort();
}
