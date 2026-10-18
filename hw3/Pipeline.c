#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "Pipeline.h"
#include "deq.h"
#include "error.h"

typedef struct {
  Deq processes;
  int fg;
  pid_t *pids;
  int children;
} *PipelineRep;

extern Pipeline newPipeline(int fg) {
  PipelineRep r = calloc(1, sizeof(*r));
  if (!r)
    ERROR("calloc() failed");
  r->processes = deq_new();
  r->fg = fg;
  return r;
}

extern void addPipeline(Pipeline pipeline, Command command) {
  PipelineRep r = pipeline;
  deq_tail_put(r->processes, command);
}

extern int sizePipeline(Pipeline pipeline) {
  PipelineRep r = pipeline;
  return deq_len(r->processes);
}

static void connect_fd(int fd, int target) {
  if (dup2(fd, target) == -1) {
    perror("dup2");
    _exit(1);
  }
}

extern void execPipeline(Pipeline pipeline, Jobs jobs, int *eof) {
  PipelineRep r = pipeline;
  int n = sizePipeline(pipeline);
  Command first = deq_head_ith(r->processes, 0);

  if (r->fg && n == 1 && isBuiltinCommand(first)) {
    execCommand(first, eof, 0);
    freePipeline(pipeline);
    return;
  }

  r->pids = calloc(n, sizeof(*r->pids));
  if (!r->pids)
    ERROR("calloc() failed");
  int previous = -1;
  fflush(NULL);
  for (int i = 0; i < n; i++) {
    int fds[2] = {-1, -1};
    if (i + 1 < n && pipe(fds) == -1) {
      perror("pipe");
      break;
    }
    pid_t pid = fork();
    if (pid == -1) {
      perror("fork");
      if (fds[0] != -1) close(fds[0]);
      if (fds[1] != -1) close(fds[1]);
      break;
    }
    if (pid == 0) {
      signal(SIGINT, r->fg ? SIG_DFL : SIG_IGN);
      signal(SIGQUIT, r->fg ? SIG_DFL : SIG_IGN);
      signal(SIGPIPE, SIG_DFL);
      if (previous != -1) {
        connect_fd(previous, STDIN_FILENO);
      } else if (!r->fg) {
        /* Without job control, background commands must not consume input
           intended for the shell. Explicit '< file' takes precedence below. */
        int nullfd = open("/dev/null", O_RDONLY);
        if (nullfd == -1) {
          perror("/dev/null");
          _exit(1);
        }
        connect_fd(nullfd, STDIN_FILENO);
        if (nullfd != STDIN_FILENO) close(nullfd);
      }
      if (fds[1] != -1) connect_fd(fds[1], STDOUT_FILENO);
      if (previous != -1) close(previous);
      if (fds[0] != -1) close(fds[0]);
      if (fds[1] != -1) close(fds[1]);
      int child_eof = 0;
      int status = execCommand(deq_head_ith(r->processes, i), &child_eof, 1);
      /* A child builtin must never return to the shell's input loop. */
      _exit(status);
    }
    r->pids[r->children++] = pid;
    if (previous != -1) close(previous);
    if (fds[1] != -1) close(fds[1]);
    previous = fds[0];
  }
  if (previous != -1) close(previous);

  /* Launch every stage before waiting, so a pipe larger than its buffer
     cannot deadlock. Background jobs take ownership of their pipeline. */
  if (r->fg) {
    waitPipeline(pipeline, 1);
    freePipeline(pipeline);
  } else {
    addJobs(jobs, pipeline);
  }
}

extern int waitPipeline(Pipeline pipeline, int block) {
  PipelineRep r = pipeline;
  int complete = 1;
  for (int i = 0; i < r->children; i++) {
    if (!r->pids[i])
      continue;
    pid_t result;
    do {
      result = waitpid(r->pids[i], NULL, block ? 0 : WNOHANG);
    } while (result == -1 && errno == EINTR);
    if (result == 0) {
      complete = 0;
    } else {
      if (result == -1 && errno != ECHILD)
        perror("waitpid");
      r->pids[i] = 0;
    }
  }
  return complete;
}

extern void freePipeline(Pipeline pipeline) {
  PipelineRep r = pipeline;
  deq_del(r->processes, freeCommand);
  free(r->pids);
  free(r);
}
