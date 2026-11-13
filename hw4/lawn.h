#ifndef LAWN_H
#define LAWN_H

typedef void *Lawn;

extern Lawn lawn_new(int lawnsize, int molesize);
// Create, run, and free on the main thread. A worker calls stop after all
// mole operations finish; run returns once the window is hidden.
extern void lawn_run(Lawn l);
extern void lawn_stop(Lawn l);
extern void lawn_free(Lawn l);

#endif
