#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "balloc.h"
#include "bbm.h"
#include "bm.h"
#include "freelist.h"
#include "utils.h"

// Linker wrapping measures only this executable's mmap/munmap calls. It
// also lets every constructor failure path be exercised deterministically.
static size_t map_calls, live_maps;
static long fail_after = -1;
extern void *__real_mmap(void *, size_t, int, int, int, off_t);
extern int __real_munmap(void *, size_t);

void *__wrap_mmap(void *addr, size_t size, int prot, int flags, int fd, off_t off) {
  map_calls++;
  if (fail_after == 0) {
    errno = ENOMEM;
    return MAP_FAILED;
  }
  if (fail_after > 0)
    fail_after--;
  void *p = __real_mmap(addr, size, prot, flags, fd, off);
  if (p != MAP_FAILED)
    live_maps++;
  return p;
}

int __wrap_munmap(void *p, size_t size) {
  int result = __real_munmap(p, size);
  assert(result == 0);
  assert(live_maps > 0);
  live_maps--;
  return result;
}

static void test_utils(void) {
  assert(divup(0, 8) == 0);
  assert(divup(8, 8) == 1);
  assert(divup(9, 8) == 2);
  assert(divup(SIZE_MAX, 2) == SIZE_MAX / 2 + 1);
  assert(bits2bytes(17) == 3);
  assert(e2size(-1) == 0);
  assert(e2size((int)(sizeof(size_t) * CHAR_BIT)) == 0);
  assert(size2e(0) == 0 && size2e(1) == 0);
  assert(size2e(16) == 4 && size2e(17) == 5);
  assert(size2e(SIZE_MAX) == (int)(sizeof(size_t) * CHAR_BIT));
  for (int e = 0; e < (int)(sizeof(size_t) * CHAR_BIT); e++)
    assert(size2e(e2size(e)) == e);
  unsigned char byte = 0;
  for (int i = 0; i < 8; i++) {
    assert(!bittst(&byte, i));
    bitset(&byte, i);
    assert(bittst(&byte, i));
    bitinv(&byte, i);
    assert(!bittst(&byte, i));
    bitset(&byte, i);
  }
  assert(byte == 255);
  bitclr(&byte, 7);
  assert(byte == 127);
  void *mem = mmalloc(31);
  assert(mem != MAP_FAILED);
  memset(mem, 0xa5, 31);
  mmfree(mem, 31);
}

static void test_bitmaps(void) {
  BM b = bmcreate(19);
  assert(b);
  for (size_t i = 0; i < 19; i++)
    assert(!bmtst(b, i));
  bmset(b, 0);
  bmset(b, 7);
  bmset(b, 8);
  bmset(b, 18);
  for (size_t i = 0; i < 19; i++)
    assert(!!bmtst(b, i) == (i == 0 || i == 7 || i == 8 || i == 18));
  char text[32], tiny[3];
  assert(bmstr(b, text, sizeof(text)) == 8);
  assert(strcmp(text, "04 01 81") == 0);
  assert(bmstr(b, tiny, sizeof(tiny)) == 8);
  assert(strcmp(tiny, "04") == 0);
  assert(bmstr(b, NULL, 0) == 8);
  bmclr(b, 7);
  assert(!bmtst(b, 7) && bmtst(b, 0));
  bmdelete(b);
  b = bmcreate(0);
  assert(b && bmstr(b, text, sizeof(text)) == 0 && !text[0]);
  bmdelete(b);
  bmdelete(NULL);

  unsigned char arena[128];
  BBM pairs = bbmcreate(81, 4);
  assert(pairs);
  bbmset(pairs, arena, arena + 32, 4);
  assert(bbmtst(pairs, arena, arena + 48, 4));
  assert(!bbmtst(pairs, arena, arena, 4));
  bbmclr(pairs, arena, arena + 48, 4);
  assert(!bbmtst(pairs, arena, arena + 32, 4));
  bbmset(pairs, arena, arena + 80, 4);
  assert(bbmtst(pairs, arena, arena + 64, 4));
  assert(bbmstr(pairs, text, sizeof(text)) == 2);
  assert(strcmp(text, "04") == 0);
  assert(baddrset(arena, arena + 32, 4) == arena + 48);
  assert(baddrclr(arena, arena + 48, 4) == arena + 32);
  assert(baddrinv(arena, arena + 48, 4) == arena + 32);
  assert(baddrinv(arena, arena + 32, 4) == arena + 48);
  assert(baddrtst(arena, arena + 48, 4));
  assert(!baddrtst(arena, arena + 32, 4));
  bbmdelete(pairs);
}

