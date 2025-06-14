#ifndef _MMM_H
#define _MMM_H

#include "mmm.h"

#define LINELEN 100

typedef struct {
  mix mix;
  parsestate ps;
  char prevline[LINELEN];
  char cardreaderfile[LINELEN];
  char cardpunchfile[LINELEN];
  char tapefiles[8][LINELEN];
  char debuglines[4000][LINELEN];
  bool shouldtrace;
} mmmstate;

#endif