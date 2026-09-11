#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deq.h"
#include "error.h"

// indices and size of array of node pointers
typedef enum {Head,Tail,Ends} End;

typedef struct Node { // one linked-list node
  // Two neighbor pointers, indexed by direction.
  struct Node *np[Ends];        // next/prev neighbors
  Data data;
} *Node;

typedef struct {                // this struct represents the double-ended queue
  Node ht[Ends];                // head/tail nodes (array of nodes)
  int len;
} *Rep;

static Rep rep(Deq q) {
  if (!q) ERROR("zero pointer");
  return (Rep)q;
}

// np[Head] points toward the head (the previous node); np[Tail]
// points toward the tail (the next node). The outward pointer at each
// endpoint is NULL. For either chosen end e, the opposite direction is
// Head+Tail-e: Head becomes Tail, and Tail becomes Head.
static End opposite(End e) {
  // Ends is the array size, not a valid index. Reject it as well.
  if (e!=Head && e!=Tail) ERROR("invalid end");
  return (End)(Head+Tail-e);
}

static void put(Rep r, End e, Data d) {
  End other=opposite(e);
  Node old=r->ht[e];
  Node n=(Node)malloc(sizeof(*n));
  if (!n) ERROR("malloc() failed");
  n->data=d;

  // The new node sits outside the old endpoint. Its outward link is
  // NULL, and its inward link points to the old endpoint, if one exists.
  n->np[e]=0;
  n->np[other]=old;
  if (old)
    old->np[e]=n;       // Link the old endpoint back to the new one.
  else
    r->ht[other]=n;     // First node: it is both the head and the tail.
  r->ht[e]=n;
  r->len++;
}

static Data ith(Rep r, End e, int i) {
  End inward=opposite(e);
  // Index 0 means the chosen endpoint. Invalid indices return NULL
  // without changing the queue (including negative indices).
  if (i<0 || i>=r->len) return 0;
  Node n=r->ht[e];
  for (int j=0; j<i; j++)
    n=n->np[inward];    // Head lookup walks tailward; tail lookup headward.
  return n->data;
}

// Detach one existing node. Both get and rem use this helper so the
// pointer updates and memory management are implemented in one place.
static Data detach(Rep r, Node n) {
  Node before=n->np[Head];
  Node after=n->np[Tail];
  Data d=n->data;       // Save the caller's data pointer before freeing n.

  // Bypass n in the forward direction. If n was the head, the node
  // after it becomes the head instead (possibly NULL for a singleton).
  if (before)
    before->np[Tail]=after;
  else
    r->ht[Head]=after;

  // Bypass n in the backward direction, or move the tail if necessary.
  // For the last node, these two branches clear both queue endpoints.
  if (after)
    after->np[Head]=before;
  else
    r->ht[Tail]=before;

  r->len--;
  free(n);             // Free only the wrapper node, not the stored data.
  return d;            // The caller still owns and can use the data.
}

static Data get(Rep r, End e) {
  opposite(e);         // Validate the end before using it as an index.
  Node n=r->ht[e];
  return n ? detach(r,n) : 0; // An empty queue stays empty.
}

static Data rem(Rep r, End e, Data d) {
  End inward=opposite(e);
  // Search from the requested end. With repeated data pointers, this
  // removes only the first match in that direction. Compare pointer
  // identity with ==, not the contents of the pointed-to objects.
  for (Node n=r->ht[e]; n; n=n->np[inward]) {
    if (n->data==d)
      return detach(r,n); // Return immediately: n has now been freed.
  }
  return 0;             // No match: links and length are unchanged.
}

extern Deq deq_new() {
  Rep r=(Rep)malloc(sizeof(*r));
  if (!r) ERROR("malloc() failed");
  r->ht[Head]=0;
  r->ht[Tail]=0;
  r->len=0;
  return r;
}

extern int deq_len(Deq q) { return rep(q)->len; }

extern void deq_head_put(Deq q, Data d) {        put(rep(q),Head,d); }
extern Data deq_head_get(Deq q)         { return get(rep(q),Head);   }
extern Data deq_head_ith(Deq q, int i)  { return ith(rep(q),Head,i); }
extern Data deq_head_rem(Deq q, Data d) { return rem(rep(q),Head,d); }

extern void deq_tail_put(Deq q, Data d) {        put(rep(q),Tail,d); }
extern Data deq_tail_get(Deq q)         { return get(rep(q),Tail);   }
extern Data deq_tail_ith(Deq q, int i)  { return ith(rep(q),Tail,i); }
extern Data deq_tail_rem(Deq q, Data d) { return rem(rep(q),Tail,d); }

extern void deq_map(Deq q, DeqMapF f) {
  for (Node n=rep(q)->ht[Head]; n; n=n->np[Tail]) // traverses list to run function f on each node
    f(n->data);
}

extern void deq_del(Deq q, DeqMapF f) {
  if (f) deq_map(q,f);
  Node curr=rep(q)->ht[Head];
  while (curr) {
    Node next=curr->np[Tail];
    free(curr);
    curr=next;
  }
  free(q);
}

extern Str deq_str(Deq q, DeqStrF f) {
  char *s=strdup("");
  for (Node n=rep(q)->ht[Head]; n; n=n->np[Tail]) {
    char *d=f ? f(n->data) : n->data;
    char *t; asprintf(&t,"%s%s%s",s,(*s ? " " : ""),d);
    free(s); s=t;
    if (f) free(d);
  }
  return s;
}
