#include <stdio.h>
#include <stdlib.h>

extern char **environ;

int main(int argc, char **argv) {
  char **envp = environ;
  if (!*envp)
    puts("(empty)");
  while (*envp)
    printf("%s\n", *envp++);
  return EXIT_SUCCESS;
}
