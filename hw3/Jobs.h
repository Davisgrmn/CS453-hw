#ifndef JOBS_H
#define JOBS_H

typedef void *Jobs;

#include "Pipeline.h"

extern Jobs newJobs(void);
extern void addJobs(Jobs jobs, Pipeline pipeline);
extern int sizeJobs(Jobs jobs);
extern void reapJobs(Jobs jobs);
/* Wait for all remaining jobs before releasing the queue. */
extern void freeJobs(Jobs jobs);

#endif
