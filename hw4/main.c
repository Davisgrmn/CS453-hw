#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "lawn.h"
#include "mole.h"
#include "mtq.h"
#include "threads.h"
#include "error.h"

typedef struct {
  Lawn lawn;
  Mtq queue;
  size_t n;
} Simulation;

static void *produce(void *arg) {
  Simulation *s = arg;
  /* Slow simulation work happens outside the queue lock. */
  mtq_tail_put(s->queue, mole_new(s->lawn, 0, 0));
  return NULL;
}

static void *consume(void *arg) {
  Simulation *s = arg;
  mole_whack(mtq_head_get(s->queue));
  return NULL;
}

static void *run(void *arg) {
  Simulation *s = arg;
  /* Start both groups before joining either: a full/empty queue must not
   * prevent the other group from starting. Each worker handles one mole.
   */
  Threads consumers = threads_start(s->n, consume, s);
  Threads producers = threads_start(s->n, produce, s);
  threads_join(producers);
  threads_join(consumers);
  lawn_stop(s->lawn);
  return NULL;
}

static int number(const char *text) {
  char *end;
  errno = 0;
  long n = strtol(text, &end, 10);
  if (errno || !*text || *end || n < 0 || n > INT_MAX)
    ERROR("expected an integer from 0 to %d: %s", INT_MAX, text);
  return (int)n;
}

int main(int argc, char **argv) {
  if (argc > 3) {
    fprintf(stderr, "usage: %s [moles [capacity]]\n", argv[0]);
    return 1;
  }
  int n = argc > 1 ? number(argv[1]) : 10;
  int capacity = argc > 2 ? number(argv[2]) : 4;
  srandom((unsigned int)time(NULL));
  Simulation simulation = {lawn_new(0, 0), mtq_new(capacity), (size_t)n};
  Threads coordinator = threads_start(1, run, &simulation);
  lawn_run(simulation.lawn); /* FLTK's event loop belongs to the main thread. */
  threads_join(coordinator);
  mtq_del(simulation.queue, NULL); /* Equal counts drain the queue. */
  lawn_free(simulation.lawn);
  return 0;
}
