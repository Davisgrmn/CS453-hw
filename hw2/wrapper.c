#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "balloc.h"

// Link this file only into programs that should replace libc allocation.
// The assignment wrapper owns one fixed-size pool for the process lifetime.
static Balloc bp = NULL;

extern void *malloc(size_t size) {
  // Do not truncate size_t to the allocator interface's unsigned int.
  if (size > UINT_MAX || size > 4096) {
    errno = ENOMEM;
    return NULL;
  }
  if (!bp)
    bp = bcreate(4096, 4, 12);
  return bp ? balloc(bp, (unsigned int)size) : NULL;
}

extern void free(void *ptr) {
  bfree(bp, ptr);
}

extern void *realloc(void *ptr, size_t size) {
  if (!ptr)
    return malloc(size);
  if (!size) {
    free(ptr);
    return NULL;
  }
  size_t oldsize = bsize(bp, ptr);
  if (size <= oldsize)
    return ptr;
  void *new = malloc(size);
  // On failure the old block, including all its contents, remains owned
  // by the caller. Copy only after allocation succeeds.
  if (!new)
    return NULL;
  memcpy(new, ptr, oldsize);
  free(ptr);
  return new;
}
