//// Boundary-tag malloc

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>

#define HEAPBLOCK      4096
#define ALIGNOFFL(p)   ( (uintptr_t)(p))%_Alignof(btheader)
#define ALIGNOFFR(p)   (-(uintptr_t)(p))%_Alignof(btheader)
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

#define WHITE  "\033[37m"
#define YELLOW "\033[92m"
#define CYAN   "\033[36m"
void printheap(int verbose) {
  btheader *p = (btheader*)Heapbase;
  do {
    printf(YELLOW "[" WHITE "%d", p->size);
    if (verbose >= 1) printf(" @ %p", p);
    if (verbose >= 2) {
      if (p->used) printf(" prevsize=%d", p->prevsize);
      else {
	if (p->prev == &Head) printf(" prev=HEAD");
	else printf(" prev=%p", p->prev);
	if (p->next == &Head) printf(" next=HEAD");
	else printf(" next=%p", p->next);
      }
    }
    printf(YELLOW "]" WHITE);
    if (p->used) printf(YELLOW "*" WHITE);
    printf(" ");
    p = (btheader*)((char*)p + sizeof(btheader) + p->size);
  } while ((char*)p < Heapbase + HEAPBLOCK);
  printf("\n");
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
  // Find a block from the free list.
  while (true) {
    p = p->next;
    // We couldn't find a large enough free block.
    if (p == &Head) break;
    // This free block is too small.
    if (n > p->size) continue;

    // In full, it is "((char*)p + sizeof(*p)) + p->size - (n + sizeof(*q))".
    char *qc = (char*)p + p->size - n;
    int off = ALIGNOFFL(qc);
    qc -= off;
    btheader *q = (btheader*)qc;

    if (q == p) {
      // The entire block is reserved, delete it from free list.
      p->used = true;
      p->prev->next = p->next;
      p->next->prev = p->prev;
      SETPREVSIZE(q);
      return (char*)p + sizeof(btheader);
    }
    else {
      // Truncate the free block.
      q->used = true;
      q->size = n + off;
      p->size -= n + off + sizeof(btheader);
      // Update prevsize
      q->prevsize = p->size;
      SETPREVSIZE(q);
      return qc + sizeof(btheader);
    }
  };
  return NULL;
}

void btfree(void *x) {
  char *pc = HEADERC(x);
  btheader *p = (btheader*)pc;
  assert(p->used);
  btheader *left = LEFTNBR(p);
  btheader *right = RIGHTNBR(p);

  if (!LFIT(left) || left->used) {
    p->used = false;
    if (!RFIT(right) || right->used) {  // USED FREE USED
      // Insert p to free list, behind Head
      p->next = &Head;
      p->prev = Head.prev;
      Head.prev->next = p;
      Head.prev = p;
    }
    else {                              // USED FREE FREE
      p->size += sizeof(btheader) + right->size;
      // Links to next should now be links to p
      right->prev->next = p;
      right->next->prev = p;
      // Add links from p
      p->next = right->next;
      p->prev = right->prev;
      SETPREVSIZE(p);
    }
  }
  else {
    left->size += sizeof(btheader) + p->size;
    if (RFIT(right) && !right->used) {  // FREE FREE FREE
      left->size += sizeof(btheader) + right->size;
      // Remove next from free list
      left->next = left->next->next;
      left->next->prev = left;
      SETPREVSIZE(left);
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
  printf("[TEST] Single malloc: %d bytes\n", n);
  char *x = btmalloc(n);
  if (n > HEAPBLOCK - sizeof(btheader)) {
    assert(!x);
    return;
  }
  assert(x);

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
}

//// To verify the correctness of malloc, free, and their interactions
void bt_simpleworkloadtest() {
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
}

int main() {
  bt_singlemalloctest(10);
  bt_singlemalloctest(HEAPBLOCK - sizeof(btheader));
  bt_singlemalloctest(HEAPBLOCK);
  bt_simpleworkloadtest();
  return 0;
}