#ifndef _MMM_H
#define _MMM_H

#include <termios.h>
#include "assembler.h"
#include "emulator.h"
#include "mylib/str.h"
#include "mylib/arena.h"

#define MMM_LINE_LEN 128
#define MMM_DASHBOARD_BUFSIZE (1<<20)
#define MMM_ARENA_SIZE (1<<22)

typedef struct {
  Mix mix;
  ParseState ps;
  Arena arena;
  Str prev_line;

  Str mixal_file;
  Str card_reader_file;
  Str card_punch_file;
  Str tape_files[8];

  Str source_lines[4000];
  ExtraParseInfo extra_parse_infos[4000];

  bool should_trace;
  struct termios orig_termios;
  struct termios cbreak;

  bool dashboard_run_continuously;
  char *dashboard_buf;
} MMM;

#endif

// No implementation, just an empty object file
