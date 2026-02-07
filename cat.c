#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
  size_t n = argc == 1 ? 1 : argc - 1;
  char **files = malloc(n * sizeof(char *));
  for (size_t i = 1; i < argc; i++)
    files[i - 1] = argv[i];
  if (argc == 1)
    files[0] = "-";

  int fd = -1;
  unsigned char buffer[4096];
  for (size_t i = 0; i < n;) {
    // open file in sequence if not open
    if (fd < 0) {
      if (strcmp(files[i], "-") != 0) {
        fd = open(files[i], O_RDONLY);
        if (fd < 0) {
          fprintf(stderr, "(%s: could not open %s: %s)\n", argv[0], files[i],
                  strerror(errno));
          i++;
          continue;
        }
      } else {
        fd = STDIN_FILENO;
      }
    }

    // read file into buffer
    ssize_t size = read(fd, buffer, sizeof(buffer) / sizeof(char));
    if (size < 0) {
      fprintf(stderr, "(%s: could not read %s: %s)\n", argv[0], files[i],
              strerror(errno));
      return EXIT_FAILURE;
    }
    if (size == 0) {
      // move onto next in sequence
      if (fd != STDIN_FILENO)
        close(fd);
      fd = -1;
      i++;
      continue;
    }

    // write buffered data into stdout
    ssize_t written = 0;
    while (written < size) {
      ssize_t w = write(STDOUT_FILENO, buffer, (size_t)size);
      if (w < 0) {
        fprintf(stderr, "(could not write: %s)\n", strerror(errno));
        return EXIT_FAILURE;
      }
      written += w;
    }
  }

  return EXIT_SUCCESS;
}
