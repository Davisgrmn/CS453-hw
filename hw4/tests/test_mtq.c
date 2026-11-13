#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "mtq.h"
#include "threads.h"
#include "pthread_check.h"

static int sum;
static void add(Data d) { sum += *(int *)d; }
static Str number(Data d) {
  char *s;
  assert(asprintf(&s, "%d", *(int *)d) >= 0);
  return s;
}

static void test_operations(void) {
  Mtq q = mtq_new(0);
  int a = 1, b = 2, c = 3, missing = 4;
  assert(mtq_len(q) == 0);
  assert(!mtq_head_ith(q, 0) && !mtq_tail_rem(q, &missing));
  mtq_head_put(q, &b);
  mtq_head_put(q, &a);
  mtq_tail_put(q, &c);
  assert(mtq_len(q) == 3);
  assert(mtq_head_ith(q, 0) == &a && mtq_tail_ith(q, 0) == &c);
  assert(mtq_head_ith(q, 1) == &b && mtq_tail_ith(q, 1) == &b);
  assert(!mtq_head_ith(q, -1) && !mtq_tail_ith(q, 3));
  assert(!mtq_head_rem(q, &missing));
  sum = 0;
  mtq_map(q, add);
  assert(sum == 6);
  char *text = mtq_str(q, number);
  assert(strcmp(text, "1 2 3") == 0);
  free(text);
  mtq_tail_put(q, &b); // a, b, c, b: remove the match nearest the chosen end.
  assert(mtq_tail_rem(q, &b) == &b && mtq_tail_ith(q, 0) == &c);
  assert(mtq_head_rem(q, &b) == &b && mtq_len(q) == 2);
  assert(mtq_tail_get(q) == &c && mtq_head_get(q) == &a);
  mtq_tail_put(q, NULL);
  assert(mtq_len(q) == 1);
  assert(mtq_head_get(q) == NULL && mtq_len(q) == 0);
  mtq_del(q, NULL);

  q = mtq_new(0);
  for (int i = 0; i < 500; i++) {
    int *item = malloc(sizeof(*item));
    assert(item);
    *item = i;
    mtq_tail_put(q, item);
  }
  assert(mtq_len(q) == 500);
  mtq_del(q, free); // Also test ownership of data left in a queue.

  q = mtq_new(4);
  mtq_tail_put(q, "alpha");
  mtq_tail_put(q, "beta");
  text = mtq_str(q, NULL);
  assert(strcmp(text, "alpha beta") == 0);
  free(text);
  mtq_del(q, NULL);
}

/* A separate condition variable observes worker completion. The timed wait
 * proves that an empty get/full put has not returned before we unblock it;
 * it is not a sleep used to coordinate access to the queue itself.
 */
typedef struct {
  Mtq q;
  pthread_mutex_t mutex;
  pthread_cond_t changed;
  int entered, done, put, tail;
  Data value, result;
} Probe;

static void *probe(void *arg) {
  Probe *p = arg;
  PTHREAD_CHECK(pthread_mutex_lock(&p->mutex));
  p->entered = 1;
  PTHREAD_CHECK(pthread_cond_signal(&p->changed));
  PTHREAD_CHECK(pthread_mutex_unlock(&p->mutex));
  Data result = NULL;
  if (p->put) {
    if (p->tail) mtq_tail_put(p->q, p->value);
    else mtq_head_put(p->q, p->value);
  } else {
    result = p->tail ? mtq_tail_get(p->q) : mtq_head_get(p->q);
  }
  PTHREAD_CHECK(pthread_mutex_lock(&p->mutex));
  p->result = result;
  p->done = 1;
  PTHREAD_CHECK(pthread_cond_signal(&p->changed));
  PTHREAD_CHECK(pthread_mutex_unlock(&p->mutex));
  return NULL;
}

static Threads start_probe(Probe *p) {
  PTHREAD_CHECK(pthread_mutex_init(&p->mutex, NULL));
  PTHREAD_CHECK(pthread_cond_init(&p->changed, NULL));
  Threads t = threads_start(1, probe, p);
  PTHREAD_CHECK(pthread_mutex_lock(&p->mutex));
  while (!p->entered)
    PTHREAD_CHECK(pthread_cond_wait(&p->changed, &p->mutex));
  struct timespec deadline;
  assert(clock_gettime(CLOCK_REALTIME, &deadline) == 0);
  deadline.tv_nsec += 20000000;
  if (deadline.tv_nsec >= 1000000000) {
    deadline.tv_sec++;
    deadline.tv_nsec -= 1000000000;
  }
  while (!p->done) {
    int error = pthread_cond_timedwait(&p->changed, &p->mutex, &deadline);
    if (error == ETIMEDOUT)
      break;
    PTHREAD_CHECK(error);
  }
  assert(!p->done);
  PTHREAD_CHECK(pthread_mutex_unlock(&p->mutex));
  return t;
}

static void finish_probe(Probe *p, Threads t) {
  threads_join(t);
  assert(p->done);
  PTHREAD_CHECK(pthread_cond_destroy(&p->changed));
  PTHREAD_CHECK(pthread_mutex_destroy(&p->mutex));
}