static void test_freelist(void) {
  void *base = mmalloc(256);
  FreeList f = freelistcreate(256, 4, 8);
  assert(base != MAP_FAILED && f);
  size_t calls = map_calls;
  void *a = freelistalloc(f, base, 4, 4);
  void *b = freelistalloc(f, base, 4, 4);
  assert(a && b && a != b);
  assert(freelistsize(f, base, a, 4, 8) == 4);
  memset(a, 0xff, 16);
  memset(b, 0xff, 16);
  freelistfree(f, base, a, -1, -2); // Invalid exponents must be rejected.
  freelistfree(f, base, a, 5, 4); // Wrong size must not alter the allocation.
  assert(freelistsize(f, base, a, 4, 8) == 4);
  freelistfree(f, base, a, 4, 4);
  freelistfree(f, base, b, 4, 4);
  void *whole = freelistalloc(f, base, 8, 4);
  assert(whole == base);
  assert(!freelistalloc(f, base, 4, 4));
  char text[1024];
  size_t len = freeliststr(f, text, sizeof(text));
  assert(len == strlen(text) && len == freeliststr(f, NULL, 0));
  assert(strstr(text, "2^8 (256 bytes): free=0 allocated=1"));
  freelistfree(f, base, whole, 8, 4);
  assert(map_calls == calls);
  freelistdelete(f, 4, 8);
  mmfree(base, 256);
}

static void test_allocator(void) {
  assert(!bcreate(0, 4, 12));
  assert(!bcreate(16, -1, 4));
  assert(!bcreate(16, 0, 4)); // Too small for a free-list pointer.
  assert(!bcreate(16, 5, 4));
  assert(!bcreate(16, 4, (int)(sizeof(unsigned int) * CHAR_BIT)));
  assert(!bcreate(8, 4, 12));
  bdelete(NULL);
  bfree(NULL, NULL);
  assert(!balloc(NULL, 1) && !bsize(NULL, NULL));

  Balloc p = bcreate(256, 4, 8), other = bcreate(256, 4, 8);
  assert(p && other);
  size_t calls = map_calls;
  unsigned char *a = balloc(p, 17);
  void *b = balloc(p, 0);
  assert(a && b && bsize(p, a) == 32 && bsize(p, b) == 16);
  memset(a, 0xa5, 32); // Every byte, including the old list link, is usable.
  assert(!bsize(p, a + 1) && !bsize(p, a + 16));
  bfree(p, a + 16);
  bfree(other, a);
  bfree(p, NULL);
  assert(bsize(p, a) == 32 && !bsize(other, a));
  for (int i = 0; i < 32; i++)
    assert(a[i] == 0xa5);
  assert(!balloc(p, 257) && !balloc(p, UINT_MAX));
  bfree(p, a);
  assert(!bsize(p, a));
  bfree(p, a); // Repeated free must not add a second copy to a free list.
  bfree(p, b);
  void *whole = balloc(p, 256);
  assert(whole && bsize(p, whole) == 256 && !balloc(p, 1));
  char text[1024], tiny[2] = {'x', 'x'};
  size_t len = bstr(p, text, sizeof(text));
  assert(len == strlen(text) && len == bstr(p, NULL, 0));
  assert(strstr(text, "2^8 (256 bytes): free=0 allocated=1"));
  assert(bstr(p, tiny, sizeof(tiny)) == len && tiny[1] == '\0');
  assert(map_calls == calls);
  // Deletion also releases pools with outstanding allocations.
  bdelete(p);
  bdelete(other);
}

static uint32_t random_state = 0x4d595df4;
static uint32_t next_random(void) {
  random_state ^= random_state << 13;
  random_state ^= random_state >> 17;
  random_state ^= random_state << 5;
  return random_state;
}

typedef struct {
  unsigned char *mem;
  unsigned int size;
  unsigned char pattern;
} Allocation;

static void check_payload(Allocation *a) {
  for (unsigned int i = 0; i < a->size; i++)
    assert(a->mem[i] == a->pattern);
}

