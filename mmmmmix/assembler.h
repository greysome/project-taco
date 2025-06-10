#ifndef _ASSEMBLER_H
#define _ASSEMBLER_H
#include <ctype.h>
#include <stdlib.h>
#include "emulator.h"

// Stores information about a future reference or a literal constant
// to be filled in later.
typedef struct {
  int addr;
  bool resolved;
  bool which;              // False for sym, true for literal.
  char sym[11];
  word literal;
} futureref;

#define MAXSYMS 1000
typedef struct {
  int star;
  int numsyms, numfuturerefs;
  char syms[MAXSYMS][11];
  word symvals[MAXSYMS];
  futureref futurerefs[MAXSYMS];
  int localsymcounts[10];  // The current number of instances of each local symbol <n>H.
} parsestate;

// Returned by reference in parseline() for mmm to use.
typedef struct {
  bool setdebugline;
  bool isend;
} extraparseinfo;

void initparsestate(parsestate *ps);

bool lookupsym(char *sym, word *val, parsestate *ps);

bool parsesym(char **s, char *sym);
bool parseoperator(char **s, byte *C, byte *F);
bool parsenum(char **s, int *val);

// parseatomic() and the rest all write into a word instead of an int,
// because words and ints are NOT equivalent.
// Specifically, +0 != -0.

bool parseatomic(char **s, word *val, parsestate *ps);
bool parseexpr(char **s, word *val, parsestate *ps);
bool parseA(char **s, word *A, parsestate *ps);
bool parseI(char **s, byte *I, parsestate *ps);
bool parseF(char **s, byte *F, parsestate *ps);
bool parseW(char **s, word *W, parsestate *ps);

bool parseline(char *line, parsestate *ps, mix *mix, extraparseinfo *extraparseinfo);
#endif
