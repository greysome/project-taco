#ifndef RAND_H
#define RAND_H

#include <stdint.h>
#include <time.h>

void rand_init(void);
uint32_t rand_next(void);
float rand_next_f(void);
int rand_next_int(int high);

#endif // RAND_H

#ifdef RAND_IMPLEMENTATION

static uint64_t rand_state;

void rand_init(void) {
  rand_state = time(NULL);
}

uint32_t rand_next(void) {
  uint64_t m = 0x9b60933458e17d7d;
  uint64_t a = 0xd737232eeccdf7ed;
  rand_state = rand_state * m + a;
  int shift = 29 - (int)(rand_state >> 61);
  return (uint32_t)(rand_state >> shift);
}

float rand_next_f(void) {
  return (float)rand_next() / (float)(1ULL<<32);
}

int rand_next_int(int high) {
  return (int)(rand_next_f() * (float)high);
}

#endif // RAND_IMPLEMENTATION