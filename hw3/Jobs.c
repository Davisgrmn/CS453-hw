#include "Jobs.h"
#include "deq.h"

extern Jobs newJobs(void) {
  return deq_new();
}

extern void addJobs(Jobs jobs, Pipeline pipeline) {
  deq_tail_put(jobs, pipeline);
}

extern int sizeJobs(Jobs jobs) {
  return deq_len(jobs);
}

extern void reapJobs(Jobs jobs) {
  for (int i = 0; i < deq_len(jobs); ) {
    Pipeline pipeline = deq_head_ith(jobs, i);
    if (waitPipeline(pipeline, 0)) {
      deq_head_rem(jobs, pipeline);
      freePipeline(pipeline);
    } else {
      i++;
    }
  }
}

extern void freeJobs(Jobs jobs) {
  while (deq_len(jobs)) {
    Pipeline pipeline = deq_head_get(jobs);
    waitPipeline(pipeline, 1);
    freePipeline(pipeline);
  }
  deq_del(jobs, 0);
}
