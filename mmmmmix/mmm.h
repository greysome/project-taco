#ifndef _MMM_H
#define _MMM_H

#include <termios.h>
#include "assembler.h"
#include "emulator.h"

#define MMM_LINE_LEN 128
#define MMM_DASHBOARD_BUFSIZE (1<<20)

typedef struct {
  // Pointer because the arena might be used outside the scope of the
  // MMM instance, and we do not want any desyncs.
  Arena *arena;
  Arena *tmp_arena;
  Mix mix;
  ParseState ps;

  LineInfo line_infos[4000];

  char prev_line[MMM_LINE_LEN];
  StrView mixal_file;
  StrView device_files[21];

  bool should_trace;
  StrView trace_file;
  int trace_fd;
} MMM;

#endif

// No implementation, just an empty object file