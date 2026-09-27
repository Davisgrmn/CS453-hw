#include <errno.h>

#include "bbm.h"
#include "bm.h"
#include "utils.h"

// One bit represents a pair of 2^e-byte blocks, including a partial last pair.
static size_t mapsize(size_t size, int e) {
  return divup(divup(size, e2size(e)), 2);
}

static size_t bitaddr(void *base, void *mem, int e) {
  size_t offset = (unsigned char *)baddrclr(base, mem, e) - (unsigned char *)base;
  return offset / e2size(e) / 2;
}

extern BBM bbmcreate(size_t size, int e) {
  if (!e2size(e)) {
    errno = EINVAL;
    return NULL;
  }
  return bmcreate(mapsize(size, e));
}

extern void bbmdelete(BBM b) { bmdelete(b); }

extern void bbmset(BBM b, void *base, void *mem, int e) {
  bmset(b, bitaddr(base, mem, e));
}

extern void bbmclr(BBM b, void *base, void *mem, int e) {
  bmclr(b, bitaddr(base, mem, e));
}

extern int bbmtst(BBM b, void *base, void *mem, int e) {
  return bmtst(b, bitaddr(base, mem, e));
}

extern size_t bbmstr(BBM b, char *buf, size_t capacity) {
  return bmstr(b, buf, capacity);
}

extern void bbmprt(BBM b) { bmprt(b); }

// Buddy addresses depend on offsets within the pool, not absolute addresses.
// Setting/clearing bit e selects the upper/lower buddy; flipping it finds
// the other buddy. size_t masks avoid signed shifts and 32-bit truncation.
extern void *baddrset(void *base, void *mem, int e) {
  size_t offset = (unsigned char *)mem - (unsigned char *)base;
  return (unsigned char *)base + (offset | e2size(e));
}

extern void *baddrclr(void *base, void *mem, int e) {
  size_t offset = (unsigned char *)mem - (unsigned char *)base;
  return (unsigned char *)base + (offset & ~e2size(e));
}

extern void *baddrinv(void *base, void *mem, int e) {
  size_t offset = (unsigned char *)mem - (unsigned char *)base;
  return (unsigned char *)base + (offset ^ e2size(e));
}

extern int baddrtst(void *base, void *mem, int e) {
  size_t offset = (unsigned char *)mem - (unsigned char *)base;
  return (offset & e2size(e)) != 0;
}
