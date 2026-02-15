#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void groupdup(const struct group *src, struct group *dst) {
  *dst = (struct group){0};
  dst->gr_name = strdup(src->gr_name);
  dst->gr_passwd = strdup(src->gr_passwd);
  dst->gr_gid = src->gr_gid;

  size_t memlen = 0;
  while (src->gr_mem[memlen])
    memlen++;

  dst->gr_mem = malloc(sizeof(char *) * (memlen + 1));
  for (size_t i = 0; i < memlen; i++)
    dst->gr_mem[i] = strdup(src->gr_mem[i]);
  dst->gr_mem[memlen] = NULL;
}

static int groupamem(char ***mem_ptr, const char *user) {
  int found = 0;
  size_t count = 0;
  for (size_t i = 0; (*mem_ptr)[i]; i++) {
    found = found || strcmp((*mem_ptr)[i], user) == 0;
    count++;
  }
  if (found)
    return 0;
  *mem_ptr = realloc(*mem_ptr, (count + 2) * sizeof(char *));
  (*mem_ptr)[count] = strdup(user);
  (*mem_ptr)[count + 1] = NULL;
  return 1;
}

static int groupdmem(char **mem, const char *user) {
  int found = 0;
  for (size_t i = 0; mem[i]; i++) {
    if (strcmp(mem[i], user) == 0) {
      free(mem[i]);
      found++;
    }
    if (found > 0)
      mem[i] = mem[i + found];
  }
  return found;
}

int main(int argc, char **argv) {
  if (geteuid() != 0) {
    fprintf(stderr, "(%s: must be run as root)\n", argv[0]);
    return EXIT_FAILURE;
  }
  if (argc != 4 || (strcmp(argv[1], "-a") != 0 && strcmp(argv[1], "-d") != 0)) {
    fprintf(stderr, "usage: %s -a <user> <group>\n", argv[0]);
    fprintf(stderr, "       %s -d <user> <group>\n", argv[0]);
    return EXIT_FAILURE;
  }

  int add_mode = strcmp(argv[1], "-a") == 0;
  const char *user = argv[2];
  const char *group = argv[3];

  errno = 0;
  if (!getpwnam(user)) {
    if (errno == 0)
      fprintf(stderr, "(%s: user '%s' does not exist)\n", argv[0], user);
    else
      fprintf(stderr, "(%s: getpwnam: %s)\n", argv[0], strerror(errno));
    return EXIT_FAILURE;
  }

  struct group *groups = NULL;
  size_t len = 0, cap = 0;
  ssize_t groupidx = -1;
  struct group *grp;
  setgrent();
  while ((grp = getgrent()) != NULL) {
    if (len == cap) {
      cap = cap ? cap * 2 : 16;
      groups = realloc(groups, cap * sizeof(struct group));
    }
    if (strcmp(grp->gr_name, group) == 0)
      groupidx = len;
    memset(&groups[len], 0, sizeof(groups[len]));
    groupdup(grp, &groups[len++]);
  }
  if (groupidx < 0) {
    fprintf(stderr, "(%s: group '%s' does not exist)\n", argv[0], group);
    return EXIT_FAILURE;
  }

  int ok = add_mode ? groupamem(&groups[groupidx].gr_mem, user)
                    : groupdmem(groups[groupidx].gr_mem, user);
  if (!ok) {
    fprintf(stderr,
            add_mode ? "(%s: member '%s' already exists)\n"
                     : "(%s: member '%s' doesn't exist)\n",
            argv[0], user);
    return EXIT_FAILURE;
  }

  FILE *out = fopen("/etc/group", "w");
  if (!out) {
    fprintf(stderr, "(%s: cannot open /etc/group for write: %s)\n", argv[0],
            strerror(errno));
    abort();
  }
  for (size_t i = 0; i < len; i++) {
    if (putgrent(&groups[i], out) != 0) {
      fprintf(stderr, "(%s: write failed: %s)\n", argv[0], strerror(errno));
      abort();
    }
  }
  fclose(out);

  return EXIT_SUCCESS;
}
