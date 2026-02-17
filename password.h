#ifndef __PASSWORD_H
#define __PASSWORD_H

#include <crypt.h>
#include <errno.h>
#include <pwd.h>
#include <shadow.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

static ssize_t __attribute__((unused)) getpassword(char **line, size_t *len,
                                                   FILE *stream) {
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

static int __attribute__((unused)) checkpassword(struct passwd *pwd,
                                                 char *given) {
  // get the password (either from /etc/passwd or /etc/shadow)
  char *passwd = pwd->pw_passwd;
  if (strcmp(passwd, "x") == 0) {
    // get shadow password
    errno = 0;
    struct spwd *shdw = getspnam(pwd->pw_name);
    if (shdw == NULL) {
      if (errno != 0)
        return -1;
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

static int __attribute__((unused)) newpassword(const char *plaintext,
                                                char **hash, int saltlen) {
  static const char alphabet[] =
      "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

  // get the raw salt from /dev/urandom
  unsigned char saltraw[saltlen];
  FILE *rand = fopen("/dev/urandom", "r");
  if (!rand)
    return -1;
  int total = 0;
  while (total < saltlen) {
    ssize_t r = fread(saltraw + total, 1, saltlen - total, rand);
    if (r <= 0)
      return -1;
    total += r;
  }
  fclose(rand);

  // turn the saltraw into a setting for crypt
  char setting[3 + saltlen + 1];
  setting[0] = '$';
  setting[1] = '6';
  setting[2] = '$';
  for (int i = 0; i < saltlen; i++)
    setting[i + 3] = alphabet[saltraw[i] % (sizeof(alphabet) - 1)];
  setting[saltlen + 3] = '\0';

  *hash = crypt(plaintext, setting);
  return *hash == NULL ? -1 : 0;
}

#endif // __PASSWORD_H
