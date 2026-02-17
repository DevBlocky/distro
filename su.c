#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "password.h"
#include <ctype.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

extern char **environ;

int main(int argc, char **argv) {
  if (argc > 2) {
    fprintf(stderr, "usage: %s [login]\n", argv[0]);
    return EXIT_FAILURE;
  }

  uid_t oth = 0;
  struct passwd *othpwd = argc >= 2 ? getpwnam(argv[1]) : getpwuid(oth);
  if (!othpwd) {
    if (argc >= 2)
      fprintf(stderr, "(%s: user '%s' not found)\n", argv[0], argv[1]);
    else
      fprintf(stderr, "(%s: user %d not found)\n", argv[0], oth);
    return EXIT_FAILURE;
  }
  oth = othpwd->pw_uid;

  // if not root uid, ask for password
  uid_t cur = getuid();
  if (cur != 0 && cur != oth) {
    char *password = NULL;
    size_t passwordcap = 0;
    ssize_t passwordlen;
    do {
      printf("password: ");
      fflush(stdout);
      passwordlen = getpassword(&password, &passwordcap, stdin);
      if (passwordlen <= 0) {
        fprintf(stderr, "(%s: cannot read: %s)", argv[0], strerror(errno));
        abort();
      }
      while (isspace(password[passwordlen - 1]))
        password[--passwordlen] = '\0';
    } while (!checkpassword(othpwd, password));
  }

  // setup user environ
  setenv("HOME", othpwd->pw_dir, 1);
  setenv("SHELL", othpwd->pw_shell, 1);
  chdir(othpwd->pw_dir);
  umask(S_IWGRP | S_IWOTH);

  // switch user/group
  if (initgroups(othpwd->pw_name, oth) < 0 ||
      setresgid(oth, oth, oth) < 0 ||
      setresuid(oth, oth, oth) < 0) {
    fprintf(stderr, "(%s: login fail: %s)\n", argv[0], strerror(errno));
    abort();
  }

  // start shell
  char *const shargv[] = {othpwd->pw_shell, NULL};
  if (execve(othpwd->pw_shell, shargv, environ) < 0)
    fprintf(stderr, "(%s: shell: %s)\n", argv[0], strerror(errno));
  abort();
}
