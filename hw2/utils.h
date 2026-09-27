#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

static const int bitsperbyte=8;

// Direct mmap/munmap wrappers; mmalloc returns MAP_FAILED on failure.
extern void *mmalloc(size_t size);
extern void mmfree(void *p, size_t size);

// Ceiling division; divisor zero returns zero and sets errno to EINVAL.
extern size_t divup(size_t n, size_t d);
extern size_t bits2bytes(size_t bits);

// Invalid exponents return zero. size2e rounds upward; size2e(0) is zero.
extern size_t e2size(int e);
extern int size2e(size_t size);

// Bit operations on one byte; bit must be in [0, 7].
extern void bitset(void *p, int bit);
extern void bitclr(void *p, int bit);
extern void bitinv(void *p, int bit);
extern int  bittst(void *p, int bit);

#endif
