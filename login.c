#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "password.h"

#define SH "/usr/bin/sh"

extern char **environ;
char *loginname;

int main(int argc, char **argv) {
  loginname = argv[0];
  if (geteuid() != 0) {
    fprintf(stderr, "(%s: cannot work without effective root)\n", loginname);
    return EXIT_FAILURE;
  }

  char *name = NULL, *password = NULL;
  size_t namecap = 0, passwordcap = 0;
  ssize_t namelen, passwordlen;
  for (;;) {
    printf("login: ");
    fflush(stdout);
    namelen = getline(&name, &namecap, stdin);
    if (namelen <= 0) {
      fprintf(stderr, "(%s: cannot read: %s)\n", loginname, strerror(errno));
      abort();
    }
    if (name[namelen - 1] == '\n')
      name[--namelen] = '\0'; // remove trailing \n

    printf("password: ");
    fflush(stdout);
    passwordlen = getpassword(&password, &passwordcap, stdin);
    if (passwordlen <= 0) {
      fprintf(stderr, "(%s: cannot read: %s)\n", loginname, strerror(errno));
      abort();
    }
    if (password[passwordlen - 1] == '\n')
      password[--passwordlen] = '\0'; // remove trailing \n

    // get the /etc/passwd entry for the user
    errno = 0;
    struct passwd *pwd = getpwnam(name);
    if (!pwd) {
      if (errno == 0)
        fprintf(stderr, "(%s: invalid user)\n", loginname);
      else
        fprintf(stderr, "(%s: cannot getpwnam: %s)\n", loginname,
                strerror(errno));
      continue;
    }

    if (checkpassword(pwd, password) != 1) {
      fprintf(stderr, "(%s: invalid password)\n", loginname);
      continue;
    }

    // setup user environ
    char *home = pwd->pw_dir;
    char *shell = (pwd->pw_shell && *pwd->pw_shell) ? pwd->pw_shell : SH;
    setenv("HOME", home, 1);
    setenv("SHELL", shell, 1);
    chdir(home);
    umask(S_IWGRP | S_IWOTH);

    // switch user/group
    if (initgroups(name, pwd->pw_gid) < 0 ||
        setresgid(pwd->pw_gid, pwd->pw_gid, pwd->pw_gid) < 0 ||
        setresuid(pwd->pw_uid, pwd->pw_uid, pwd->pw_uid) < 0) {
      printf("(%s: login fail: %s)\n", loginname, strerror(errno));
      abort();
    }

    // start shell
    char *const shargv[] = {shell, NULL};
    if (execve(shell, shargv, environ) < 0)
      printf("(%s: shell: %s)\n", loginname, strerror(errno));
    abort();
  }
}
