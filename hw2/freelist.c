#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <sys/mman.h>

#include "freelist.h"
#include "bbm.h"
#include "bm.h"
#include "utils.h"

// A free block stores exactly one link, at its beginning. Once allocated,
// the caller may overwrite the entire block, including this link.
typedef struct Node {
  struct Node *next;
} Node;

typedef struct {
  Node *head;
  BBM pairs;                 // XOR of the two buddies' free-list memberships.
  BM allocated;              // One bit per block allocated at this exact order.
  size_t nfree, nallocated;  // Counts used by the debugging functions.
} Order;

typedef struct {
  size_t size, bytes;
  int l, u;
  void *base;
  Order orders[];            // Index zero corresponds to exponent l.
} *Rep;

static void toggle(Order *o, void *base, void *mem, int e) {
  if (bbmtst(o->pairs, base, mem, e))
    bbmclr(o->pairs, base, mem, e);
  else
    bbmset(o->pairs, base, mem, e);
}

static void push(Rep r, void *mem, int e) {
  Order *o = &r->orders[e - r->l];
  Node *n = mem;
  n->next = o->head;
  o->head = n;
  o->nfree++;
  toggle(o, r->base, mem, e);
}

static void *pop(Rep r, int e) {
  Order *o = &r->orders[e - r->l];
  Node *n = o->head;
  o->head = n->next;
  o->nfree--;
  toggle(o, r->base, n, e);
  return n;
}

// The bitmap answers whether a buddy is free without a search. Unlinking
// that buddy still walks the singly linked list, as required by the layout.
static void unlinkbuddy(Rep r, void *buddy, int e) {
  Order *o = &r->orders[e - r->l];
  Node **link = &o->head;
  while (*link != buddy)
    link = &(*link)->next;
  *link = (*link)->next;
  o->nfree--;
  toggle(o, r->base, buddy, e);
}

extern FreeList freelistcreate(size_t size, int l, int u) {
  if (l < 0 || u < l || u >= (int)(sizeof(size_t) * CHAR_BIT) ||
      e2size(l) < sizeof(Node) || size < e2size(l)) {
    errno = EINVAL;
    return NULL;
  }
  Rep r;
  size_t bytes = sizeof(*r) + (size_t)(u - l + 1) * sizeof(Order);
  r = mmalloc(bytes);
  if (r == MAP_FAILED)
    return NULL;
  // Anonymous mappings start zeroed, including uncreated bitmap handles.
  r->bytes = bytes;
  r->size = size;
  r->l = l;
  r->u = u;
  for (int e = l; e <= u; e++) {
    Order *o = &r->orders[e - l];
    o->pairs = bbmcreate(size, e);
    o->allocated = bmcreate(size / e2size(e));
    if (!o->pairs || !o->allocated) {
      freelistdelete(r, l, u);
      return NULL;
    }
  }
  return r;
}

extern void freelistinit(FreeList f, void *base) {
  Rep r = f;
  if (!r || !base || r->base)
    return;
  r->base = base;
  // Greedily partition the arena without rounding its size upward. This
  // handles several maximum-size blocks and a non-power-of-two remainder.
  size_t offset = 0;
  while (r->size - offset >= e2size(r->l)) {
    int e = r->u;
    while (e2size(e) > r->size - offset)
      e--;
    push(r, (unsigned char *)base + offset, e);
    offset += e2size(e);
  }
}

extern void freelistdelete(FreeList f, int l, int u) {
  Rep r = f;
  (void)l;
  (void)u;
  if (!r)
    return;
  for (int e = r->l; e <= r->u; e++) {
    Order *o = &r->orders[e - r->l];
    bbmdelete(o->pairs);
    bmdelete(o->allocated);
  }
  mmfree(r, r->bytes);
}

