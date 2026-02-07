#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (geteuid() != 0) {
    fprintf(stderr, "(%s: must be run as root)\n", argv[0]);
    return EXIT_FAILURE;
  }

  // get -p option
  char *end;
  int signum = SIGKILL;
  int opt;
  while ((opt = getopt(argc, argv, "p:")) != -1) {
    if (opt == 'p') {
      long sl = strtol(optarg, &end, 0);
      if (*end != '\0' || sl < 0) {
        fprintf(stderr, "(%s: invalid signal %s)\n", argv[0], optarg);
        return EXIT_FAILURE;
      }
      signum = (int)sl;
    } else {
      fprintf(stderr, "usage: %s [-p signum] <pid>\n", argv[0]);
      return EXIT_FAILURE;
    }
  }

  if ((argc - optind) != 1) {
    fprintf(stderr, "usage: %s [-p signum] <pid>\n", argv[0]);
    return EXIT_FAILURE;
  }

  // get the pid
  long pidl = strtol(argv[optind], &end, 0);
  if (*end != '\0') {
    fprintf(stderr, "(%s: process id %s must be number)\n", argv[0],
            argv[optind]);
    return EXIT_FAILURE;
  }

  // try to kill the pid
  if (kill((pid_t)pidl, signum) < 0) {
    fprintf(stderr, "(%s: cannot signal %s: %s)\n", argv[0], argv[optind],
            strerror(errno));
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
