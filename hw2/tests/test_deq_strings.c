#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deq.h"

// Exercise the unchanged queue's libc string helpers as well as its nodes.
int main(void) {
  for (int iteration = 0; iteration < 100; iteration++) {
    Deq q = deq_new();
    deq_head_put(q, "beta");
    deq_head_put(q, "alpha");
    deq_tail_put(q, "gamma");
    char *s = deq_str(q, NULL);
    assert(s && strcmp(s, "alpha beta gamma") == 0);
    free(s);
    deq_del(q, NULL);
  }
  puts("Queue string/reuse tests passed.");
  return 0;
}
