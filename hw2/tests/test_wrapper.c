#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Test the real wrapper implementation under renamed symbols so test
// runtime allocations do not consume its deliberately small 4096-byte pool.
extern void *wrapped_malloc(size_t);
extern void wrapped_free(void *);
extern void *wrapped_realloc(void *, size_t);

int main(void) {
  wrapped_free(NULL);
  assert(!wrapped_malloc(SIZE_MAX));
  unsigned char *a = wrapped_realloc(NULL, 17);
  assert(a);
  memset(a, 0x6a, 17);
  unsigned char *b = wrapped_realloc(a, 65);
  assert(b);
  for (int i = 0; i < 17; i++)
    assert(b[i] == 0x6a);
  assert(wrapped_realloc(b, 8) == b);
  assert(!wrapped_realloc(b, SIZE_MAX));
  for (int i = 0; i < 17; i++)
    assert(b[i] == 0x6a);
  assert(!wrapped_realloc(b, 0));

  a = wrapped_malloc(2048);
  void *other = wrapped_malloc(2048);
  assert(a && other);
  memset(a, 0x5c, 2048);
  assert(!wrapped_malloc(1));
  assert(!wrapped_realloc(a, 4096));
  for (int i = 0; i < 2048; i++)
    assert(a[i] == 0x5c);
  wrapped_free(other);
  wrapped_free(a);
  a = wrapped_malloc(4096);
  assert(a);
  wrapped_free(a);
  a = wrapped_malloc(0);
  assert(a);
  wrapped_free(a);
  puts("Wrapper tests passed.");
  return 0;
}
