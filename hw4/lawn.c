#include <errno.h>
#include <stdlib.h>

#include "lawn.h"
#define LAWNIMP
#include "lawnimp.h"
#undef LAWNIMP
#include "error.h"

extern Lawn lawn_new(int lawnsize, int molesize) {
  if (!lawnsize) lawnsize = 40;
  if (!molesize) molesize = 15;
  if (lawnsize < 0 || molesize < 0)
    ERROR("lawn_new: dimensions must be positive");
  LawnRep lawn = malloc(sizeof(*lawn));
  if (!lawn)
    ERROR("lawn_new: malloc() failed");
  lawn->lawnsize = lawnsize;
  lawn->molesize = molesize;
  lawn->tick_ms = 1000;
  const char *tick = getenv("WAM_TICK_MS");
  if (tick) {
    char *end;
    errno = 0;
    long n = strtol(tick, &end, 10);
    if (errno || !*tick || *end || n < 0 || n > 60000)
      ERROR("WAM_TICK_MS must be an integer from 0 to 60000");
    lawn->tick_ms = (int)n;
  }
  lawn->window = lawnimp_new(lawnsize, molesize);
  return lawn;
}

extern void lawn_run(Lawn lawn) {
  lawnimp_run(lawn);
}

extern void lawn_stop(Lawn lawn) {
  LawnRep r = lawn;
  lawnimp_stop(r->window);
}

extern void lawn_free(Lawn lawn) {
  LawnRep r = lawn;
  lawnimp_free(r->window);
  free(r);
}