// An independent occupied-cell model checks whether each request SHOULD
// succeed, as well as detecting overlapping allocations and corrupted data.
static void stress(unsigned int capacity, int l, int u) {
  Balloc p = bcreate(capacity, l, u);
  assert(p);
  size_t calls = map_calls;
  unsigned int minimum = 1u << l;
  unsigned int cells = capacity / minimum;
  assert(cells <= 1024);
  void *small[1024];
  uintptr_t base = UINTPTR_MAX;
  for (unsigned int i = 0; i < cells; i++) {
    small[i] = balloc(p, minimum);
    assert(small[i] && bsize(p, small[i]) == minimum);
    if ((uintptr_t)small[i] < base)
      base = (uintptr_t)small[i];
  }
  assert(!balloc(p, minimum));
  for (unsigned int i = 0; i < cells; i++)
    bfree(p, small[i]);

  unsigned char occupied[1024] = {0};
  Allocation slots[64] = {{0}};
  for (int step = 0; step < 6000; step++) {
    Allocation *a = &slots[next_random() % 64];
    if (a->mem) {
      check_payload(a);
      assert(bsize(p, a->mem) == a->size);
      unsigned int index = ((uintptr_t)a->mem - base) / minimum;
      memset(occupied + index, 0, a->size / minimum);
      bfree(p, a->mem);
      *a = (Allocation){0};
    } else {
      unsigned int request = next_random() % ((1u << u) + 37);
      unsigned int rounded = minimum;
      while (rounded < request)
        rounded *= 2;
      unsigned int units = rounded / minimum;
      int possible = 0;
      if (rounded <= (1u << u)) {
        for (unsigned int start = 0; start + units <= cells; start += units) {
          unsigned int n = 0;
          while (n < units && !occupied[start + n])
            n++;
          if (n == units) {
            possible = 1;
            break;
          }
        }
      }
      a->mem = balloc(p, request);
      assert((a->mem != NULL) == possible);
      if (a->mem) {
        assert(bsize(p, a->mem) == rounded);
        uintptr_t offset = (uintptr_t)a->mem - base;
        assert(offset % rounded == 0 && offset + rounded <= capacity);
        unsigned int index = offset / minimum;
        for (unsigned int i = 0; i < units; i++) {
          assert(!occupied[index + i]);
          occupied[index + i] = 1;
        }
        a->size = rounded;
        a->pattern = (unsigned char)next_random();
        memset(a->mem, a->pattern, rounded);
      }
    }
    if (step % 113 == 0)
      for (unsigned int i = 0; i < 64; i++)
        check_payload(&slots[i]);
  }
  for (unsigned int i = 0; i < 64; i++) {
    check_payload(&slots[i]);
    bfree(p, slots[i].mem);
  }
  // All space must be reusable as the largest permitted blocks that fit.
  unsigned int remaining = cells * minimum;
  while (remaining) {
    unsigned int block = 1u << u;
    while (block > remaining)
      block /= 2;
    assert(balloc(p, block));
    remaining -= block;
  }
  assert(!balloc(p, 1));
  assert(map_calls == calls);
  bdelete(p);
}

static void test_creation_failures(void) {
  assert(live_maps == 0);
  size_t before = map_calls;
  Balloc p = bcreate(4096, 4, 12);
  assert(p);
  size_t constructor_calls = map_calls - before;
  bdelete(p);
  for (size_t failure = 0; failure < constructor_calls; failure++) {
    fail_after = (long)failure;
    assert(!bcreate(4096, 4, 12));
    assert(live_maps == 0);
  }
  fail_after = -1;
  p = bcreate(4096, 4, 12);
  assert(p);
  bdelete(p);
}

int main(void) {
  test_utils();
  test_bitmaps();
  test_freelist();
  test_allocator();
  stress(8192, 4, 10); // Several maximum-sized blocks.
  stress(4096, 4, 12); // Full-depth splitting and coalescing.
  stress(1021, 4, 10); // Missing buddies and unusable tail bytes.
  stress(4000, 4, 8);  // Irregular capacity exceeding the maximum block.
  stress(24, 3, 3);    // Multiple blocks, with no possible splitting.
  stress(16, 4, 12);   // Upper bound larger than the entire pool.
  assert(live_maps == 0);
  test_creation_failures();
  assert(live_maps == 0);
  puts("Allocator tests passed (36,000 modeled operations, mapping failures included).");
  return 0;
}
