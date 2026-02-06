#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

struct listent {
  ino_t inode;
  char *name;

  mode_t mode;
  uid_t uid;
  gid_t gid;
};

char filetype(mode_t m) {
  if (S_ISREG(m))
    return '-';
  if (S_ISDIR(m))
    return 'd';
  if (S_ISCHR(m))
    return 'c';
  if (S_ISBLK(m))
    return 'b';
  if (S_ISFIFO(m))
    return 'p';
  if (S_ISLNK(m))
    return 'l';
  if (S_ISSOCK(m))
    return 's';
  return '?';
}
void fileperm(mode_t m, char *s) {
  s[0] = m & S_IRUSR ? 'r' : '-';
  s[1] = m & S_IWUSR ? 'w' : '-';
  s[2] = m & S_IXUSR ? 'x' : '-';
  s[3] = m & S_IRGRP ? 'r' : '-';
  s[4] = m & S_IWGRP ? 'w' : '-';
  s[5] = m & S_IXGRP ? 'x' : '-';
  s[6] = m & S_IROTH ? 'r' : '-';
  s[7] = m & S_IWOTH ? 'w' : '-';
  s[8] = m & S_IXOTH ? 'x' : '-';
  s[9] = '\0';

  if (m & S_ISUID)
    s[2] = 's';
  if (m & S_ISGID)
    s[5] = 's';
}

int main(int argc, char **argv) {
  DIR *d = opendir(".");
  if (!d) {
    fprintf(stderr, "(%s: cannot open: %s)\n", argv[0], strerror(errno));
    return EXIT_FAILURE;
  }
  int dfd = dirfd(d);

  // create a list of all directory entries
  struct listent *list = NULL;
  size_t len = 0, cap = 0;
  for (struct dirent *dirent; (dirent = readdir(d)) != NULL;) {
    if (len >= cap) {
      cap = cap ? cap * 2 : 8;
      list = realloc(list, cap * sizeof(struct listent));
    }

    struct listent *ent = &list[len++];
    ent->inode = dirent->d_ino;
    ent->name = malloc(256 * sizeof(char));
    strncpy(ent->name, dirent->d_name, sizeof(dirent->d_name));

    struct stat statbuf;
    if (fstatat(dfd, ent->name, &statbuf, 0) != 0) {
      fprintf(stderr, "(%s: cannot stat %s: %s)\n", argv[0], ent->name,
              strerror(errno));
      continue;
    }
    ent->mode = statbuf.st_mode;
    ent->uid = statbuf.st_uid;
    ent->gid = statbuf.st_gid;
  }

  // sort dir entries by name
  // this is selection sort, probably should use something else but idc
  for (size_t i = 0; i < len; i++) {
    size_t mini = i;
    for (size_t j = i + 1; j < len; j++) {
      if (strcmp(list[j].name, list[mini].name) < 0)
        mini = j;
    }
    if (mini != i) {
      struct listent tmp = list[i];
      list[i] = list[mini];
      list[mini] = tmp;
    }
  }

  // print the list of directory entries
  for (size_t i = 0; i < len; i++) {
    struct listent *ent = &list[i];
    char perm[16];
    fileperm(ent->mode, perm);
    char typ = filetype(ent->mode);

    // TODO: once /etc/passwd is available, print user/group names instead
    // of id

    printf("%c%s (%u/%u) %s\n", typ, perm, ent->uid, ent->gid, ent->name);
  }

  return EXIT_SUCCESS;
}
