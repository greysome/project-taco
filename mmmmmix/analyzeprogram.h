#ifndef _ANALYZEPROGRAM_H
#define _ANALYZEPROGRAM_H
#include "emulator.h"
#include "assembler.h"

typedef struct {
  mix mix;
  parsestate ps;
  char prevline[LINELEN];
  char globalcardfile[LINELEN];
  char globaltapefiles[8][LINELEN];
  char debuglines[4000][LINELEN];
  bool shouldtrace;
} mmmstate;

void analyzeprogram(mmmstate *mmm, int startaddr, int endaddr);

#endif