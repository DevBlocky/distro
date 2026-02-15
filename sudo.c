#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int sudouid(uid_t uid) {
  struct passwd *pwd = getpwuid(uid);
  if (pwd == NULL)
    return 0;
  struct group *grp = getgrnam("sudo");
  if (grp == NULL)
    return 0;
  for (size_t i = 0; grp->gr_mem[i]; i++)
    if (strcmp(grp->gr_mem[i], pwd->pw_name) == 0)
      return 1;
  return 0;
}

int main(int argc, char **argv) {
  if (argc <= 1) {
    fprintf(stderr, "usage: %s <command>\n", argv[0]);
    return EXIT_FAILURE;
  }

  uid_t uid = getuid();
  if (uid != 0 && !sudouid(uid)) {
    fprintf(stderr, "(%s: user not in 'sudo' group)\n", argv[0]);
    return EXIT_FAILURE;
  }

  execvp(argv[1], &argv[1]);
  fprintf(stderr, "(%s: cannot exec %s: %s)\n", argv[0], argv[1], strerror(errno));
  abort();
}
