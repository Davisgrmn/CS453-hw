#ifndef FREELIST_H
#define FREELIST_H

#include <stdio.h>

typedef void *FreeList;

// Create all metadata up front. The caller owns the separate memory arena.
extern FreeList freelistcreate(size_t size, int l, int u);
extern void     freelistdelete(FreeList f, int l, int u);
// Partition the arena once. freelistalloc also binds on first use, for
// callers using the original interface. base must be pointer-aligned.
extern void freelistinit(FreeList f, void *base);

// e is the requested block exponent. These functions never map memory.
extern void *freelistalloc(FreeList f, void *base, int e, int l);
extern void  freelistfree(FreeList f, void *base, void *mem, int e, int l);

// Return the allocation's exponent, or -1 for an invalid/unallocated address.
extern int freelistsize(FreeList f, void *base, void *mem, int l, int u);
extern void freelistprint(FreeList f, int l, int u);
// snprintf-style summary in caller-owned storage; does not allocate memory.
extern size_t freeliststr(FreeList f, char *buf, size_t capacity);

#endif
