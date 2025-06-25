//// Boundary-tag malloc

//// Modifiable parameters
#define HEAPBLOCK      131072
#define GRANULARITY    8
#define HEURISTIC      2
#define SIZEDIST       2
#define LIFETIMEDIST   0
#define INFOVERBOSE    0
#define BENCHVERBOSE   0
#define ITERS          10000

//// From here on out is the actual code
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include "common.h"

#define ALIGNOFFL(p)   ( (uintptr_t)(p))%sizeof(btheader)
#define ALIGNOFFR(p)   (-(uintptr_t)(p))%sizeof(btheader)
#define RFIT(x)        ((char*)(x)<Heapbase+HEAPBLOCK)
#define LFIT(x)        ((char*)(x)>= Heapbase)
#define HEADERC(x)     (x-sizeof(btheader))
#define HEADER(x)      (btheader*)(x-sizeof(btheader))
#define LEFTNBR(p)     (btheader*)((char*)(p)-(p)->prevsize-sizeof(btheader))
#define RIGHTNBR(p)    (btheader*)((char*)(p)+sizeof(btheader)+(p)->size)
#define SETPREVSIZE(p) {btheader*r=RIGHTNBR(p);if(RFIT(r)&&r->used)r->prevsize=(p)->size;}
typedef enum {false, true} bool;

struct _btheader {
  struct _btheader *prev;
  struct _btheader *next;
  bool used;
  int prevsize;
  int size;
};
typedef struct _btheader btheader;

btheader Head = {.used = false};
char *Heapbase;

void btinfo(int verbose) {
  btheader *p = (btheader*)Heapbase;
  int nfree = 0;
  int nused = 0;
  do {
    assert(p->size >= GRANULARITY);
    if (p->used) nused++;
    else         nfree++;

    if (p->used) printf(RED "[" WHITE "%d", p->size);
    else         printf(GREEN "[" WHITE "%d", p->size);
    if (verbose >= 1) printf(" @ " PRIPTR, LEASTSIG(p) + sizeof(btheader));
    if (verbose >= 2) {
      if (p->used) printf(" prevsize=%d", p->prevsize);
      else {
	if (p->prev == &Head) printf(" prev=" CYAN "HEAD" WHITE);
	else                  printf(" prev=" PRIPTR, LEASTSIG(p->prev));
	if (p->next == &Head) printf(" next=" CYAN "HEAD" WHITE);
	else                  printf(" next=" PRIPTR, LEASTSIG(p->next));
      }
    }
    if (p->used) printf(RED "]" WHITE);
    else         printf(GREEN "]" WHITE);
    printf(" ");
    p = (btheader*)((char*)p + sizeof(btheader) + p->size);
  } while ((char*)p < Heapbase + HEAPBLOCK);
  printf("\n%d blocks total: %d " RED "used" WHITE ", %d " GREEN "free" WHITE "\n", nfree+nused, nused, nfree);
}

void *btmalloc(int n) {
  // Has the heap been initialized yet?
  if (!Head.used) {
    Heapbase = sbrk(0);
    sbrk(HEAPBLOCK);
    btheader *p = (btheader*)Heapbase;
    p->used = false;
    p->prev = &Head;
    p->next = &Head;
    p->size = HEAPBLOCK - sizeof(btheader);
    p->size += ALIGNOFFR((char*)p + p->size);
    Head.used = true;
    Head.prev = p;
    Head.next = p;
  }

  btheader *p = &Head;
#if HEURISTIC == 1
  btheader *best; int bestsize = INT_MAX;
#elif HEURISTIC == 2
  btheader *worst; int worstsize = 0;
#endif

  //// Find a block from the free list.
  while (true) {
    if (INFOVERBOSE >= 2) printf("(btmalloc) p = " PRIPTR " -> " PRIPTR " \n", LEASTSIG(p), LEASTSIG(p->next));
    p = p->next;

#if HEURISTIC == 0
    // We couldn't find a large enough free block.
    if (p == &Head) return NULL;
#elif HEURISTIC == 1
    if (p == &Head) {
      if (bestsize == INT_MAX) return NULL;
      else break;
    }
#elif HEURISTIC == 2
    if (p == &Head) {
      if (worstsize == 0) return NULL;
      else break;
    }
#endif

    // This free block is too small.
    if (n > p->size) continue;

#if HEURISTIC == 0
    break;
#elif HEURISTIC == 1
    if (p->size < bestsize) { bestsize = p->size; best = p; }
#elif HEURISTIC == 2
    if (p->size > worstsize) { worstsize = p->size; worst = p; }
#endif
  };
  //// End of while loop

#if HEURISTIC == 0
  char *pc = (char*)p;
#elif HEURISTIC == 1
  p = best; char *pc = (char*)best;
#elif HEURISTIC == 2
  p = worst; char *pc = (char*)worst;
#endif
  // In full, it is "pc + sizeof(btheader)) + p->size - (n + sizeof(btheader))".
  char *qc = pc + p->size - n;
  int off = ALIGNOFFL(qc);
  qc -= off;
  btheader *q = (btheader*)qc;

  if (qc-pc < sizeof(btheader) + GRANULARITY) {
    // The entire block is reserved, delete it from free list.
    p->used = true;
    p->prev->next = p->next;
    p->next->prev = p->prev;
    SETPREVSIZE(p);
    return pc + sizeof(btheader);
  }
  else {
    // Truncate the free block.
    q->used = true;
    q->size = n + off;
    p->size -= n + off + sizeof(btheader);
    q->prevsize = p->size;
    SETPREVSIZE(q);
    return qc + sizeof(btheader);
  }
}

