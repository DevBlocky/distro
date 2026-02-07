#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <seconds>\n", argv[0]);
    return EXIT_FAILURE;
  }
  char *end;
  long dur = strtol(argv[1], &end, 0);
  if (*end != '\0') {
    fprintf(stderr, "(%s: invalid sleep duration %s)\n", argv[0], argv[1]);
    return EXIT_FAILURE;
  }
  sleep((unsigned int)dur);
  return EXIT_SUCCESS;
}
