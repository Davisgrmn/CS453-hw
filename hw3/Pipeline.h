#ifndef PIPELINE_H
#define PIPELINE_H

typedef void *Pipeline;

#include "Command.h"
#include "Jobs.h"

extern Pipeline newPipeline(int fg);
extern void addPipeline(Pipeline pipeline, Command command);
extern int sizePipeline(Pipeline pipeline);
extern void execPipeline(Pipeline pipeline, Jobs jobs, int *eof);
/* Return true once all children have been reaped; block selects wait/WNOHANG. */
extern int waitPipeline(Pipeline pipeline, int block);
extern void freePipeline(Pipeline pipeline);

#endif
