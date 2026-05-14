//// Boundary-tag malloc, based on 2.5
////
//// 1. For simplicity, the heap has a fixed size. Of course a real
////    malloc implementation should grow the heap when the memory runs
////    out.
////
//// 2. In Knuth, the block size is stored in the header and footer.
////    It is a bit annoying to work with footers so I instead store
////    size in the header of the current and next block (if it is not
////    the last block) -- this is the purpose of the prevsize
////    field. Thus every time the size of a block changes, the macro
////    SETPREVSIZE() has to be called to update the prevsize field of
////    the next block.
////
//// 3. All headers are aligned to multiples of sizeof(header). This
////    wastes memory but simplifies some aspects of the coding (e.g.
////    there is no risk of a malloc partially overwriting a header).
////
//// 4. The next links may not go in increasing memory addresses,
////    because a freed block whose adjacent blocks are used is
////    appended to the end of the free list (since we don't
////    immediately know the free blocks nearest to it). This makes it
////    a bit messier to reason with next and prev pointers, and they
////    have to be distinguished from left and right neighbours. The
////    solution to 2.5--19 mentions an alternative approach where
////    collapsing is not done during free, but deferred until the
////    search through the free list in malloc.
////
////    Question to investigate: does this spoil the utility of the
////    rover trick (2.5--6)?
////
//// 5. It is a bit annoying that I have to keep casting between char*
////    and header*. This makes the code "noisier" as compared to
////    assembly.

//// Modifiable parameters
#define HEAPSIZE       131072
#define GRANULARITY    8          // Free blocks are no smaller than this (excluding the header)
#define HEURISTIC      2          // 0=firstfit, 1=bestfit, 2=worstfit
#define SIZEDIST       0          // Size distribution (see bench.c)
#define LIFETIMEDIST   1          // Lifetime distribution (see bench.c)
#define INFOVERBOSE    0          // btinfo() verbosity level -- 0=size, 1=+address, 2=+prev,next
#define BENCHVERBOSE   1          // bench() verbosity level (see benh.c)
#define ITERS          100000     // Number of bench iterations

//// From here on out is the actual code
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include "common.h"

#define ALIGNOFFL(p)   ( (uintptr_t)(p))%sizeof(header)
#define ALIGNOFFR(p)   (-(uintptr_t)(p))%sizeof(header)
#define RFIT(x)        ((char*)(x)<Heapbase+HEAPSIZE)
#define LFIT(x)        ((char*)(x)>= Heapbase)
#define HEADERC(x)     (x-sizeof(header))
#define HEADER(x)      (header*)(x-sizeof(header))
#define LEFTNBR(p)     (header*)((char*)(p)-(p)->prevsize-sizeof(header))
#define RIGHTNBR(p)    (header*)((char*)(p)+sizeof(header)+(p)->size)
#define SETPREVSIZE(p) {header*r=RIGHTNBR(p);if(RFIT(r)&&r->used)r->prevsize=(p)->size;}
typedef enum {false, true} bool;

struct _header {
  struct _header *prev;
  struct _header *next;
  bool used;
  int prevsize;
  int size;
};
typedef struct _header header;

header Head = {.used = false};
char *Heapbase;

void btinfo(int verbose) {
  header *p = (header*)Heapbase;
  int nfree = 0;
  int nused = 0;
  do {
    assert(p->size >= GRANULARITY);
    if (p->used) nused++;
    else         nfree++;

    if (p->used) printf(RED "[" WHITE "%d", p->size);
    else         printf(GREEN "[" WHITE "%d", p->size);
    if (verbose >= 1) printf(" @ " PRIPTR, LEASTSIG(p) + sizeof(header));
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
    p = (header*)((char*)p + sizeof(header) + p->size);
  } while ((char*)p < Heapbase + HEAPSIZE);
  printf("\n%d blocks total: %d " RED "used" WHITE ", %d " GREEN "free" WHITE "\n", nfree+nused, nused, nfree);
}

