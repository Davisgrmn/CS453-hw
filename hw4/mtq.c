#include <pthread.h>
#include <stdlib.h>

#include "mtq.h"
#include "pthread_check.h"

typedef struct {
  Deq deque;
  int capacity;
  pthread_mutex_t mutex;
  pthread_cond_t not_empty;
  pthread_cond_t not_full;
} *Rep;

static Rep rep(Mtq q) {
  if (!q)
    ERROR("mtq: null queue");
  return q;
}

extern Mtq mtq_new(int capacity) {
  if (capacity < 0)
    ERROR("mtq_new: capacity must be nonnegative");
  Rep r = malloc(sizeof(*r));
  if (!r)
    ERROR("mtq_new: malloc() failed");
  r->deque = deq_new();
  r->capacity = capacity;
  PTHREAD_CHECK(pthread_mutex_init(&r->mutex, NULL));
  PTHREAD_CHECK(pthread_cond_init(&r->not_empty, NULL));
  PTHREAD_CHECK(pthread_cond_init(&r->not_full, NULL));
  return r;
}

extern int mtq_len(Mtq q) {
  Rep r = rep(q);
  PTHREAD_CHECK(pthread_mutex_lock(&r->mutex));
  int n = deq_len(r->deque);
  PTHREAD_CHECK(pthread_mutex_unlock(&r->mutex));
  return n;
}

typedef void (*Put)(Deq, Data);
typedef Data (*Get)(Deq);
typedef Data (*Ith)(Deq, int);
typedef Data (*Rem)(Deq, Data);

static void put(Rep r, Data d, Put function) {
  PTHREAD_CHECK(pthread_mutex_lock(&r->mutex));
  /* Wait atomically releases the mutex; always recheck after waking. */
  while (r->capacity && deq_len(r->deque) >= r->capacity)
    PTHREAD_CHECK(pthread_cond_wait(&r->not_full, &r->mutex));
  function(r->deque, d);
  PTHREAD_CHECK(pthread_cond_signal(&r->not_empty));
  PTHREAD_CHECK(pthread_mutex_unlock(&r->mutex));
}

static Data get(Rep r, Get function) {
  PTHREAD_CHECK(pthread_mutex_lock(&r->mutex));
  while (!deq_len(r->deque))
    PTHREAD_CHECK(pthread_cond_wait(&r->not_empty, &r->mutex));
  Data d = function(r->deque);
  PTHREAD_CHECK(pthread_cond_signal(&r->not_full));
  PTHREAD_CHECK(pthread_mutex_unlock(&r->mutex));
  return d;
}

static Data ith(Rep r, int i, Ith function) {
  PTHREAD_CHECK(pthread_mutex_lock(&r->mutex));
  Data d = function(r->deque, i);
  PTHREAD_CHECK(pthread_mutex_unlock(&r->mutex));
  return d;
}

static Data rem(Rep r, Data d, Rem function) {
  PTHREAD_CHECK(pthread_mutex_lock(&r->mutex));
  int before = deq_len(r->deque);
  Data result = function(r->deque, d);
  /* Checking length also handles successful removal of a stored NULL. */
  if (deq_len(r->deque) != before)
    PTHREAD_CHECK(pthread_cond_signal(&r->not_full));
  PTHREAD_CHECK(pthread_mutex_unlock(&r->mutex));
  return result;
}

extern void mtq_head_put(Mtq q, Data d) { put(rep(q), d, deq_head_put); }
extern Data mtq_head_get(Mtq q) { return get(rep(q), deq_head_get); }
extern Data mtq_head_ith(Mtq q, int i) { return ith(rep(q), i, deq_head_ith); }
extern Data mtq_head_rem(Mtq q, Data d) { return rem(rep(q), d, deq_head_rem); }

extern void mtq_tail_put(Mtq q, Data d) { put(rep(q), d, deq_tail_put); }
extern Data mtq_tail_get(Mtq q) { return get(rep(q), deq_tail_get); }
extern Data mtq_tail_ith(Mtq q, int i) { return ith(rep(q), i, deq_tail_ith); }
extern Data mtq_tail_rem(Mtq q, Data d) { return rem(rep(q), d, deq_tail_rem); }

extern void mtq_map(Mtq q, DeqMapF f) {
  Rep r = rep(q);
  if (!f)
    ERROR("mtq_map: null callback");
  PTHREAD_CHECK(pthread_mutex_lock(&r->mutex));
  deq_map(r->deque, f);
  PTHREAD_CHECK(pthread_mutex_unlock(&r->mutex));
}

extern Str mtq_str(Mtq q, DeqStrF f) {
  Rep r = rep(q);
  PTHREAD_CHECK(pthread_mutex_lock(&r->mutex));
  Str s = deq_str(r->deque, f);
  PTHREAD_CHECK(pthread_mutex_unlock(&r->mutex));
  return s;
}

extern void mtq_del(Mtq q, DeqMapF f) {
  Rep r = rep(q);
  /* Destruction requires exclusive ownership, after all workers are joined. */
  deq_del(r->deque, f);
  PTHREAD_CHECK(pthread_cond_destroy(&r->not_empty));
  PTHREAD_CHECK(pthread_cond_destroy(&r->not_full));
  PTHREAD_CHECK(pthread_mutex_destroy(&r->mutex));
  free(r);
}
