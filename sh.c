#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#undef NDEBUG
#include <assert.h>

extern char **environ;
char *shname;

static void handlesig(int signo) { (void)signo; }

static void strappend(char **buf, size_t *len, size_t *cap, char c) {
  if (*len >= *cap) {
    *cap = *cap ? *cap * 2 : 16;
    *buf = realloc(*buf, *cap * sizeof(**buf));
  }
  (*buf)[(*len)++] = c;
}

struct tok {
  char *str;
  int literal;
};
struct tokens {
  struct tok *data;
  size_t len;
  size_t cap;
};
static void pushtokens(struct tokens *vec, char *str, int literal) {
  if (vec->len >= vec->cap) {
    vec->cap = vec->cap ? vec->cap * 2 : 16;
    vec->data = realloc(vec->data, vec->cap * sizeof(*vec->data));
  }
  struct tok tok = {.str = str, .literal = literal};
  vec->data[vec->len++] = tok;
}
static void freetokens(struct tokens *vec) {
  for (size_t i = 0; i < vec->len; i++)
    free(vec->data[i].str);
  free(vec->data);
  *vec = (struct tokens){0};
}

static int isoperator(char c) { return c == '>' || c == '<' || c == '|'; }
static int isquote(char c) { return c == '"' || c == '\''; }
static void tokenize(const unsigned char *inp, struct tokens *out) {
  while (*inp) {
    // move through spaces
    while (isspace(*inp))
      inp++;
    if (!*inp)
      break;

    // parse operator
    if (isoperator(*inp)) {
      char *optok = malloc(sizeof(char) * 2);
      optok[0] = *inp++;
      optok[1] = '\0';
      pushtokens(out, optok, 0);
      continue;
    }

    // parse text/quote
    int literal = 0;
    char *buf = NULL;
    size_t len = 0, cap = 0;
    while (*inp && !isspace(*inp) && !isoperator(*inp)) {
      if (isquote(*inp)) {
        literal = 1;
        unsigned char quote = *inp++;
        while (*inp && *inp != quote)
          strappend(&buf, &len, &cap, *inp++);
        if (*inp == quote)
          inp++;
      } else {
        strappend(&buf, &len, &cap, *inp++);
      }
    }
    strappend(&buf, &len, &cap, '\0');
    pushtokens(out, buf, literal);
  }
}

struct command {
  char **argv;
  size_t argc;
  char *fin;
  char *fout;
};
struct pipeline {
  struct command *cmds;
  size_t len;
  size_t cap;
};
static void pushpipeline(struct pipeline *p, struct command *cmd) {
  if (p->len >= p->cap) {
    p->cap = p->cap ? p->cap * 2 : 4;
    p->cmds = realloc(p->cmds, p->cap * sizeof(*p->cmds));
  }
  p->cmds[p->len++] = *cmd;
}
static void freepipeline(struct pipeline *p) {
  // in mkpipeline, only the cmds ptr and argv ptr
  // is new, everything else is a reference to tokens
  for (size_t i = 0; i < p->len; i++)
    free(p->cmds[i].argv);
  free(p->cmds);
  *p = (struct pipeline){0};
}

static int parseredir(const struct tokens *tokens, size_t *i,
                      struct command *cmd, const char *op) {
  if (*i >= tokens->len) {
    fprintf(stderr, "(%s: empty redirection)\n", shname);
    return -2;
  }
  struct tok target = tokens->data[(*i)++];
  if (!target.literal && isoperator(target.str[0])) {
    fprintf(stderr, "(%s: bad redirection target)\n", shname);
    return -2;
  }
  if (op[0] == '<')
    cmd->fin = target.str;
  else
    cmd->fout = target.str;
  return 0;
}
static int mkpipeline(const struct tokens *tokens, struct pipeline *out) {
  for (size_t i = 0; i < tokens->len;) {
    struct command cmd = (struct command){0};
    size_t argcap = 0;
    int pipe = 0;
    int err = 0;

    // parse single command
    while (i < tokens->len) {
      struct tok tok = tokens->data[i++];

      // parse operators
      if (!tok.literal && tok.str[1] == '\0') {
        switch (tok.str[0]) {
        case '|':
          pipe = 1;
          goto done;
        case '<':
        case '>':
          err = parseredir(tokens, &i, &cmd, tok.str);
          if (err)
            goto done;
          continue;
        }
      }

      // arguments are not allowed after redirection
      if (cmd.fin || cmd.fout) {
        fprintf(stderr, "(%s: unexpected token \"%s\" after command)\n", shname,
                tok.str);
        err = -3;
        goto done;
      }

      // add argument to argv
      if ((cmd.argc + 1) >= argcap) {
        argcap = argcap ? argcap * 2 : 8;
        cmd.argv = realloc(cmd.argv, argcap * sizeof(*cmd.argv));
      }
      cmd.argv[cmd.argc++] = tok.str;
    }

  done:
    // add command to pipeline
    if (cmd.argc == 0) {
      fprintf(stderr, "(%s: empty command)\n", shname);
      return -1;
    }
    cmd.argv[cmd.argc] = NULL;
    pushpipeline(out, &cmd);

    if (err)
      return err;
    if (pipe && i >= tokens->len) {
      fprintf(stderr, "(%s: unexpected end of command)\n", shname);
      return -4;
    }
  }
  return 0;
}

