#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

#include "threads.h"
#include "pthread_check.h"

typedef struct {
  size_t n;
  pthread_t ids[];
} *Rep;

extern Threads threads_start(size_t n, ThreadFunction function, void *arg) {
  if (!function)
    ERROR("threads_start: null function");
  Rep r;
  if (n > (SIZE_MAX - sizeof(*r)) / sizeof(pthread_t))
    ERROR("threads_start: thread count is too large");
  r = malloc(sizeof(*r) + n * sizeof(pthread_t));
  if (!r)
    ERROR("threads_start: malloc() failed");
  r->n = n;
  for (size_t i = 0; i < n; i++)
    PTHREAD_CHECK(pthread_create(&r->ids[i], NULL, function, arg));
  return r;
}

extern void threads_join(Threads threads) {
  Rep r = threads;
  if (!r)
    ERROR("threads_join: null group");
  for (size_t i = 0; i < r->n; i++)
    PTHREAD_CHECK(pthread_join(r->ids[i], NULL));
  free(r);
}
