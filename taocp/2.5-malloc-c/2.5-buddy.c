//// Buddy malloc, based on 2.5
////
//// 1. Like btmalloc, the heap has a fixed size to simplify the coding.
////
//// 2. Besides the list heads, headers are stored at the start of a
////    block and the appropriate offset is returned after a malloc()
////    call.
////
//// 3. To make the coding easier for myself, I was extremely wasteful
////    with memory: I reserved memory for the prev and next fields in
////    used blocks, even though they are not needed. Besides,
////    allocation sizes are more likely to be powers of 2, but the
////    extra space of the header causes malloc() to allocate a block
////    whose size is the next power of 2. Thus half the block is left
////    unused.
////
//// 4. I also waste some memory with the list heads: realistically
////    there won't be any blocks of size 1,2,4,8,16 because of the
////    header alone.

//// Modifiable parameters
#define HEAPEXP       16          // Heap size is 2^HEAPEXP
#define SIZEDIST      2           // Size distribution (see bench.c)
#define LIFETIMEDIST  1           // Lifetime distribution (see bench.c)
#define INFOVERBOSE   0           // budinfo() verbosity level -- 0=size, 1=+address, 2=+prev,next, 3=+lists
#define BENCHVERBOSE  0           // bench() verbosity level (see bench.c)
#define ITERS         100000      // Bench iteration

//// From here on out is the actual code
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include "common.h"

#define HEADER(x)     (header*)(x-sizeof(header))
#define ISLISTHEAD(p) ((char*)Listheads<=(char*)(p)&&(char*)(p)<=(char*)(Listheads+HEAPEXP+1))
typedef enum {false, true} bool;

struct _header {
  bool used;
  int exp;
  struct _header *prev;
  struct _header *next;
};
typedef struct _header header;

header Listheads[HEAPEXP+1] = {(header){.used = false}};
char *Heapbase;

void budinfo(int verbose) {
  if (verbose >= 3) {
    for (int k = 0; k <= HEAPEXP; k++) {
      printf("%d: ", 1<<k);
      header *p = Listheads[k].next;
      while (p != Listheads+k) {
        printf(PRIPTR " ", LEASTSIG(p));
      	p = p->next;
      }
      // In case the while block loops infinitely
      //printf(PRIPTR " " PRIPTR " " PRIPTR, LEASTSIG(&Listheads[k]), LEASTSIG(p), LEASTSIG(p->next));
      printf("\n");
    }
  }

  header *p = (header*)Heapbase;
  int nfree = 0;
  int nused = 0;
  do {
    if (p->used) nused++;
    else         nfree++;

    if (p->used) printf(RED "[" WHITE "%d", 1<<p->exp);
    else         printf(GREEN "[" WHITE "%d", 1<<p->exp);
    if (verbose >= 1) printf(" @ " PRIPTR, LEASTSIG(p) + sizeof(header));
    if (verbose >= 2) {
      if (!p->used) {
	if (ISLISTHEAD(p->prev)) printf(" prev=" CYAN "HEAD(%d)" WHITE, p->exp);
	else                     printf(" prev=" PRIPTR, LEASTSIG(p->prev));
	if (ISLISTHEAD(p->next)) printf(" next=" CYAN "HEAD(%d)" WHITE, p->exp);
	else                     printf(" next=" PRIPTR, LEASTSIG(p->next));
      }
    }
    if (p->used) printf(RED "]" WHITE);
    else         printf(GREEN "]" WHITE);
    printf(" ");
    p = (header*)((char*)p + (1<<p->exp));
  } while ((char*)p < Heapbase + (1<<HEAPEXP));
  printf("\n%d blocks total: %d " RED "used" WHITE ", %d " GREEN "free" WHITE "\n", nfree+nused, nused, nfree);
}

void budreset() {
  Listheads[0].used = true;
  // For k < HEAPEXP, the list of size 2^k blocks consists only of the list head.
  header *lh;
  for (int k = 0; k < HEAPEXP; k++) {
    lh = Listheads + k;
    lh->prev = lh;
    lh->next = lh;
    lh->exp = k;
  }

  // The list of size 2^HEAPEXP blocks has one element (the entire heap).
  Heapbase = sbrk(0); sbrk(1<<HEAPEXP);
  lh = Listheads + HEAPEXP;
  *((header*)Heapbase) = (header){.used = false, .prev = lh, .next = lh, .exp = HEAPEXP};
  lh->prev = (header*)Heapbase;
  lh->next = (header*)Heapbase;
  lh->exp = HEAPEXP;
}

void *budmalloc(uint32_t n) {
  // Has the heap been initialized already?
  if (!Listheads[0].used) budreset();

  n += sizeof(header);  // The header is included in the block
  //// Determine the exponent, which is the smallest number j such that 2^j >= n.
  int j = __builtin_clz(n);
  if ((n & (n-1)) == 0) j = 31 - j;  // n is a power of 2?
  else                  j = 32 - j;

  //// Find the smallest j <= k <= n such that the list of size 2^k blocks is nonempty.
  int k; header *lh;
  for (k = j; k <= HEAPEXP; k++) {
    lh = Listheads + k;
    if (lh->next != lh) break;
  }
  if (k == HEAPEXP+1 && lh->next == lh) return NULL;

  //// Split the block until it has size 2^j.
  header *p = lh->next;
  p->used = true;
  p->exp = j;
  /// Remove it from the list of size 2^k blocks.
  p->prev->next = p->next;
  p->next->prev = p->prev;

  while (k > j) {
    k--;
    /// Add its size 2^k buddy to the list of size 2^k blocks (behind the list head).
    void *buddy = (void*)(
      ( ( (uintptr_t)p - (uintptr_t)Heapbase ) ^ (1<<k) )  +
      (uintptr_t)Heapbase
    );
    lh = Listheads + k;
    header *q = buddy;
    q->used = false;
    q->exp = k;
    q->next = lh;
    q->prev = lh->prev;
    lh->prev->next = q;
    lh->prev = q;
  }
  return (char*)p + sizeof(header);
}

void budfree(void *x) {
  header *p = HEADER(x);
  p->used = false;

checkbuddy:
  if (p->exp == HEAPEXP) {
    header *lh = Listheads + HEAPEXP;
    lh->next = p;
    lh->prev = p;
    p->next = lh;
    p->prev = lh;
    return;
  }

  header *buddy = (header*)(
    ( ( (uintptr_t)p - (uintptr_t)Heapbase ) ^ (1<<p->exp) )  +
    (uintptr_t)Heapbase
  );

  if (buddy->used || buddy->exp != p->exp) {
    //// Insert p into list of size 2^(p->exp) blocks
    header *lh = Listheads + p->exp;
    p->next = lh;
    p->prev = lh->prev;
    lh->prev->next = p;
    lh->prev = p;
    return;
  }

  //// Remove buddy from list of size 2^(p->exp) blocks
  buddy->prev->next = buddy->next;
  buddy->next->prev = buddy->prev;
  if ((char*)buddy < (char*)p) p = buddy;
  p->exp++;
  goto checkbuddy;
}

#define FREE     budfree
#define MALLOC   budmalloc
#define RESET    budreset
#define INFO()   budinfo(INFOVERBOSE)
#include "bench.c"

int main() {
  budreset();
  bench(BENCHVERBOSE);
  return 0;
}