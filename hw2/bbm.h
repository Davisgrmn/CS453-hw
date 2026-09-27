// One bitmap bit per buddy pair, indexed by offsets within an arena.
// Both buddies map to the same bit. The freelist module assigns its meaning.

#ifndef BBM_H
#define BBM_H

#include <stdio.h>

typedef void *BBM;

extern BBM  bbmcreate(size_t size, int e);
extern void bbmdelete(BBM b);

extern void bbmset(BBM b, void *base, void *mem, int e);
extern void bbmclr(BBM b, void *base, void *mem, int e);
extern  int bbmtst(BBM b, void *base, void *mem, int e);

extern void bbmprt(BBM b);
// snprintf-style rendering in caller-owned storage.
extern size_t bbmstr(BBM b, char *buf, size_t capacity);

// Address helpers require a valid exponent and pointers within one arena.
extern void *baddrset(void *base, void *mem, int e);
extern void *baddrclr(void *base, void *mem, int e);
extern void *baddrinv(void *base, void *mem, int e);
extern int   baddrtst(void *base, void *mem, int e);

#endif
