#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "Command.h"
#include "Jobs.h"
#include "Parser.h"
#include "error.h"

static Jobs jobs;

/* Readline calls this while waiting for input, so completed background jobs
   are reaped even when the user leaves the shell sitting at its prompt. */
static int reap_background(void) {
  reapJobs(jobs);
  return 0;
}

int main(void) {
  int eof = 0;
  int interactive = isatty(STDIN_FILENO);
  char *history_file = NULL;
  FILE *quiet = NULL;
  jobs = newJobs();
  using_history();

  if (interactive) {
    char *cwd = getcwd(NULL, 0);
    if (!cwd || asprintf(&history_file, "%s/.history", cwd) == -1)
      ERROR("cannot determine history filename");
    free(cwd);
    read_history(history_file);
  } else {
    rl_bind_key('\t', rl_insert);
    quiet = fopen("/dev/null", "w");
    if (!quiet)
      ERROR("cannot open /dev/null");
    if (fcntl(fileno(quiet), F_SETFD, FD_CLOEXEC) == -1)
      ERROR("fcntl() failed");
    rl_outstream = quiet;
  }

  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  /* An event hook on a non-terminal can keep Readline spinning at EOF. */
  rl_event_hook = interactive ? reap_background : NULL;
  while (!eof) {
    reapJobs(jobs);
    char *line = readline(interactive ? "$ " : NULL);
    if (!line)
      break;
    if (*line)
      add_history(line);
    interpret(line, &eof, jobs);
    free(line);
    reapJobs(jobs);
  }
  rl_event_hook = NULL;
  /* EOF and the exit builtin both wait for every remaining background job. */
  freeJobs(jobs);
  if (interactive && write_history(history_file) != 0)
    WARN("could not write command history");
  free(history_file);
  rl_clear_history();
  if (quiet)
    fclose(quiet);
  freestateCommand();
  return 0;
}
