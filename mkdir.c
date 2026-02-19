#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv) {
  int parents = 0;
  int opt;
  while ((opt = getopt(argc, argv, "p")) != -1) {
    if (opt == 'p')
      parents = 1;
    else {
      fprintf(stderr, "usage: %s [-p] <dir>\n", argv[0]);
      return EXIT_FAILURE;
    }
  }
  if ((argc - optind) != 1) {
    fprintf(stderr, "usage: %s [-p] <dir>\n", argv[0]);
    return EXIT_FAILURE;
  }

  const char *dir = argv[optind];
  size_t dirlen = strlen(dir);
  struct stat st;
  for (size_t i = 1; parents && (i + 1) < dirlen; i++) {
    if (dir[i] != '/')
      continue;

    char *partial = malloc((i + 1) * sizeof(char));
    memcpy(partial, dir, i);
    partial[i] = '\0';

    if (mkdir(partial, 0777) >= 0) {
      // mkdir ok, pass
    } else if (errno != EEXIST) {
      // mkdir error
      fprintf(stderr, "(%s: cannot make '%s': %s)\n", argv[0], partial,
              strerror(errno));
      return EXIT_FAILURE;
    } else if (stat(partial, &st) < 0) {
      // stat error
      fprintf(stderr, "(%s: cannot stat '%s': %s)\n", argv[0], partial,
              strerror(errno));
      return EXIT_FAILURE;
    } else if (!S_ISDIR(st.st_mode)) {
      // mkdir EEXIST and stat is not directory
      fprintf(stderr, "(%s: cannot make '%s': %s)\n", argv[0], partial,
              strerror(ENOTDIR));
      return EXIT_FAILURE;
    }

    free(partial);
  }

  if (mkdir(dir, 0777) < 0) {
    fprintf(stderr, "(%s: cannot make '%s': %s)\n", argv[0], dir,
            strerror(errno));
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
