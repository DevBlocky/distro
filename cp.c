#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *cpname;

struct options {
  int force;
  int recursive;
};

static void printpatherr(const char *op, const char *path) {
  fprintf(stderr, "(%s: %s '%s': %s)\n", cpname, op, path, strerror(errno));
}

static void printcopyerr(const char *src, const char *dst) {
  fprintf(stderr, "(%s: copy '%s' to '%s': %s)\n", cpname, src, dst,
          strerror(errno));
}

static char *joinpath(const char *left, const char *right) {
  size_t leftlen = strlen(left);
  size_t rightlen = strlen(right);
  int needsep = leftlen > 0 && left[leftlen - 1] != '/';

  char *out = malloc(leftlen + needsep + rightlen + 1);
  memcpy(out, left, leftlen);
  if (needsep)
    out[leftlen++] = '/';
  memcpy(out + leftlen, right, rightlen + 1);
  return out;
}

static int removepath(const char *path);
static int removedirrecurse(const char *path) {
  DIR *dir = opendir(path);
  if (dir == NULL)
    return -1;

  int failed = 0;
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    char *child = joinpath(path, entry->d_name);
    if (removepath(child) < 0)
      failed = 1;
    free(child);
  }

  if (closedir(dir) < 0)
    failed = 1;
  if (rmdir(path) < 0)
    failed = 1;
  return failed ? -1 : 0;
}

static int removepath(const char *path) {
  struct stat st;
  if (lstat(path, &st) < 0)
    return -1;

  if (S_ISDIR(st.st_mode))
    return removedirrecurse(path);
  return unlink(path);
}

static int copypath(const char *src, const char *dst,
                    const struct options *opts);
static int copydirectory(const char *src, const char *dst,
                         const struct options *opts) {
  DIR *dir = opendir(src);
  if (dir == NULL) {
    printpatherr("open", src);
    return -1;
  }

  int failed = 0;
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    char *srcchild = joinpath(src, entry->d_name);
    char *dstchild = joinpath(dst, entry->d_name);
    if (copypath(srcchild, dstchild, opts) < 0)
      failed = 1;
    free(srcchild);
    free(dstchild);
  }

  if (closedir(dir) < 0) {
    printpatherr("close", src);
    failed = 1;
  }

  return failed ? -1 : 0;
}

static int copypath(const char *src, const char *dst,
                    const struct options *opts) {
  struct stat srcst;
  if (lstat(src, &srcst) < 0) {
    printpatherr("stat", src);
    return -1;
  }

  // check destination conditions
  struct stat dstst;
  int dststok = lstat(dst, &dstst) == 0;
  if (dststok) {
    if (srcst.st_dev == dstst.st_dev && srcst.st_ino == dstst.st_ino) {
      errno = EINVAL;
      printcopyerr(src, dst);
      return -1;
    } else if (!opts->force) {
      errno = EEXIST;
      printcopyerr(src, dst);
      return -1;
    }
    // if copying regular file or directory->!directory,
    // remove the destination
    if ((!S_ISDIR(srcst.st_mode) || !S_ISDIR(dstst.st_mode)) &&
        removepath(dst) < 0) {
      printpatherr("remove", dst);
      return -1;
    }
  } else if (errno != ENOENT) {
    printpatherr("stat", dst);
    return -1;
  }

  if (S_ISDIR(srcst.st_mode)) {
    if (!opts->recursive) {
      errno = EISDIR;
      printcopyerr(src, dst);
      return -1;
    }
    // mkdir if destination is not directory
    if ((!dststok || !S_ISDIR(dstst.st_mode)) &&
        mkdir(dst, srcst.st_mode & 0777) < 0) {
      printpatherr("mkdir", dst);
      return -1;
    }
    // source is directory, so copy recursively
    return copydirectory(src, dst, opts);
  }

  // open both files then use sendfile to copy
  int srcfd = open(src, O_RDONLY);
  if (srcfd < 0) {
    printpatherr("open", src);
    return -1;
  }
  int dstfd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, srcst.st_mode & 0777);
  if (dstfd < 0) {
    printpatherr("open", dst);
    close(srcfd);
    return -1;
  }

  int failed = 0;
  while (1) {
    ssize_t sent = sendfile(dstfd, srcfd, NULL, 1 << 20);
    if (sent < 0) {
      if (errno == EINTR)
        continue;
      printcopyerr(src, dst);
      failed = 1;
      break;
    }
    if (sent == 0)
      break;
  }

  // cleanup
  if (close(srcfd) < 0 && !failed) {
    printpatherr("close", src);
    failed = 1;
  }
  if (close(dstfd) < 0 && !failed) {
    printpatherr("close", dst);
    failed = 1;
  }
  return failed ? -1 : 0;
}

int main(int argc, char **argv) {
  cpname = argv[0];

  struct options opts = {0};
  int opt;
  while ((opt = getopt(argc, argv, "rRf")) != -1) {
    if (opt == 'f')
      opts.force = 1;
    else if (opt == 'r' || opt == 'R')
      opts.recursive = 1;
    else {
      fprintf(stderr, "usage: %s [-rRf] <src1> <src2...> <dst>\n", cpname);
      return EXIT_FAILURE;
    }
  }

  if ((argc - optind) < 2) {
    fprintf(stderr, "usage: %s [-rRf] <src1> <src2...> <dst>\n", cpname);
    return EXIT_FAILURE;
  }

  int srccount = argc - optind - 1;
  char *dst = argv[argc - 1];

  struct stat st;
  int statok = lstat(dst, &st) == 0;
  if (!statok && errno != ENOENT) {
    printpatherr("stat", dst);
    return EXIT_FAILURE;
  }
  if (srccount > 1) {
    if (!statok) {
      errno = ENOENT;
      printpatherr("stat", dst);
      return EXIT_FAILURE;
    }
    if (!S_ISDIR(st.st_mode)) {
      errno = ENOTDIR;
      printpatherr("stat", dst);
      return EXIT_FAILURE;
    }
  }

  int failed = 0;
  for (int i = optind; i < argc - 1; i++) {
    char *target = dst;
    if (statok && S_ISDIR(st.st_mode)) {
      char *srcdup = strdup(argv[i]);
      char *base = basename(srcdup);
      target = joinpath(dst, base);
      free(srcdup);
    }

    if (copypath(argv[i], target, &opts) < 0)
      failed = 1;

    if (target != dst)
      free(target);
  }

  return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
