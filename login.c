#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <shadow.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#define SH "/usr/bin/sh"

extern char **environ;
char *loginname;

ssize_t getpassword(char **line, size_t *len, FILE *stream) {
  // disable terminal echo
  struct termios old, new;
  if (tcgetattr(fileno(stream), &old) != 0)
    return -1;
  new = old;
  new.c_lflag &= ~ECHO;
  if (tcsetattr(fileno(stream), TCSAFLUSH, &new) != 0)
    return -1;

  ssize_t n = getline(line, len, stream);

  // reset terminal
  tcsetattr(fileno(stream), TCSAFLUSH, &old);
  putchar('\n');

  return n;
}

int checkpassword(struct passwd *pwd, char *given) {
  // get the password (either from /etc/passwd or /etc/shadow)
  char *passwd = pwd->pw_passwd;
  if (strcmp(passwd, "x") == 0) {
    // get shadow password
    errno = 0;
    struct spwd *shdw = getspnam(pwd->pw_name);
    if (shdw == NULL) {
      if (errno != 0)
        fprintf(stderr, "(%s: cannot getspnam: %s)\n", loginname,
                strerror(errno));
      return 0;
    }
    passwd = shdw->sp_pwdp;
  }

  // special password cases (blank is always, * is never)
  if (strcmp(passwd, "") == 0)
    return 1;
  if (strcmp(passwd, "*") == 0)
    return 0;

  // compare hash of given and lookup
  char *calc = crypt(given, passwd);
  return !!calc && strcmp(calc, passwd) == 0;
}

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

    if (strcmp(name, "exit") == 0)
      return EXIT_SUCCESS;

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
