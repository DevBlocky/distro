#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *rmname;

struct options {
  int recursive;
  int force;
  int dir;
};

static void printrmerr(const char *path) {
  fprintf(stderr, "(%s: remove '%s': %s)\n", rmname, path, strerror(errno));
}

static int rmpath(const char *path, const struct options *opts);
static int rmdirrecurse(const char *path, const struct options *opts) {
  DIR *dir = opendir(path);
  if (dir == NULL) {
    if (opts->force && errno == ENOENT)
      return 0;
    printrmerr(path);
    return -1;
  }

  int failed = 0;
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    size_t pathlen = strlen(path);
    size_t namelen = strlen(entry->d_name);
    int needsep = pathlen > 0 && path[pathlen - 1] != '/';

    char *child = malloc(pathlen + needsep + namelen + 1);
    memcpy(child, path, pathlen);
    if (needsep)
      child[pathlen++] = '/';
    memcpy(child + pathlen, entry->d_name, namelen + 1);

    if (rmpath(child, opts) < 0)
      failed = 1;
    free(child);
  }

  if (closedir(dir) < 0) {
    printrmerr(path);
    failed = 1;
  }

  if (rmdir(path) < 0) {
    if (!opts->force || errno != ENOENT) {
      printrmerr(path);
      failed = 1;
    }
  }

  return failed ? -1 : 0;
}

static int rmpath(const char *path, const struct options *opts) {
  struct stat st;
  if (lstat(path, &st) < 0) {
    if (opts->force && errno == ENOENT)
      return 0;
    printrmerr(path);
    return -1;
  }

  if (S_ISDIR(st.st_mode)) {
    if (opts->recursive)
      return rmdirrecurse(path, opts);

    if (opts->dir) {
      if (rmdir(path) >= 0)
        return 0;
      if (opts->force && errno == ENOENT)
        return 0;
      printrmerr(path);
      return -1;
    }

    errno = EISDIR;
    printrmerr(path);
    return -1;
  }

  if (unlink(path) >= 0)
    return 0;
  if (opts->force && errno == ENOENT)
    return 0;
  printrmerr(path);
  return -1;
}

int main(int argc, char **argv) {
  rmname = argv[0];

  struct options opts = {0};
  int opt;
  while ((opt = getopt(argc, argv, "rRfd")) != -1) {
    if (opt == 'r' || opt == 'R')
      opts.recursive = 1;
    else if (opt == 'f')
      opts.force = 1;
    else if (opt == 'd')
      opts.dir = 1;
    else {
      fprintf(stderr, "usage: %s [-rRfd] <path1> <path2...>\n", rmname);
      return EXIT_FAILURE;
    }
  }
  if ((argc - optind) < 1) {
    fprintf(stderr, "usage: %s [-rRfd] <path1> <path2...>\n", rmname);
    return EXIT_FAILURE;
  }

  int ok = 1;
  for (int i = optind; i < argc; i++) {
    if (rmpath(argv[i], &opts) < 0)
      ok = 0;
  }
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