static void test_blocking(void) {
  int item = 42;
  for (int tail = 0; tail < 2; tail++) {
    Mtq q = mtq_new(1);
    Probe p = {.q = q, .tail = tail};
    Threads t = start_probe(&p);
    mtq_tail_put(q, &item);
    finish_probe(&p, t);
    assert(p.result == &item && mtq_len(q) == 0);
    mtq_del(q, NULL);
  }
  // Both put directions must wake after either a get or a successful rem.
  // Stored NULL tests that removal signals based on length, not return value.
  for (int tail = 0; tail < 2; tail++) {
    for (int removal = 0; removal < 3; removal++) {
      Mtq q = mtq_new(1);
      mtq_head_put(q, NULL);
      Probe p = {.q = q, .tail = tail, .put = 1, .value = &item};
      Threads t = start_probe(&p);
      if (removal == 0) assert(mtq_head_get(q) == NULL);
      else if (removal == 1) assert(mtq_head_rem(q, NULL) == NULL);
      else assert(mtq_tail_rem(q, NULL) == NULL);
      finish_probe(&p, t);
      assert(mtq_len(q) == 1 && mtq_tail_get(q) == &item);
      mtq_del(q, NULL);
    }
  }
}

#define WORKERS 4
#define ITEMS_PER_WORKER 1000
#define TOTAL (WORKERS * ITEMS_PER_WORKER)
typedef struct {
  Mtq q;
  int capacity;
  atomic_uint next;
  int items[TOTAL];
  atomic_uint seen[TOTAL];
} Stress;

static void *produce_many(void *arg) {
  Stress *s = arg;
  for (int i = 0; i < ITEMS_PER_WORKER; i++) {
    unsigned id = atomic_fetch_add(&s->next, 1);
    assert(id < TOTAL);
    if (id % 2) mtq_head_put(s->q, &s->items[id]);
    else mtq_tail_put(s->q, &s->items[id]);
    if (s->capacity)
      assert(mtq_len(s->q) <= s->capacity);
  }
  return NULL;
}

static void *consume_many(void *arg) {
  Stress *s = arg;
  for (int i = 0; i < ITEMS_PER_WORKER; i++) {
    int *item = i % 2 ? mtq_head_get(s->q) : mtq_tail_get(s->q);
    assert(item && *item >= 0 && *item < TOTAL);
    assert(atomic_fetch_add(&s->seen[*item], 1) == 0);
  }
  return NULL;
}

static void stress(int capacity) {
  Stress *s = malloc(sizeof(*s));
  assert(s);
  s->q = mtq_new(capacity);
  s->capacity = capacity;
  atomic_init(&s->next, 0);
  for (int i = 0; i < TOTAL; i++) {
    s->items[i] = i;
    atomic_init(&s->seen[i], 0);
  }
  Threads consumers = threads_start(WORKERS, consume_many, s);
  Threads producers = threads_start(WORKERS, produce_many, s);
  threads_join(producers);
  threads_join(consumers);
  assert(mtq_len(s->q) == 0 && atomic_load(&s->next) == TOTAL);
  for (int i = 0; i < TOTAL; i++)
    assert(atomic_load(&s->seen[i]) == 1);
  mtq_del(s->q, NULL);
  free(s);
}

#define THREAD_COUNT 8
typedef struct {
  pthread_mutex_t mutex;
  pthread_barrier_t barrier;
  size_t count;
  pthread_t ids[THREAD_COUNT];
} GroupProbe;

static void *visit(void *arg) {
  GroupProbe *p = arg;
  PTHREAD_CHECK(pthread_mutex_lock(&p->mutex));
  assert(p->count < THREAD_COUNT);
  p->ids[p->count++] = pthread_self();
  PTHREAD_CHECK(pthread_mutex_unlock(&p->mutex));
  int result = pthread_barrier_wait(&p->barrier);
  assert(result == 0 || result == PTHREAD_BARRIER_SERIAL_THREAD);
  return NULL;
}

static void test_threads(void) {
  GroupProbe p = {.count = 0};
  PTHREAD_CHECK(pthread_mutex_init(&p.mutex, NULL));
  PTHREAD_CHECK(pthread_barrier_init(&p.barrier, NULL, THREAD_COUNT));
  threads_join(threads_start(0, visit, &p));
  assert(p.count == 0);
  threads_join(threads_start(THREAD_COUNT, visit, &p));
  assert(p.count == THREAD_COUNT);
  for (size_t i = 0; i < THREAD_COUNT; i++)
    for (size_t j = i + 1; j < THREAD_COUNT; j++)
      assert(!pthread_equal(p.ids[i], p.ids[j]));
  PTHREAD_CHECK(pthread_barrier_destroy(&p.barrier));
  PTHREAD_CHECK(pthread_mutex_destroy(&p.mutex));
}

int main(void) {
  alarm(60); // A deadlock must fail a test rather than leave it running forever.
  test_operations();
  test_threads();
  test_blocking();
  stress(1);
  stress(4);
  stress(0);
  puts("MTQ/thread tests passed: blocking, ownership, and 12,000 concurrent items.");
  return 0;
}
