#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/history.h>

#include "Command.h"
#include "error.h"

typedef struct {
  char **argv;
  int argc;
  char *input;
  char *output;
} *CommandRep;

typedef int (*BuiltinFunction)(CommandRep, int *);

static char *owd;

static int builtin_args(CommandRep r, int n) {
  if (r->argc == n + 1)
    return 1;
  fprintf(stderr, "%s: expected %d argument%s\n",
          r->argv[0], n, n == 1 ? "" : "s");
  return 0;
}

static int bi_exit(CommandRep r, int *eof) {
  if (!builtin_args(r, 0))
    return 1;
  *eof = 1;
  return 0;
}

static int bi_pwd(CommandRep r, int *eof) {
  (void)eof;
  if (!builtin_args(r, 0))
    return 1;
  char *cwd = getcwd(NULL, 0);
  if (!cwd) {
    perror("pwd");
    return 1;
  }
  puts(cwd);
  free(cwd);
  return 0;
}

static int bi_cd(CommandRep r, int *eof) {
  (void)eof;
  if (!builtin_args(r, 1))
    return 1;
  const char *dir = r->argv[1];
  if (strcmp(dir, "-") == 0) {
    if (!owd) {
      fprintf(stderr, "cd: no previous working directory\n");
      return 1;
    }
    dir = owd;
  }
  char *previous = getcwd(NULL, 0);
  if (!previous) {
    perror("cd");
    return 1;
  }
  if (chdir(dir) == -1) {
    perror("cd");
    free(previous);
    return 1;
  }
  /* Update the previous directory only after chdir succeeds. */
  free(owd);
  owd = previous;
  return 0;
}

static int bi_history(CommandRep r, int *eof) {
  (void)eof;
  if (!builtin_args(r, 0))
    return 1;
  HIST_ENTRY **entries = history_list();
  if (entries)
    for (int i = 0; entries[i]; i++)
      printf("%d %s\n", history_base + i, entries[i]->line);
  return 0;
}

static BuiltinFunction builtin(CommandRep r) {
  static const struct {
    const char *name;
    BuiltinFunction function;
  } builtins[] = {
    {"exit", bi_exit},
    {"pwd", bi_pwd},
    {"cd", bi_cd},
    {"history", bi_history},
    {NULL, NULL}
  };
  for (int i = 0; builtins[i].name; i++)
    if (strcmp(r->argv[0], builtins[i].name) == 0)
      return builtins[i].function;
  return NULL;
}

extern Command newCommand(T_command tree) {
  CommandRep r = calloc(1, sizeof(*r));
  if (!r)
    ERROR("calloc() failed");
  for (T_words p = tree->words; p; p = p->words)
    r->argc++;
  r->argv = calloc(r->argc + 1, sizeof(*r->argv));
  if (!r->argv)
    ERROR("calloc() failed");
  int i = 0;
  for (T_words p = tree->words; p; p = p->words) {
    r->argv[i] = strdup(p->word->s);
    if (!r->argv[i++])
      ERROR("strdup() failed");
  }
  if (tree->input && !(r->input = strdup(tree->input)))
    ERROR("strdup() failed");
  if (tree->output && !(r->output = strdup(tree->output)))
    ERROR("strdup() failed");
  return r;
}

extern int isBuiltinCommand(Command command) {
  return builtin(command) != NULL;
}

static int redirect(const char *path, int target, int flags) {
  int fd = open(path, flags, 0666);
  if (fd == -1) {
    perror(path);
    return -1;
  }
  if (dup2(fd, target) == -1) {
    perror("dup2");
    if (fd != target)
      close(fd);
    return -1;
  }
  if (fd != target)
    close(fd);
  return 0;
}

extern int execCommand(Command command, int *eof, int child) {
  CommandRep r = command;
  int saved_input = -1, saved_output = -1;
  int status = 1;

  /* Flush before changing descriptors, including buffered parent builtins. */
  fflush(NULL);
  if (!child) {
    if (r->input && (saved_input = dup(STDIN_FILENO)) == -1) {
      perror("dup");
      goto restore;
    }
    if (r->output && (saved_output = dup(STDOUT_FILENO)) == -1) {
      perror("dup");
      goto restore;
    }
  }
  if (r->input && redirect(r->input, STDIN_FILENO, O_RDONLY) == -1)
    goto restore;
  if (r->output && redirect(r->output, STDOUT_FILENO,
                            O_WRONLY | O_CREAT | O_TRUNC) == -1)
    goto restore;

  BuiltinFunction function = builtin(r);
  if (function) {
    status = function(r, eof);
  } else {
    execvp(r->argv[0], r->argv);
    status = errno == ENOENT ? 127 : 126;
    perror(r->argv[0]);
  }

restore:
  fflush(NULL);
  if (saved_input != -1) {
    if (dup2(saved_input, STDIN_FILENO) == -1)
      ERROR("restoring stdin failed");
    close(saved_input);
  }
  if (saved_output != -1) {
    if (dup2(saved_output, STDOUT_FILENO) == -1)
      ERROR("restoring stdout failed");
    close(saved_output);
    clearerr(stdout);
  }
  return status;
}

extern void freeCommand(Command command) {
  CommandRep r = command;
  for (int i = 0; i < r->argc; i++)
    free(r->argv[i]);
  free(r->argv);
  free(r->input);
  free(r->output);
  free(r);
}

extern void freestateCommand(void) {
  free(owd);
  owd = NULL;
}
