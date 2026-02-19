#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv) {
  int srccount = argc - 2;
  if (srccount < 1) {
    fprintf(stderr, "usage: %s <src1> <src2...> <dst>\n", argv[0]);
    return EXIT_FAILURE;
  }

  // check that the destination does not exist or is a directory
  char *dst = argv[argc - 1];
  size_t dstlen = strlen(dst);
  struct stat st;
  int stok = stat(dst, &st) == 0;
  if (!stok && errno != ENOENT) {
    fprintf(stderr, "(%s: stat '%s': %s)", argv[0], dst, strerror(errno));
    return EXIT_FAILURE;
  }
  if (srccount > 1) {
    if (!stok) {
      fprintf(stderr, "(%s: stat '%s': %s)", argv[0], dst, strerror(ENOENT));
      return EXIT_FAILURE;
    } else if (!S_ISDIR(st.st_mode)) {
      fprintf(stderr, "(%s: stat '%s': %s)", argv[0], dst, strerror(ENOTDIR));
      return EXIT_FAILURE;
    }
  }

  int failed = 0;
  for (int i = 1; i < (argc - 1); i++) {
    char *target = dst;

    // if dst is a directory, then we should move
    // into a file inside that directory
    if (stok && S_ISDIR(st.st_mode)) {
      char *srcdup = strdup(argv[i]);
      char *b = basename(srcdup);
      size_t blen = strlen(b);
      int needsep = dstlen > 0 && dst[dstlen - 1] != '/';

      target = malloc(dstlen + needsep + blen + 1);
      memcpy(target, dst, dstlen);
      if (needsep)
        target[dstlen] = '/';
      memcpy(target + dstlen + needsep, b, blen + 1);

      free(srcdup);
    }

    // perform the rename
    if (rename(argv[i], target) < 0) {
      fprintf(stderr, "(%s: rename '%s' to '%s': %s)\n", argv[0], argv[i],
              target, strerror(errno));
      failed = 1;
    }

    if (target != dst)
      free(target);
  }

  return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
