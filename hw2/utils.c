#include <errno.h>
#include <limits.h>
#include <sys/mman.h>

#include "utils.h"

// Keep mmap's MAP_FAILED convention; only allocator construction calls this.
extern void *mmalloc(size_t size) {
  return mmap(NULL, size, PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

extern void mmfree(void *p, size_t size) {
  munmap(p, size);
}

// Unlike (n+d-1)/d, this does not overflow when n is near SIZE_MAX.
extern size_t divup(size_t n, size_t d) {
  if (!d) {
    errno = EINVAL;
    return 0;
  }
  return n / d + (n % d != 0);
}

extern size_t bits2bytes(size_t bits) {
  return divup(bits, bitsperbyte);
}

extern size_t e2size(int e) {
  if (e < 0 || e >= (int)(sizeof(size_t) * CHAR_BIT))
    return 0;
  return (size_t)1 << e;
}

// Ceiling(log2(size)); zero and one both have exponent zero.
// A result equal to the width of size_t cannot be passed to e2size.
extern int size2e(size_t size) {
  int e = 0;
  for (size_t n = size ? size - 1 : 0; n; n >>= 1)
    e++;
  return e;
}

// These helpers address one bit (0..7) in the byte pointed to by p.
extern void bitset(void *p, int bit) {
  *(unsigned char *)p |= (unsigned char)(1u << bit);
}

extern void bitclr(void *p, int bit) {
  *(unsigned char *)p &= (unsigned char)~(1u << bit);
}

extern void bitinv(void *p, int bit) {
  *(unsigned char *)p ^= (unsigned char)(1u << bit);
}

extern int bittst(void *p, int bit) {
  return (*(unsigned char *)p & (1u << bit)) != 0;
}
