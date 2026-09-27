#include <errno.h>
#include <limits.h>
#include <sys/mman.h>

#include "balloc.h"
#include "freelist.h"
#include "utils.h"

// The handle, free-list metadata, and pool are separate mmap regions.
// No header or other allocator data occupies an allocated block.
typedef struct {
  void *base;
  size_t size;
  int l, u;
  FreeList lists;
} *Rep;

extern Balloc bcreate(unsigned int size, int l, int u) {
  // Each free block must hold its next pointer. The largest block must
  // also be representable by bsize's unsigned-int return type.
  if (l < 0 || u < l || u >= (int)(sizeof(unsigned int) * CHAR_BIT) ||
      !e2size(u) || e2size(l) < sizeof(void *) || size < e2size(l)) {
    errno = EINVAL;
    return NULL;
  }

  Rep r = mmalloc(sizeof(*r));
  if (r == MAP_FAILED)
    return NULL;
  r->size = size;
  r->l = l;
  r->u = u;
  r->base = mmalloc(size);
  if (r->base == MAP_FAILED) {
    mmfree(r, sizeof(*r));
    return NULL;
  }
  r->lists = freelistcreate(size, l, u);
  if (!r->lists) {
    mmfree(r->base, r->size);
    mmfree(r, sizeof(*r));
    return NULL;
  }
  freelistinit(r->lists, r->base);
  return r;
}

extern void bdelete(Balloc pool) {
  Rep r = pool;
  if (!r)
    return;
  freelistdelete(r->lists, r->l, r->u);
  mmfree(r->base, r->size);
  mmfree(r, sizeof(*r));
}

extern void *balloc(Balloc pool, unsigned int size) {
  Rep r = pool;
  if (!r) {
    errno = EINVAL;
    return NULL;
  }
  int e = size2e(size);
  if (e < r->l)
    e = r->l;
  if (e > r->u) {
    errno = ENOMEM;
    return NULL;
  }
  return freelistalloc(r->lists, r->base, e, r->l);
}

extern void bfree(Balloc pool, void *mem) {
  Rep r = pool;
  if (!r || !mem)
    return;
  int e = freelistsize(r->lists, r->base, mem, r->l, r->u);
  if (e >= r->l)
    freelistfree(r->lists, r->base, mem, e, r->l);
}

extern unsigned int bsize(Balloc pool, void *mem) {
  Rep r = pool;
  if (!r || !mem)
    return 0;
  int e = freelistsize(r->lists, r->base, mem, r->l, r->u);
  return e < 0 ? 0 : (unsigned int)e2size(e);
}

extern size_t bstr(Balloc pool, char *buf, size_t capacity) {
  Rep r = pool;
  if (!r)
    return (size_t)snprintf(buf, capacity, "allocator: (null)\n");
  size_t used = (size_t)snprintf(buf, capacity,
      "allocator: base=%p size=%zu usable=%zu l=%d u=%d\n",
      r->base, r->size, r->size / e2size(r->l) * e2size(r->l), r->l, r->u);
  size_t room = used < capacity ? capacity - used : 0;
  return used + freeliststr(r->lists, room ? buf + used : NULL, room);
}

extern void bprint(Balloc pool) {
  Rep r = pool;
  if (!r) {
    puts("allocator: (null)");
    return;
  }
  printf("allocator: base=%p size=%zu l=%d u=%d\n",
         r->base, r->size, r->l, r->u);
  freelistprint(r->lists, r->l, r->u);
}
