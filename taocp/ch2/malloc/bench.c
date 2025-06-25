//// Poor man's RNG
unsigned int _x = 0;
#define MM     4294967296u
#define AA     1664525
#define CC     1013904223
#define RAND() ((float)(_x=(AA*_x+CC)%MM)/MM)

#if SIZEDIST == 0
#define SIZE()     (int)(RAND()*1900+100)
#elif SIZEDIST == 1
int SIZE() {
  float x = RAND();
  if (x > 0.5) return 1;
  else if (x > 0.25) return 2;
  else if (x > 0.125) return 4;
  else if (x > 0.0625) return 8;
  else if (x > 0.03125) return 16;
  else return 32;
}
#elif SIZEDIST == 2
int SIZE() {
  int n = (int)(RAND()*22);
  switch (n) {
  case 0: return 10;
  case 1: return 12;
  case 2: return 14;
  case 3: return 16;
  case 4: return 18;
  case 5: return 20;
  case 6: return 30;
  case 7: return 40;
  case 8: return 50;
  case 9: return 60;
  case 10: return 70;
  case 11: return 80;
  case 12: return 90;
  case 13: return 100;
  case 14: return 150;
  case 15: return 200;
  case 16: return 250;
  case 17: return 500;
  case 18: return 1000;
  case 19: return 2000;
  case 20: return 3000;
  case 21: return 4000;
  }
  assert(false && "this shouldn't be reached");
}
#endif

#if LIFETIMEDIST == 0
#define LIFETIME() (int)(RAND()*9+1)
#elif LIFETIMEDIST == 1
#define LIFETIME() (int)(RAND()*99+1)
#elif LIFETIMEDIST == 2
#define LIFETIME() (int)(RAND()*999+1)
#endif

typedef struct {
  int freetime;
  void *p;
} node;
node nodes[4096]; int nodesptr = 0;

int Time;

void bench(int verbose) {
  RESET();
  for (Time = 0; Time < ITERS; Time++) {
    //// Free blocks
    for (int i = 0; i < nodesptr; i++) {
      if (nodes[i].freetime == Time) {
	if (verbose >= 2) printf("[BENCH] TIME=%d: FREE " PRIPTR "\n", Time, LEASTSIG(nodes[i].p));
	FREE(nodes[i].p);
	nodes[i] = nodes[(nodesptr--)-1];
	i--;
	if (verbose >= 100) INFO();
      }
    }
    //// Allocate block randomly
    int size = SIZE();
    int lifetime = LIFETIME();
    void *p = MALLOC(size);
    if (verbose >= 2) printf("[BENCH] TIME=%d: MALLOC %d bytes at " PRIPTR " lasting %du\n", Time, size, LEASTSIG(p), lifetime);
    if (!p) {
      printf("[BENCH] TIME=%d: Ran out of memory; exiting\n", Time);
      INFO();
      break;
    }
    nodes[nodesptr++] = (node){.freetime = Time+lifetime, .p = p};

    if (verbose < 2) {
      if (Time % 50 == 0) {
	printf("[BENCH] TIME=%d\n", Time);
	INFO();
	printf("\n");
      }
    }
    if (verbose >= 2) { INFO(); printf("\n"); }
  }
  RESET();
}