void btfree(void *x) {
  char *pc = HEADERC(x);
  btheader *p = (btheader*)pc;
  assert(p->used);
  btheader *left = LEFTNBR(p);
  btheader *right = RIGHTNBR(p);

  if (!LFIT(left) || left->used) {
    p->used = false;                    // USED FREE USED  or  [ FREE USED   or
    if (!RFIT(right) || right->used) {  // USED FREE ]     or  [ FREE ]
      // Insert p to free list, behind Head
      p->next = &Head;
      p->prev = Head.prev;
      Head.prev->next = p;
      Head.prev = p;
    }
    else {                              // USED FREE FREE  or  [ FREE FREE
      p->size += sizeof(btheader) + right->size;
      // Modify links so that p instead of right is in the free list
      right->prev->next = p;
      right->next->prev = p;
      p->next = right->next;
      p->prev = right->prev;
      SETPREVSIZE(p);
    }
  }
  else {
    left->size += sizeof(btheader) + p->size;
    if (!RFIT(right)) return;           // FREE FREE ]

    if (!right->used) {                 // FREE FREE FREE
      left->size += sizeof(btheader) + right->size;
      SETPREVSIZE(left);
      // Remove right from free list
      right->prev->next = right->next;
      right->next->prev = right->prev;
    }
    else {                              // FREE FREE USED
      right->prevsize = left->size;
    }
  }
}

void btreset() {
  Head.used = false;
  sbrk(-HEAPBLOCK);  // Reset the heap
}

void bt_singlemalloctest(int n) {
#if HEURISTIC != 0
  printf("[TEST] Single malloc test requires HEURISTIC=0 (FIRSTFIT)\n");
#else
  printf("[TEST] Single malloc: %d bytes\n", n);
  char *x = btmalloc(n);
  if (n > HEAPBLOCK - sizeof(btheader)) {
    assert(!x);
    return;
  }
  assert(x);
  btinfo(2);

  if (n == HEAPBLOCK - sizeof(btheader)) {
    btheader *p1 = HEADER(x);
    assert(Head.prev == &Head);
    assert(Head.next == &Head);

    assert(p1->size == n);
    assert(Head.used);
    assert(p1->used);
  }
  else {
    btheader *p1 = Head.next;
    assert(p1->next == &Head);
    assert(Head.prev == p1);
    assert(p1->prev == &Head);

    btheader *p = HEADER(x);
    int off = ALIGNOFFL((char*)p1 + p1->size - n);
    assert(p1->size == HEAPBLOCK - sizeof(btheader) - (n + sizeof(btheader) + off));
    assert(p->size == n + off);
    assert(Head.used);
    assert(!p1->used);
    assert(p->used);
  }
  btreset();
#endif
}

//// To verify the correctness of malloc, free, and their interactions
void bt_simpleworkloadtest() {
#if HEAPBLOCK != 4096
  printf("[TEST] Simple workload test requires HEAPBLOCK=4096\n");
#else
  printf("[TEST] Simple workload\n");
  char *x1 = btmalloc(512);
  char *x2 = btmalloc(256);
  char *x3 = btmalloc(128);
  btheader *p1 = HEADER(x1);
  btheader *p2 = HEADER(x2);
  btheader *p3 = HEADER(x3);

  assert(p1->size == 512);
  assert(p2->size == 256);
  assert(p3->size == 128);
  assert(p1->prevsize == 256);
  assert(p2->prevsize == 128);
  assert(p3->prevsize == 3072);

  //// Case 1: left used, right used
  btfree(x2);
  // The free list has 3 elements, including Head
  assert(Head.prev->prev->prev == &Head);
  assert(Head.next->next->next == &Head);

  char *x4 = btmalloc(512);
  btheader *p4 = HEADER(x4);
  assert(p3->prevsize == 512);
  //// Case 2: left used, right unused
  btfree(x3);
  assert(p3->size == 416);
  assert(Head.prev->prev->prev == &Head);
  assert(Head.next->next->next == &Head);

  char *x5 = btmalloc(2528);
  char *x6 = btmalloc(416);
  btfree(x4);
  //// Case 3: left unused, right used
  btfree(x6);
  assert(p4->size == 960);
  assert(p1->prevsize == 960);
  assert(Head.prev->prev == &Head);
  assert(Head.next->next == &Head);

  char *x7 = btmalloc(32);
  char *x8 = btmalloc(32);
  btfree(x7);
  //// Case 4: left unused, right unused
  btfree(x8);
  assert(p4->size == 960);
  assert(p1->prevsize == 960);
  assert(Head.prev->prev == &Head);
  assert(Head.next->next == &Head);
#endif
}

#define FREE     btfree
#define MALLOC   btmalloc
#define RESET    btreset
#define INFO()   btinfo(INFOVERBOSE)
#include "bench.c"

int main() {
  bt_singlemalloctest(10);
  bt_singlemalloctest(HEAPBLOCK - sizeof(btheader));
  bt_singlemalloctest(HEAPBLOCK);
  bt_simpleworkloadtest();
  bench(BENCHVERBOSE);
  return 0;
}