extern void *freelistalloc(FreeList f, void *base, int e, int l) {
  Rep r = f;
  if (!r || !base || l != r->l || e < l || e > r->u) {
    errno = EINVAL;
    return NULL;
  }
  // Retain the supplied interface: standalone users can bind on first use.
  // bcreate binds explicitly, so ordinary allocation never initializes a pool.
  freelistinit(r, base);
  if (base != r->base) {
    errno = EINVAL;
    return NULL;
  }
  int found = e;
  while (found <= r->u && !r->orders[found - l].head)
    found++;
  if (found > r->u) {
    errno = ENOMEM;
    return NULL;
  }
  void *mem = pop(r, found);
  while (found > e) {
    found--;
    // Keep the lower half and put its upper buddy on the smaller list.
    push(r, (unsigned char *)mem + e2size(found), found);
  }
  Order *o = &r->orders[e - l];
  size_t offset = (unsigned char *)mem - (unsigned char *)base;
  bmset(o->allocated, offset / e2size(e));
  o->nallocated++;
  return mem;
}

extern int freelistsize(FreeList f, void *base, void *mem, int l, int u) {
  Rep r = f;
  if (!r || !mem || base != r->base || l != r->l || u != r->u)
    return -1;
  // Integer comparisons also let us reject pointers from a different pool.
  uintptr_t address = (uintptr_t)mem, start = (uintptr_t)base;
  if (address < start || address - start >= r->size)
    return -1;
  size_t offset = address - start;
  for (int e = l; e <= u; e++) {
    size_t block = e2size(e);
    if (offset % block == 0 && block <= r->size - offset &&
        bmtst(r->orders[e - l].allocated, offset / block))
      return e;
  }
  return -1;
}

extern void freelistfree(FreeList f, void *base, void *mem, int e, int l) {
  Rep r = f;
  if (!r || l != r->l || e < l || e > r->u ||
      freelistsize(r, base, mem, l, r->u) != e)
    return;
  size_t offset = (unsigned char *)mem - (unsigned char *)base;
  Order *o = &r->orders[e - l];
  bmclr(o->allocated, offset / e2size(e));
  o->nallocated--;

  while (e < r->u) {
    size_t block = e2size(e);
    size_t buddyoffset = offset ^ block;
    // The last block of an irregular pool may have no in-pool buddy.
    if (buddyoffset >= r->size || block > r->size - buddyoffset)
      break;
    o = &r->orders[e - l];
    // mem is not on this list yet. A set parity bit therefore means its
    // buddy IS on the list. Below u, eager merging prevents two free buddies.
    if (!bbmtst(o->pairs, base, mem, e))
      break;
    unlinkbuddy(r, (unsigned char *)base + buddyoffset, e);
    offset &= ~block;
    mem = (unsigned char *)base + offset;
    e++;
  }
  push(r, mem, e);
}

extern size_t freeliststr(FreeList f, char *buf, size_t capacity) {
  Rep r = f;
  if (!r)
    return (size_t)snprintf(buf, capacity, "free lists: (null)\n");
  size_t used = 0;
  for (int e = r->l; e <= r->u; e++) {
    Order *o = &r->orders[e - r->l];
    size_t room = used < capacity ? capacity - used : 0;
    used += (size_t)snprintf(room ? buf + used : NULL, room,
        "2^%d (%zu bytes): free=%zu allocated=%zu head=%p\n",
        e, e2size(e), o->nfree, o->nallocated, (void *)o->head);
  }
  return used;
}

extern void freelistprint(FreeList f, int l, int u) {
  Rep r = f;
  if (!r || l != r->l || u != r->u)
    return;
  for (int e = l; e <= u; e++) {
    Order *o = &r->orders[e - l];
    printf("2^%d (%zu bytes): free=%zu allocated=%zu\n  blocks:",
           e, e2size(e), o->nfree, o->nallocated);
    for (Node *n = o->head; n; n = n->next)
      printf(" %p", (void *)n);
    printf("\n  buddy parity: ");
    bbmprt(o->pairs);
    printf("  allocations: ");
    bmprt(o->allocated);
  }
}
