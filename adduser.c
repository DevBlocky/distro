#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#include "password.h"
#include <grp.h>
#include <pwd.h>
#include <shadow.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define HOMEPREFIX "/home/"

int main(int argc, char **argv) {
  if (geteuid() != 0) {
    fprintf(stderr, "(%s: must be run as root)\n", argv[0]);
    return EXIT_FAILURE;
  }
  if (argc != 2) {
    fprintf(stderr, "usage: %s <login>\n", argv[0]);
    return EXIT_FAILURE;
  }
  char *login = argv[1];
  if (*login >= '0' && *login <= '9') {
    fprintf(stderr, "(%s: login cannot start with number)\n", argv[0]);
    return EXIT_FAILURE;
  }
  for (char *check = login; *check != '\0'; check++) {
    if ((*check < '0' || *check > '9') && (*check < 'a' || *check > 'z')) {
      fprintf(stderr, "(%s: login must be lowercase alphanumeric)\n", argv[0]);
      return EXIT_FAILURE;
    }
  }

  // get password and confirm it
  char *password = NULL;
  ssize_t passwordlen = 0;
  for (int i = 0; i < 2; i++) {
    char *p = NULL;
    size_t cap = 0;
    if (!password)
      printf("password: ");
    else
      printf("confirm password: ");
    fflush(NULL);
    ssize_t len = getpassword(&p, &cap, stdin);
    if (len <= 0) {
      fprintf(stderr, "(%s: cannot read: %s)\n", argv[0], strerror(errno));
      abort();
    }
    if (p[len - 1] == '\n')
      p[--len] = '\0'; // remove trailing \n

    if (!password) {
      password = p;
      passwordlen = len;
    } else if (strcmp(password, p) != 0) {
      // confirm password failed
      fprintf(stderr, "(%s: mismatched password)\n", argv[0]);
      return EXIT_FAILURE;
    } else {
      free(p);
    }
  }

  // make sure the user does not already exist
  size_t loginlen = strlen(login);
  errno = 0;
  if (getpwnam(login) != NULL) {
    fprintf(stderr, "(%s: user already exists)\n", argv[0]);
    return EXIT_FAILURE;
  } else if (errno != 0) {
    fprintf(stderr, "(%s: cannot getpwnam: %s)\n", argv[0], strerror(errno));
    abort();
  }

  // find a new uid for the user
  uid_t maxid = 999;
  struct passwd *pwd;
  setpwent();
  while ((pwd = getpwent()) != NULL)
    if (pwd->pw_uid > maxid)
      maxid = pwd->pw_uid;
  endpwent();
  setgrent();
  struct group *grp;
  while ((grp = getgrent()) != NULL)
    if (grp->gr_gid > maxid)
      maxid = grp->gr_gid;
  endgrent();

  // create the password hash if password isn't blank
  char *hash = NULL;
  if (*password && newpassword(password, &hash, 6) != 0) {
    fprintf(stderr, "(%s: could not make new password)\n", argv[0]);
    return EXIT_FAILURE;
  }

  // emplace new /etc/passwd entry
  char *homedir = malloc(sizeof(HOMEPREFIX) + loginlen);
  strncpy(homedir, HOMEPREFIX, sizeof(HOMEPREFIX));
  strncpy(&homedir[sizeof(HOMEPREFIX) - 1], login, loginlen + 1);
  struct passwd newpwd = {0};
  newpwd.pw_name = login;
  newpwd.pw_passwd = hash ? "x" : "";
  newpwd.pw_uid = maxid + 1;
  newpwd.pw_gid = maxid + 1;
  newpwd.pw_gecos = "";
  newpwd.pw_dir = homedir;
  newpwd.pw_shell = "/usr/bin/sh";
  FILE *fpasswd = fopen("/etc/passwd", "a");
  if (fpasswd == NULL) {
    fprintf(stderr, "(%s: cannot open /etc/passwd: %s)\n", argv[0],
            strerror(errno));
    abort();
  }
  putpwent(&newpwd, fpasswd);
  fclose(fpasswd);

  // emplace new /etc/group entry
  char *mem[] = {NULL};
  struct group newgrp = {0};
  newgrp.gr_name = login;
  newgrp.gr_passwd = "x";
  newgrp.gr_gid = maxid + 1;
  newgrp.gr_mem = mem;
  FILE *fgroup = fopen("/etc/group", "a");
  if (fgroup == NULL) {
    fprintf(stderr, "(%s: cannot open /etc/group: %s)\n", argv[0],
            strerror(errno));
    abort();
  }
  putgrent(&newgrp, fgroup);
  fclose(fgroup);

  // emplace new /etc/shadow entry (if required)
  if (hash) {
    struct spwd newshdw = {0};
    newshdw.sp_namp = login;
    newshdw.sp_pwdp = hash;
    // TODO: set additional flags
    FILE *fshadow = fopen("/etc/shadow", "a");
    if (fshadow == NULL) {
      fprintf(stderr, "(%s: cannot open /etc/shadow: %s)\n", argv[0],
              strerror(errno));
      abort();
    }
    putspent(&newshdw, fshadow);
    fclose(fshadow);
  }

  // make the home directory for the new user
  mkdir("/home", 0755);
  if (mkdir(homedir, 0700) < 0 || chown(homedir, maxid + 1, maxid + 1) < 0) {
    fprintf(stderr, "(%s: cannot make home directory: %s)\n", argv[0],
            strerror(errno));
    abort();
  }

  return EXIT_SUCCESS;
}