enum builtin {
  BUILTIN_NONE,
  BUILTIN_EXIT,
  BUILTIN_CD,
  BUILTIN_CLEAR,
};
static enum builtin getbuiltin(const char *s) {
  if (strcmp(s, "exit") == 0)
    return BUILTIN_EXIT;
  if (strcmp(s, "cd") == 0)
    return BUILTIN_CD;
  if (strcmp(s, "clear") == 0)
    return BUILTIN_CLEAR;
  return BUILTIN_NONE;
}
static int execbuiltin(enum builtin typ, size_t argc, char **argv) {
  int res = 0;
  switch (typ) {
  case BUILTIN_EXIT:
    if (argc != 1)
      return -16;
    exit(1);
    break;
  case BUILTIN_CD:
    if (argc != 2)
      return -16;
    res = chdir(argv[1]);
    if (res < 0)
      fprintf(stderr, "(%s: cd: %s)\n", shname, strerror(errno));
    break;
  case BUILTIN_CLEAR:
    if (argc != 1)
      return -16;
    // CSI commands: clear screen, return cursor to 1;1
    if (isatty(STDOUT_FILENO))
      printf("\033[2J\033[;H");
    break;
  case BUILTIN_NONE:
  }
  return res;
}
static pid_t execcommand(const struct command *cmd, int in, int out) {
  char *cmdname = cmd->argv[0];
  enum builtin typ = getbuiltin(cmdname);
  if (typ != BUILTIN_NONE) {
    int status = execbuiltin(typ, cmd->argc, cmd->argv);
    if (status == -16)
      fprintf(stderr, "(%s: %s: invalid number of arguments)\n", shname,
              cmdname);
    return status;
  }

  fflush(NULL); // make sure child doesn't have buffered data
  pid_t pid = fork();
  if (pid < 0) {
    fprintf(stderr, "(%s: %s spawn: %s)\n", shname, cmdname, strerror(errno));
    return -1;
  }
  if (pid != 0)
    return pid;

  // we are the child process
  // NOTE: we close in/out again because this
  // is a separate process with its own FDs
  if (in >= 0) {
    dup2(in, STDIN_FILENO);
    close(in);
  }
  if (out >= 0) {
    dup2(out, STDOUT_FILENO);
    close(out);
  }

  struct sigaction dfl = {0};
  dfl.sa_handler = SIG_DFL;
  sigemptyset(&dfl.sa_mask);
  dfl.sa_flags = 0;
  sigaction(SIGINT, &dfl, NULL);
  sigaction(SIGQUIT, &dfl, NULL);

  execvp(cmdname, cmd->argv);
  fprintf(stderr, "(%s: %s exec: %s)\n", shname, cmdname, strerror(errno));
  abort();
}
static int execpipeline(const struct pipeline *p) {
  pid_t *pids = malloc(p->len * sizeof(*pids));

  int pipefd[2] = {-1, -1};
  for (size_t i = 0; i < p->len; i++) {
    int in = pipefd[0]; // last pipe read-end
    if (i < p->len - 1)
      assert(pipe(pipefd) == 0);
    else
      pipefd[1] = -1;
    int out = pipefd[1]; // new pipe write-end

    struct command *cmd = &p->cmds[i];

    // setup redirection
    if (cmd->fin) {
      int fd = open(cmd->fin, O_RDONLY);
      if (fd < 0) {
        fprintf(stderr, "(%s: could not open %s: %s)\n", shname, cmd->fin,
                strerror(errno));
        return -1;
      }
      if (in >= 0)
        close(in);
      in = fd;
    }
    if (cmd->fout) {
      int fd = open(cmd->fout, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd < 0) {
        fprintf(stderr, "(%s: could not open %s: %s)\n", shname, cmd->fout,
                strerror(errno));
        return -1;
      }
      if (out >= 0)
        close(out);
      out = fd;
    }

    // execute command with input/output fds, then cleanup
    pids[i] = execcommand(cmd, in, out);
    if (in >= 0)
      close(in);
    if (out >= 0)
      close(out);
  }

  // wait for last in pipeline to finish
  for (size_t i = 0; i < p->len; i++) {
    if (pids[i] <= 0)
      continue;
    char *cmdname = p->cmds[i].argv[0];
    int status;
    while (waitpid(pids[i], &status, 0) < 0) {
      if (errno == EINTR)
        continue;
      fprintf(stderr, "(%s: %s: wait failed: %s)\n", shname, cmdname,
              strerror(errno));
      status = -1;
      break;
    }
    if (status < 0)
      continue;
    if (WIFSIGNALED(status)) {
      if (WTERMSIG(status) == SIGSEGV)
        fprintf(stderr, "(%s: %s: segmentation fault)\n", shname, cmdname);
      if (WTERMSIG(status) == SIGABRT)
        fprintf(stderr, "(%s: %s: aborted)\n", shname, cmdname);
    }
  }

  free(pids);
  return 0;
}

int main(int argc, char **argv) {
  shname = argv[0];

  if (isatty(STDIN_FILENO)) {
    struct sigaction sa = {0};
    sa.sa_handler = handlesig;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
  }

  char *line = NULL;
  size_t linesz = 0;
  ssize_t read;
  while (feof(stdin) == 0) {
    char *cwd = getcwd(NULL, 0);
    printf("%s# ", cwd);
    free(cwd);
    fflush(stdout);

    read = getline(&line, &linesz, stdin);
    if (read < 0) {
      if (errno == EINTR) {
        clearerr(stdin);
        putchar('\n');
        continue;
      }
      printf("(%s: could not read input: %s)\n", shname, strerror(errno));
      return EXIT_FAILURE;
    }

    // tokenize then parse pipeline
    struct tokens tokens = {0};
    tokenize((unsigned char *)line, &tokens);
    struct pipeline p = {0};
    int status = mkpipeline(&tokens, &p);

    if (status == 0) {
      status = execpipeline(&p);
    }

    // cleanup
    // wait until after exec for tokens because they are
    // referenced by the command struct
    freepipeline(&p);
    freetokens(&tokens);
  }
  return EXIT_SUCCESS;
}
