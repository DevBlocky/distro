#ifndef __PASSWORD_H
#define __PASSWORD_H

#include <errno.h>
#include <pwd.h>
#include <shadow.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

ssize_t getpassword(char **line, size_t *len, FILE *stream) {
  // disable terminal echo
  struct termios olda, newa;
  if (tcgetattr(fileno(stream), &olda) != 0)
    return -1;
  newa = olda;
  newa.c_lflag &= ~ECHO;
  if (tcsetattr(fileno(stream), TCSAFLUSH, &newa) != 0)
    return -1;

  ssize_t n = getline(line, len, stream);

  // reset terminal
  tcsetattr(fileno(stream), TCSAFLUSH, &olda);
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
        return -1;
      // fprintf(stderr, "(%s: cannot getspnam: %s)\n", loginname,
      //         strerror(errno));
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

#endif // __PASSWORD_H