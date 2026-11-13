#ifndef MTQ_H
#define MTQ_H

#include "deq.h"

typedef void *Mtq;

/* One mutex protects each wrapped deque. Capacity 0 means unbounded;
 * otherwise put waits for room. get waits for an item. Negative capacity
 * is an error. NULL data pointers are allowed, just as in the original deque.
 */
extern Mtq mtq_new(int capacity);
extern int mtq_len(Mtq q);

extern void mtq_head_put(Mtq q, Data d);
extern Data mtq_head_get(Mtq q);
extern Data mtq_head_ith(Mtq q, int i);
extern Data mtq_head_rem(Mtq q, Data d);

extern void mtq_tail_put(Mtq q, Data d);
extern Data mtq_tail_get(Mtq q);
extern Data mtq_tail_ith(Mtq q, int i);
extern Data mtq_tail_rem(Mtq q, Data d);

/* ith and rem do not wait: a missing item/index returns NULL. Observations
 * such as len are snapshots, not reservations for a later operation.
 * map/str callbacks run under the queue lock and must not re-enter this queue.
 * Returned Data pointers still belong to the caller; str returns owned text.
 */
extern void mtq_map(Mtq q, DeqMapF f);
extern Str mtq_str(Mtq q, DeqStrF f);

/* All users must have finished (including blocked puts/gets) before deletion.
 * A non-NULL callback releases data still stored in the queue.
 */
extern void mtq_del(Mtq q, DeqMapF f);

#endif
