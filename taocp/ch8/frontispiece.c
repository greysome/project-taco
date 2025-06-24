#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef enum {R,E,C,U,S,I,O,N,D} letter;
char CHARS[] = "recusiond";
int BITMAPS[] = {
  0b1101001101010011,
  0b1110000111110110,
  0b1110000100011110,
  0b0110100110011001,
  0b0111110000111110,
  0b0111001000100111,
  0b0110100110010110,
  0b1001110110111001,
  0b0111100110010111
};
int LENGTHS[] = {9,10,8,8,10,8,8,10,10};
letter RECURSED[] = {R,E,C,U,R,S,E,D};
letter RECURSIONS[] = {R,E,C,U,R,S,I,O,N,S};
letter *RECURSION = (letter *)RECURSIONS;  // Save 36 bytes of space

#define DEPTH 3
int INCS[DEPTH+1];
int BUFDIM;
char *BUF;

void recurse(int depth, int l, int x, int y) {
  letter *word; int len = LENGTHS[l];
  if (len == 8) word = RECURSED;
  else if (len == 9) word = RECURSION;
  else if (len == 10) word = RECURSIONS;

  int bitmap = BITMAPS[l];
  int inc = INCS[depth-1];
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      int bit = bitmap & 1;
      if (bit) {
	if (depth == 0) BUF[y*BUFDIM+x] = CHARS[l];
	else recurse(depth-1, *(word++), x+j*inc, y+i*inc);
      }
      bitmap >>= 1;
    }
  }
}

int main() {
  //// Populate increment list
#define PADBASE 0.35
  float pad = PADBASE;
  int n = 1, p = 0;
  for (int i = 0; i < DEPTH; i++) {
    INCS[i] = n+p;
    pad *= 4.0;
    n = 4*n + 3*p;
    p = (int)roundf(pad);
    BUFDIM = n;
  }

  //// Allocate buffer
  BUF = malloc((BUFDIM*BUFDIM+1)*sizeof(char));
  for (int i = 0; i < BUFDIM*BUFDIM; i++) BUF[i] = '.';
  BUF[BUFDIM*BUFDIM] = '\0';

  //// Let's go!
  recurse(DEPTH, 0, 0, R);

  //// Print populated buffer
  while (*BUF) {
    for (int i = 0; i < BUFDIM; i++, BUF++) putchar(*BUF);
    putchar('\n');
  }
}