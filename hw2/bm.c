#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "bm.h"
#include "utils.h"

// The handle addresses the bytes; the preceding size_t stores the bit count.
// This header belongs to bitmap metadata, never to a user's allocated block.
static size_t bmbits(BM b) { return ((size_t *)b)[-1]; }
static size_t bmbytes(BM b) { return bits2bytes(bmbits(b)); }

static void ok(BM b, size_t i) {
  if (b && i < bmbits(b))
    return;
  fprintf(stderr, "bitmap index out of range\n");
  exit(1);
}

extern BM bmcreate(size_t bits) {
  size_t bytes = bits2bytes(bits);
  size_t *p = mmalloc(sizeof(size_t) + bytes);
  if (p == MAP_FAILED)
    return NULL;
  *p = bits;
  BM b = p + 1;
  memset(b, 0, bytes);
  return b;
}

extern void bmdelete(BM b) {
  if (!b)
    return;
  size_t *p = (size_t *)b - 1;
  mmfree(p, sizeof(size_t) + bits2bytes(*p));
}

// Divide the bit index into a byte index and a bit within that byte.
extern void bmset(BM b, size_t i) {
  ok(b, i);
  bitset((unsigned char *)b + i / bitsperbyte, i % bitsperbyte);
}

extern void bmclr(BM b, size_t i) {
  ok(b, i);
  bitclr((unsigned char *)b + i / bitsperbyte, i % bitsperbyte);
}

extern int bmtst(BM b, size_t i) {
  ok(b, i);
  return bittst((unsigned char *)b + i / bitsperbyte, i % bitsperbyte);
}

// Render bytes from high to low, with snprintf-style truncation/length rules.
extern size_t bmstr(BM b, char *buf, size_t capacity) {
  size_t used = 0;
  if (capacity)
    buf[0] = '\0';
  if (!b)
    return (size_t)snprintf(buf, capacity, "(null)");
  for (size_t i = bmbytes(b); i; i--) {
    size_t room = used < capacity ? capacity - used : 0;
    used += (size_t)snprintf(room ? buf + used : NULL, room,
        "%02x%s", (unsigned int)((unsigned char *)b)[i - 1], i > 1 ? " " : "");
  }
  return used;
}

extern void bmprt(BM b) {
  if (!b) {
    puts("(null)");
    return;
  }
  for (size_t i = bmbytes(b); i; i--)
    printf("%02x%s", (unsigned int)((unsigned char *)b)[i - 1], i > 1 ? " " : "");
  putchar('\n');
}
