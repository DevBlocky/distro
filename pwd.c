#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
  char *cwd = getcwd(NULL, 0);
  printf("%s\n", cwd);
  free(cwd);
  return EXIT_SUCCESS;
}