void *btmalloc(int n) {
  // Has the heap been initialized already?
  if (!Head.used) {
    Heapbase = sbrk(0);
    sbrk(HEAPSIZE);
    header *p = (header*)Heapbase;
    p->used = false;
    p->prev = &Head;
    p->next = &Head;
    p->size = HEAPSIZE - sizeof(header);
    p->size += ALIGNOFFR((char*)p + p->size);
    Head.used = true;
    Head.prev = p;
    Head.next = p;
  }

  header *p = &Head;
#if HEURISTIC == 1
  header *best; int bestsize = INT_MAX;
#elif HEURISTIC == 2
  header *worst; int worstsize = 0;
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
  // In full, it is "pc + sizeof(header)) + p->size - (n + sizeof(header))".
  char *qc = pc + p->size - n;
  int off = ALIGNOFFL(qc);
  qc -= off;
  header *q = (header*)qc;

  if (qc-pc < sizeof(header) + GRANULARITY) {
    // The entire block is reserved, delete it from free list.
    p->used = true;
    p->prev->next = p->next;
    p->next->prev = p->prev;
    SETPREVSIZE(p);
    return pc + sizeof(header);
  }
  else {
    // Truncate the free block.
    q->used = true;
    q->size = n + off;
    p->size -= n + off + sizeof(header);
    q->prevsize = p->size;
    SETPREVSIZE(q);
    return qc + sizeof(header);
  }
}

void btfree(void *x) {
  char *pc = HEADERC(x);
  header *p = (header*)pc;
  assert(p->used);
  header *left = LEFTNBR(p);
  header *right = RIGHTNBR(p);

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
      p->size += sizeof(header) + right->size;
      // Modify links so that p instead of right is in the free list
      right->prev->next = p;
      right->next->prev = p;
      p->next = right->next;
      p->prev = right->prev;
      SETPREVSIZE(p);
    }
  }
  else {
    left->size += sizeof(header) + p->size;
    if (!RFIT(right)) return;           // FREE FREE ]

    if (!right->used) {                 // FREE FREE FREE
      left->size += sizeof(header) + right->size;
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
  sbrk(-HEAPSIZE);  // Reset the heap
}

void bt_singlemalloctest(int n) {
#if HEURISTIC != 0
  printf("[TEST] Single malloc test requires HEURISTIC=0 (FIRSTFIT)\n");
#else
  printf("[TEST] Single malloc: %d bytes\n", n);
  char *x = btmalloc(n);
  if (n > HEAPSIZE - sizeof(header)) {
    assert(!x);
    return;
  }
  assert(x);
  btinfo(2);

  if (n == HEAPSIZE - sizeof(header)) {
    header *p1 = HEADER(x);
    assert(Head.prev == &Head);
    assert(Head.next == &Head);

    assert(p1->size == n);
    assert(Head.used);
    assert(p1->used);
  }
  else {
    header *p1 = Head.next;
    assert(p1->next == &Head);
    assert(Head.prev == p1);
    assert(p1->prev == &Head);

    header *p = HEADER(x);
    int off = ALIGNOFFL((char*)p1 + p1->size - n);
    assert(p1->size == HEAPSIZE - sizeof(header) - (n + sizeof(header) + off));
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
#if HEAPSIZE != 4096 || HEURISTIC != 0
  printf("[TEST] Simple workload test requires HEAPSIZE=4096 and HEURISTIC=0 (firstfit)\n");
#else
  printf("[TEST] Simple workload\n");
  char *x1 = btmalloc(512);
  char *x2 = btmalloc(256);
  char *x3 = btmalloc(128);
  header *p1 = HEADER(x1);
  header *p2 = HEADER(x2);
  header *p3 = HEADER(x3);

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
  header *p4 = HEADER(x4);
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
  bt_singlemalloctest(HEAPSIZE - sizeof(header));
  bt_singlemalloctest(HEAPSIZE);
  bt_simpleworkloadtest();
  bench(BENCHVERBOSE);
  return 0;
}