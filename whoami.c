#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int main(void) {
  uid_t uid = getuid();
  struct passwd *pwd = getpwuid(uid);
  if (pwd)
    printf("uid=%u (%s)\n", uid, pwd->pw_name);
  else
    printf("uid=%u\n", uid);

  uid_t euid = geteuid();
  if (euid != uid) {
    struct passwd *epwd = getpwuid(euid);
    if (epwd)
      printf("euid=%u (%s)\n", euid, epwd->pw_name);
    else
      printf("euid=%u\n", euid);
  }
  return EXIT_SUCCESS;
}