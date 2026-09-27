// A general-purpose bitmap.

#ifndef BM_H
#define BM_H

#include <stdio.h>

typedef void *BM;

// Create an initially clear bitmap; NULL indicates mapping failure.
extern BM   bmcreate(size_t bits);
extern void bmdelete(BM b);

extern void bmset(BM b, size_t i);
extern void bmclr(BM b, size_t i);
extern int  bmtst(BM b, size_t i);

// Diagnostic output; bmstr uses snprintf-style length/truncation rules.
extern void bmprt(BM b);
extern size_t bmstr(BM b, char *buf, size_t capacity);

#endif
