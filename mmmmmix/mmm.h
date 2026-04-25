#ifndef _MMM_H
#define _MMM_H

#include <termios.h>
#include "assembler.h"
#include "emulator.h"

#define MMM_LINE_LEN 128
#define MMM_DASHBOARD_BUFSIZE (1<<20)

typedef struct {
  Mix mix;
  ParseState ps;
  char prev_line[MMM_LINE_LEN];

  char mixal_file[MMM_LINE_LEN];
  char card_reader_file[MMM_LINE_LEN];
  char card_punch_file[MMM_LINE_LEN];
  char tape_files[8][MMM_LINE_LEN];

  char source_lines[4000][MMM_LINE_LEN];
  ExtraParseInfo extra_parse_infos[4000];

  bool should_trace;
  struct termios orig_termios;
  struct termios cbreak;

  bool dashboard_run_continuously;
  char *dashboard_buf;
} MMM;

#endif

// No implementation, just an empty object file