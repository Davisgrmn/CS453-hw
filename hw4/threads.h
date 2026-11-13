#ifndef THREADS_H
#define THREADS_H

#include <stddef.h>

typedef void *Threads;
typedef void *(*ThreadFunction)(void *);

/* Start n new, joinable threads, each calling function(arg) exactly once.
 * The function and argument must remain valid until threads_join returns.
 * n may be zero. Allocation and pthread failures are fatal with diagnostics.
 */
extern Threads threads_start(size_t n, ThreadFunction function, void *arg);

/* Join every thread, discard callback results, and release the group handle.
 * Call once per group, from a thread outside the group.
 */
extern void threads_join(Threads threads);

#endif
