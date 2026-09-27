#ifndef BALLOC_H
#define BALLOC_H

#include <stddef.h>

typedef void *Balloc;

// A fixed-size mmap-backed arena. Require 2^l >= sizeof(void *), l <= u,
// size >= 2^l, and 2^u representable by unsigned int. Invalid parameters
// return NULL/EINVAL. A remainder smaller than 2^l is not allocatable.
extern Balloc bcreate(unsigned int size, int l, int u);
extern void   bdelete(Balloc pool);

// Round requests (including zero) up to at least 2^l and a power of two.
// Requests above 2^u and exhausted pools return NULL/ENOMEM.
extern void *balloc(Balloc pool, unsigned int size);
// NULL, unallocated, interior, and foreign pointers are ignored.
extern void  bfree(Balloc pool, void *mem);

// Actual block size, or zero if mem is not a live allocation's start.
extern unsigned int bsize(Balloc pool, void *mem);
extern void bprint(Balloc pool);
// Caller-owned output buffer; return required length excluding NUL, like
// snprintf. A NULL buffer with capacity zero queries the required length.
extern size_t bstr(Balloc pool, char *buf, size_t capacity);

#endif
