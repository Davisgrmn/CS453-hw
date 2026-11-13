#include <assert.h>
#include <stdio.h>

#include "deq.h"

int main(void) {
  Deq q = deq_new();
  int a = 1, b = 2, c = 3, missing = 4;

  // Empty queue: nothing can be retrieved or removed.
  assert(deq_head_get(q) == NULL);
  assert(deq_tail_get(q) == NULL);
  assert(deq_head_ith(q, 0) == NULL);
  assert(deq_tail_ith(q, 0) == NULL);
  assert(deq_head_rem(q, &a) == NULL);
  assert(deq_tail_rem(q, &a) == NULL);
  assert(deq_len(q) == 0);

  // put: insert into an empty queue, then add at both ends.
  deq_head_put(q, &b);
  assert(deq_head_ith(q, 0) == &b);
  assert(deq_tail_ith(q, 0) == &b);
  assert(deq_len(q) == 1);

  deq_head_put(q, &a);
  deq_tail_put(q, &c);           // Queue: a, b, c
  assert(deq_len(q) == 3);

  // ith: valid positions counted from either end.
  assert(deq_head_ith(q, 0) == &a);
  assert(deq_head_ith(q, 1) == &b);
  assert(deq_head_ith(q, 2) == &c);
  assert(deq_tail_ith(q, 0) == &c);
  assert(deq_tail_ith(q, 1) == &b);
  assert(deq_tail_ith(q, 2) == &a);

  // ith: negative and out-of-bounds indices.
  assert(deq_head_ith(q, -1) == NULL);
  assert(deq_tail_ith(q, -1) == NULL);
  assert(deq_head_ith(q, 3) == NULL);
  assert(deq_tail_ith(q, 3) == NULL);
  assert(deq_len(q) == 3);

  // get: remove from both ends, including the last node.
  assert(deq_head_get(q) == &a);
  assert(deq_len(q) == 2);
  assert(deq_tail_get(q) == &c);
  assert(deq_len(q) == 1);
  assert(deq_head_get(q) == &b);
  assert(deq_len(q) == 0);

  // put/get: reuse an empty queue and remove a singleton from the tail.
  deq_tail_put(q, &a);
  assert(deq_head_ith(q, 0) == &a);
  assert(deq_tail_get(q) == &a);
  assert(deq_len(q) == 0);

  // rem: a missing pointer must leave the queue unchanged.
  deq_tail_put(q, &a);
  deq_tail_put(q, &b);
  deq_tail_put(q, &c);           // Queue: a, b, c
  assert(deq_head_rem(q, &missing) == NULL);
  assert(deq_tail_rem(q, &missing) == NULL);
  assert(deq_len(q) == 3);

  // rem: remove the middle node and check both remaining links.
  assert(deq_head_rem(q, &b) == &b);
  assert(deq_len(q) == 2);
  assert(deq_head_ith(q, 1) == &c);
  assert(deq_tail_ith(q, 1) == &a);

  // rem: remove the tail, then the last remaining node.
  assert(deq_tail_rem(q, &c) == &c);
  assert(deq_len(q) == 1);
  assert(deq_head_rem(q, &a) == &a);
  assert(deq_len(q) == 0);
  assert(deq_head_ith(q, 0) == NULL);
  assert(deq_tail_ith(q, 0) == NULL);

  deq_del(q, NULL);
  puts("All tests passed.");
  return 0;
}
