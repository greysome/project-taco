#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include "mylib/arena.h"
#include "mylib/io.h"
#include "mylib/str.h"
#include "mylib/util.h"
#include "emulator.h"
#include "assembler.h"
#include "analyze_program.h"
#include "mmm.h"

static char *mix_device_names[] = {
  [TAPE_0] = "tp0",
  [TAPE_1] = "tp1",
  [TAPE_2] = "tp2",
  [TAPE_3] = "tp3",
  [TAPE_4] = "tp4",
  [TAPE_5] = "tp5",
  [TAPE_6] = "tp6",
  [TAPE_7] = "tp7",
  [DISK_0] = "dk0",
  [DISK_1] = "dk1",
  [DISK_2] = "dk2",
  [DISK_3] = "dk3",
  [DISK_4] = "dk4",
  [DISK_5] = "dk5",
  [DISK_6] = "dk6",
  [DISK_7] = "dk7",
  [CARD_READER] = "cr",
  [CARD_PUNCH] = "cp",
  [LINE_PRINTER] = "lp",
  [TERMINAL] = "term",
  [PAPER_TAPE] = "pt",
};

void mmm_init(MMM *mmm, Arena *arena, Arena *tmp_arena) {
  mmm->arena = arena;
  mmm->tmp_arena = tmp_arena;
  // mmm->mix will be initialized in load_mixal()
  mmm->mixal_file = sv_empty();
  for (int devid = 0; devid < MIX_DEVICE_COUNT; devid++) {
    mmm->device_files[devid] = sv_empty();
  }
  mmm->prev_line[0] = '\0';
  for (int addr = 0; addr < 4000; addr++) {
    mmm->line_infos[addr].is_mixal_line = false;
    mmm->line_infos[addr].is_con_or_alf = false;
  }
  mmm->should_trace = false;
  mmm->trace_file = sv_empty();
  mmm->trace_fd = -1;

  // Default IO operation times
  mmm->mix.IN_cycles[CARD_READER] = 10000;
  mmm->mix.IOC_cycles[LINE_PRINTER] = 10000;
  mmm->mix.OUT_cycles[LINE_PRINTER] = 7500;
  mmm->mix.OUT_cycles[CARD_PUNCH] = 10000;
  // I chose a value arbitrarily for tape IO because I haven't seen an
  // official figure yet
  for (int i = 0; i < 8; i++) {
    mmm->mix.IN_cycles[i] = 30000;
    mmm->mix.OUT_cycles[i] = 30000;
    mmm->mix.IOC_cycles[i] = 30000;
  }
}

bool try_open(StrView file, int flags, int mode, int *fd, Arena tmp_arena) {
  // Ensure file is null-terminated
  StrBuf sb = sb_from_sv(file, &tmp_arena);
  int fd0 = open(sb.s, flags, mode);
  if (fd0 == -1) {
    my_eprintf(RED("Error while open(%s) -- %s\n"), sb.s, strerror(errno));
    return false;
  }
  else {
    *fd = fd0;
    return true;
  }
}

bool load_mixal(StrView mixalfile, MMM *mmm) {
  Arena *arena = mmm->arena;
  mix_init(&mmm->mix);
  parse_state_init(&mmm->ps, arena);

  int source_fd;
  if (!try_open(mixalfile, O_RDONLY, 0, &source_fd, *mmm->tmp_arena))
    return false;

  struct stat statbuf;
  if (fstat(source_fd, &statbuf) == -1) {
    my_eprintf(RED("Error while stat(" sv_fmt ") -- %s\n"), sv_arg(mixalfile), strerror(errno));
    return false;
  }

  assert(statbuf.st_size >= 0);
  StrBuf source_buf = sb_alloc(statbuf.st_size, arena);
  if (read(source_fd, source_buf.s, statbuf.st_size) < statbuf.st_size) {
    my_eprintf(RED("Error while read(" sv_fmt ") -- %s\n"), sv_arg(mixalfile), strerror(errno));
    return false;
  }
  source_buf.len = statbuf.st_size;

  int line_num = 0;
  StrView source_view = sv_from_sb(source_buf);
  while (1) {
    line_num++;
    LineInfo *line_info = &mmm->line_infos[mmm->ps.star];
    if (!parse_line(&source_view, &mmm->ps, &mmm->mix, line_info)) {
      // TODO print offending line as well
      my_eprintf(RED("Assembler error at line %d\n"), line_num);
      return false;
    }
    if (line_info->is_end || sv_is_empty(source_view))
      break;
  }
  my_printf(GREEN("Loaded MIXAL file %s\n"), mixalfile.s);
  mmm->mixal_file = mixalfile;
  return true;
}

void sb_append_shortword(StrBuf *sb, word w, Arena *arena) {
  sb_appendf(sb, "%c %02d %02d", arena, w.sign ? '+' : '-', w.bytes[3], w.bytes[4]);
}

void sb_append_word(StrBuf *sb, word w, Arena *arena) {
  sb_appendf(sb, "%c %02d %02d %02d %02d %02d", arena,
             w.sign ? '+' : '-', w.bytes[0], w.bytes[1], w.bytes[2], w.bytes[3], w.bytes[4]);
}

// Display instruction in the format
// +- AAAA I F C     MIXAL source (if it exists)
void sb_append_mixal_line(StrBuf *sb, int addr, MMM *mmm, Arena *arena, char *override_color) {
  if (addr < 0 || addr >= 4000)
    my_eprintf(RED("Invalid memory address %04d\n"), addr);
  else {
    word w = mmm->mix.mem[addr];
    if (override_color == NULL) {
      sb_appendf(sb, BLUE("%04d") "  " YELLOW("%c %04d %02d %02d %02d\t"), arena,
                 addr, w.sign ? '+' : '-',
                 w.bytes[0] * MIX_BYTE_SIZE + w.bytes[1], w.bytes[2], w.bytes[3], w.bytes[4]);
    }
    else {
      sb_appendf(sb, "%s%04d  %c %04d %02d %02d %02d\t" ANSI_WHITE, arena,
                 override_color, addr, w.sign ? '+' : '-',
                 w.bytes[0] * MIX_BYTE_SIZE + w.bytes[1], w.bytes[2], w.bytes[3], w.bytes[4]);
    }

    if (mmm->line_infos[addr].is_mixal_line) {
      StrView sv = mmm->line_infos[addr].mixal_line;
      if (override_color != NULL)
        sb_append_cstr(sb, override_color, arena);
      sb_appendf(sb, sv_fmt, arena, sv_arg(sv));
      if (override_color != NULL)
        sb_append_cstr(sb, ANSI_WHITE, arena);
    }
    else
      sb_appendf(sb, "???\n", arena, addr);
  }
}

void sb_append_mixal_line_colored(StrBuf *sb, int addr, MMM *mmm, Arena *arena) {
  if (addr == mmm->mix.next_PC)
    sb_append_mixal_line(sb, addr, mmm, arena, ANSI_GREEN);
  else if (mmm->mix.is_overwritten[addr])
    sb_append_mixal_line(sb, addr, mmm, arena, ANSI_RED);
  else
    sb_append_mixal_line(sb, addr, mmm, arena, NULL);
}

void help_command() {
  my_print(
    "<empty>\t\trepeat previous command\n"
    "h\t\tprint this help\n"
    "q\t\tquit\n"
    "l\t\treload MIXAL, card and tape files\n"
    "c\t\tview CPU state\n"
    "io\t\tview IO devices\n"
    "+<device> <file>\tattach IO device\n"
    "-<device>\tdetach IO device\n"
    "g\t\trun whole program\n"
    "g+<file>\trun whole program and dump instruction trace (optional file)\n"
    "s\t\trun one step\n"
    "b<addr>\t\trun till address\n"
    "b.<sym>\t\trun till address at symbol\n"
    "v\t\tview MIXAL source lines\n"
    "vc\t\tview constants\n"
    "v.<sym>\t\tview memory address at symbol\n"
    "t\t\tprint timing statistics\n"
    "a<start>-<end>\tanalyze program\n"
  );
}

void quit_command(MMM *mmm) {
  for (int devid = 0; devid < MIX_DEVICE_COUNT; devid++) {
    StrView devfile = mmm->device_files[devid];
    int devfd = mmm->mix.device_fds[devid];
    if (!sv_is_empty(devfile)) {
      if (close(devfd) == -1)
        my_eprintf("Error while close(" sv_fmt ") -- %s", sv_arg(devfile), strerror(errno));
    }
  }
}

void attach_device(StrView file, int devid, MMM *mmm) {
  int fd;
  if (!try_open(file, O_RDWR, 0, &fd, *mmm->arena))
    return;
  char *devname = mix_device_names[devid];
  StrView *devfile = &mmm->device_files[devid];
  *devfile = file;
  mmm->mix.device_fds[devid] = fd;
  my_eprintf(GREEN("%s: " sv_fmt "\n"), mix_device_names[devid], sv_arg(*devfile));
}

void detach_device(int devid, MMM *mmm) {
  char *devname = mix_device_names[devid];
  int *devfd = &mmm->mix.device_fds[devid];
  if (*devfd == -1) {
    my_eprintf(GREEN("%s is not attached\n"), devname);
  }
  else {
    StrView *devfile = &mmm->device_files[devid];
    if (close(*devfd) == -1)
      my_eprintf("Error while close(" sv_fmt ") -- %s", sv_arg(*devfile), strerror(errno));
    else {
      *devfile = sv_empty();
      *devfd = -1;
      my_eprintf(GREEN("%s detached\n"), devname);
    }
  }
}

bool reset_command(MMM *mmm) {
  if (!load_mixal(mmm->mixal_file, mmm))
    return false;
  for (int devid = 0; devid < MIX_DEVICE_COUNT; devid++) {
    StrView devfile = mmm->device_files[devid];
    if (!sv_is_empty(devfile)) {
      attach_device(devfile, devid, mmm);
    }
  }
  return true;
}

void attach_command(StrView *sv, MMM *mmm) {
  StrView devname = sv_parse_word(sv);
  for (int devid = 0; devid < MIX_DEVICE_COUNT; devid++) {
    if (sv_match_cstr(&devname, mix_device_names[devid])) {
      StrView devfile = sv_parse_word(sv);
      attach_device(devfile, devid, mmm);
      return;
    }
  }
  my_eprint(RED("Invalid device name\n"));
}

void detach_command(StrView *sv, MMM *mmm) {
  StrView devname = sv_parse_word(sv);
  for (int devid = 0; devid < MIX_DEVICE_COUNT; devid++) {
    if (sv_match_cstr(&devname, mix_device_names[devid])) {
      detach_device(devid, mmm);
      return;
    }
  }
  my_eprint(RED("Invalid device name\n"));
}

void cpu_command(MMM *mmm) {
  Arena *arena = mmm->arena;
  static StrBuf sb = sb_empty_init;
  if (sb_is_empty(sb))
    sb = sb_alloc(1024, arena);

  sb_clear(&sb);
  sb_append_cstr(&sb, GRAY("rA  "), arena);
  sb_append_word(&sb, mmm->mix.A, arena);
  sb_appendf(&sb, "  " CYAN("(%ld)") "\t", arena, word_to_int(mmm->mix.A));

  sb_append_cstr(&sb, GRAY("rX  "), arena);
  sb_append_word(&sb, mmm->mix.X, arena);
  sb_appendf(&sb, "  " CYAN("(%ld)") "\n", arena, word_to_int(mmm->mix.X));

  sb_append_cstr(&sb, GRAY("rI1 "), arena);
  sb_append_shortword(&sb, mmm->mix.I[0], arena);
  sb_appendf(&sb, "  " CYAN("(%ld)") "\t\t", arena, word_to_int(mmm->mix.I[0]));

  sb_append_cstr(&sb, GRAY("rI2 "), arena);
  sb_append_shortword(&sb, mmm->mix.I[1], arena);
  sb_appendf(&sb, "  " CYAN("(%ld)") "\t\t", arena, word_to_int(mmm->mix.I[1]));

  sb_append_cstr(&sb, GRAY("rI3 "), arena);
  sb_append_shortword(&sb, mmm->mix.I[2], arena);
  sb_appendf(&sb, "  " CYAN("(%ld)") "\n", arena, word_to_int(mmm->mix.I[2]));

  sb_append_cstr(&sb, GRAY("rI4 "), arena);
  sb_append_shortword(&sb, mmm->mix.I[3], arena);
  sb_appendf(&sb, "  " CYAN("(%ld)") "\t\t", arena, word_to_int(mmm->mix.I[3]));

  sb_append_cstr(&sb, GRAY("rI5 "), arena);
  sb_append_shortword(&sb, mmm->mix.I[5], arena);
  sb_appendf(&sb, "  " CYAN("(%ld)") "\t\t", arena, word_to_int(mmm->mix.I[4]));

  sb_append_cstr(&sb, GRAY("rI6 "), arena);
  sb_append_shortword(&sb, mmm->mix.I[5], arena);
  sb_appendf(&sb, "  " CYAN("(%ld)") "\n", arena, word_to_int(mmm->mix.I[5]));

  sb_append_cstr(&sb, GRAY("rJ  "), arena);
  sb_append_shortword(&sb, mmm->mix.J, arena);
  sb_appendf(&sb, "  " CYAN("(%ld)") "\t\t", arena, word_to_int(mmm->mix.J));

  sb_append_cstr(&sb, GRAY("Flags "), arena);
  if (mmm->mix.cmp == 0)
    sb_append_cstr(&sb, "=", arena);
  else if (mmm->mix.cmp == 1)
    sb_append_cstr(&sb, ">", arena);
  else if (mmm->mix.cmp == -1)
    sb_append_cstr(&sb, "<", arena);
  else
    assert(false && "unreachable");
  if (mmm->mix.overflow)
    sb_append_cstr(&sb, "OV", arena);
  sb_append_cstr(&sb, "\n", arena);

  sb_append_mixal_line(&sb, mmm->mix.next_PC, mmm, arena, NULL);
  my_print(sb.s);
}

void io_command(MMM *mmm) {
  Arena *arena = mmm->arena;
  static StrBuf sb = sb_empty_init;
  if (sb_is_empty(sb))
    sb = sb_alloc(1024, arena);

  sb_clear(&sb);
  for (int devid = 0; devid < 21; devid++) {
    char *devname = mix_device_names[devid];
    StrView devfile = mmm->device_files[devid];
    sb_appendf(&sb, GRAY("%s: ") sv_fmt "\t", arena, devname, sv_arg(devfile));

    IOTask iotask = mmm->mix.iotasks[devid];
    if (iotask.timer > 0) {
      if (iotask.C == 35) {
        sb_appendf(&sb, "[" CYAN("IOC") " (%du left)]",
                  arena, iotask.timer);
      }
      else if (iotask.C == 36) {
        sb_appendf(&sb, "[" CYAN("IN") " @ " BLUE("%04d") " (%du left)]",
                  arena, iotask.M, iotask.timer);
      }
      else if (iotask.C == 37) {
        sb_appendf(&sb, "[" CYAN("OUT") " @ " BLUE("%04d") " (%du left)]",
                  arena, iotask.M, iotask.timer);
      }
    }
    sb_append_cstr(&sb, "\n", arena);
  }
  my_print(sb.s);
}

void time_command(MMM *mmm) {
  Arena *arena = mmm->arena;
  static StrBuf sb = sb_empty_init;
  if (sb_is_empty(sb))
    sb = sb_alloc(1024, arena);

  int totalcycles = 0;
  int maxdigits = 0;
  for (int i = 0; i < 4000; i++) {
    totalcycles += mmm->mix.trace_cycles[i];
    int nd = ndigits(mmm->mix.trace_counts[i]);
    if (nd > maxdigits)
      maxdigits = nd;
  }

  sb_clear(&sb);

  for (int addr = 0; addr < 4000; addr++) {
    int count = mmm->mix.trace_counts[addr];

    if (count >= 1) {
      char *fmt = "%*c%d times, %du";
      int startlen = sb.len;
      sb_appendf(&sb, fmt, arena,
                 maxdigits-ndigits(count)+1, ' ', count, mmm->mix.trace_cycles[addr]);
      int headerlen = sb.len - startlen;
      sb_appendf(&sb, "%*c", arena, 24-headerlen, ' ');
      sb_append_mixal_line(&sb, addr, mmm, arena, NULL);
    }
  }

  sb_appendf(&sb, "Total: %du\n", arena, totalcycles);
  my_print(sb.s);
}

bool mmm_step(MMM *mmm) {
  Arena *arena = mmm->arena;
  static StrBuf sb = sb_empty_init;
  if (sb_is_empty(sb))
    sb = sb_alloc(1024, arena);

  mix_step(&mmm->mix);

  // Write instruction trace into file
  if (mmm->should_trace) {
    int PC = mmm->mix.PC;
    word A = mmm->mix.A;
    word X = mmm->mix.X;
    word I1 = mmm->mix.I[0];
    word I2 = mmm->mix.I[1];
    word I3 = mmm->mix.I[2];
    word I4 = mmm->mix.I[3];
    word I5 = mmm->mix.I[4];
    word I6 = mmm->mix.I[5];
    word J = mmm->mix.J;

    int cmp = mmm->mix.cmp;
    char cmp_char;
    if (cmp == 0) cmp_char = '=';
    else if (cmp == 1) cmp_char = '>';
    else if (cmp == -1) cmp_char = '<';
    else assert(false && "unreachable");

    char overflow_char = mmm->mix.overflow ? 'O' : ' ';
    StrView mixal_line = mmm->line_infos[PC].mixal_line;

    sb_clear(&sb);

#define word_fmt "%c%04d%02d%02d%02d"
#define shortword_fmt "%c%02d%02d"
#define REPEAT7(s) s s s s s s s
#define WORD_ARGS(w) w.sign ? '+' : '-', w.bytes[0] * MIX_BYTE_SIZE + w.bytes[1], w.bytes[2], w.bytes[3], w.bytes[4]
#define SHORTWORD_ARGS(w) w.sign ? '+' : '-', w.bytes[3], w.bytes[4]
    sb_appendf(
      &sb,
      "%04d:%d " word_fmt " " word_fmt " " REPEAT7(shortword_fmt " ") "%c%c\t" sv_fmt,
      arena,
      PC, mmm->mix.trace_counts[PC],
      WORD_ARGS(A), WORD_ARGS(J),
      SHORTWORD_ARGS(I1), SHORTWORD_ARGS(I2), SHORTWORD_ARGS(I3), SHORTWORD_ARGS(I4), SHORTWORD_ARGS(I5), SHORTWORD_ARGS(I6), SHORTWORD_ARGS(J),
      cmp_char, overflow_char,
      sv_arg(mixal_line)
    );
#undef REPEAT7
#undef WORD_ARGS
#undef SHORTWORD_ARGS

    if (!write_all(mmm->trace_fd, sb.s, sb.len)) {
      my_eprintf(RED("Error while write(" sv_fmt ") -- %s\n"), sv_arg(mmm->trace_file), strerror(errno));
      mmm->should_trace = false;
    }
  }

  if (mmm->mix.err) {
    my_eprintf(RED("Emulator error at %d: %s\n"), mmm->mix.PC, mmm->mix.err);
    return false;
  }
  return true;
}

void analyze_command(StrView *sv, MMM *mmm) {
  unsigned int start;
  unsigned int end;
  if (!sv_parse_uint(sv, &start) ||
      !sv_match_char(sv, '-') ||
      !sv_parse_uint(sv, &end)) {
    my_eprintf(RED("Please provide a range <start>-<end>!\n"));
    return;
  }
  if (start >= 4000 || end >= 4000 || start > end) {
    my_eprintf(RED("Please make sure that 0 <= start < end < 4000!\n"));
    return;
  }
  analyze_program(start, end, mmm);
}

void view_command(StrView *sv, MMM *mmm) {
  Arena *arena = mmm->arena;
  static StrBuf sb = sb_empty_init;
  if (sb_is_empty(sb))
    sb = sb_alloc(1024, arena);

  sb_clear(&sb);

  StrView arg = sv_parse_word(sv);
  // Print all MIXAL source lines
  if (sv_is_empty(arg)) {
    for (int addr = 0; addr < 4000; addr++) {
      LineInfo info = mmm->line_infos[addr];
      if (info.is_mixal_line)
        sb_append_mixal_line_colored(&sb, addr, mmm, arena);
    }
  }

  // Print all constants
  else if (sv_match_char(&arg, 'c')) {
    for (int addr = 0; addr < 4000; addr++) {
      LineInfo info = mmm->line_infos[addr];
      if (info.is_con_or_alf) {
        sb_append_mixal_line_colored(&sb, addr, mmm, arena);
      }
    }
  }

  // Print symbol's memory word
  else if (sv_match_char(&arg, '.')) {
    word w;
    if (lookup_sym(arg, &w, &mmm->ps)) {
      int addr = word_to_int(w);
      sb_append_mixal_line_colored(&sb, addr, mmm, arena);
    }
    else {
      my_printf(BLUE("Unknown symbol " sv_fmt "\n"), sv_arg(arg));
      return;
    }
  }

  my_print(sb.s);
}

void breakpoint_command(StrView *sv, MMM *mmm) {
  Arena *arena = mmm->arena;
  static StrBuf sb = sb_empty_init;
  if (sb_is_empty(sb))
    sb = sb_alloc(1024, arena);

  int addr;
  unsigned int addr0;

  if (sv_parse_uint(sv, &addr0)) {
    if (addr0 >= 4000) {
      my_eprint(RED("Breakpoint must be between 0-4000\n"));
      return;
    }
    addr = (int) addr0;
  }
  else {
    StrView arg = sv_parse_word(sv);
    if (sv_match_char(&arg, '.')) {
      word w;
      if (lookup_sym(arg, &w, &mmm->ps))
        addr = word_to_int(w);
      else {
        my_printf(BLUE("Unknown symbol " sv_fmt "\n"), sv_arg(arg));
        return;
      }
    }
    else
      return;
  }

  do {
    if (!mmm_step(mmm))
      break;
  } while (mmm->mix.next_PC != addr);

  sb_clear(&sb);
  sb_append_mixal_line(&sb, addr, mmm, arena, NULL);
  my_print(sb.s);
}

void step_command(MMM *mmm) {
  Arena *arena = mmm->arena;
  static StrBuf sb = sb_empty_init;
  if (sb_is_empty(sb))
    sb = sb_alloc(1024, arena);

  if (mmm->mix.done) {
    if (mmm->mix.err)
      my_eprintf(RED("ERROR: %s; type l to reset\n"), mmm->mix.err);
    else
      my_printf(GREEN("Program has finished running with exit code %d; type l to reset\n"), mmm->mix.exit_code);
  }
  else {
    mmm_step(mmm);
    sb_clear(&sb);
    sb_append_mixal_line(&sb, mmm->mix.next_PC, mmm, arena, NULL);
  }

  my_print(sb.s);
}

void go_command(StrView *sv, MMM *mmm) {
  if (sv_match_char(sv, '+')) {
    mmm->should_trace = true;

    StrView tracefile = sv_parse_word(sv);
    if (sv_is_empty(tracefile))
      tracefile = sv_from_cstr("trace.out");
    int tracefd;
    if (!try_open(tracefile, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR, &tracefd, *mmm->arena))
      return;
    mmm->trace_file = tracefile;
    mmm->trace_fd = tracefd;
  }

  while (!mmm->mix.done)
    mmm_step(mmm);

  my_printf(GREEN("Program has finished running with exit code %d; type l to reset\n"), mmm->mix.exit_code);
  StrView *tracefile = &mmm->trace_file;
  int *tracefd = &mmm->trace_fd;
  if (mmm->should_trace) {
    my_printf(GREEN("Program trace written to " sv_fmt "\n"), sv_arg(*tracefile));
    if (close(*tracefd) == -1)
      my_eprintf("Error while close(" sv_fmt ") -- %s", sv_arg(*tracefile), strerror(errno));
    else {
      *tracefile = sv_empty();
      *tracefd = -1;
    }
  }
}

int main(int argc, char **argv) {
  Arena arena = arena_new(1 << 24);
  Arena tmp_arena = arena_new(1 << 24);
  MMM *mmm = arena_alloc(sizeof(*mmm), &arena);
  mmm_init(mmm, &arena, &tmp_arena);

  if (argc < 2) {
    my_eprint(RED("Please specify a file!\n"));
    return EXIT_FAILURE;
  }

  if (!load_mixal(sv_from_cstr(argv[1]), mmm))
    return EXIT_FAILURE;

  my_print(CYAN("MIX Management Module, by wyan (v1-alpha)\n"));
  my_print("Type h for help\n");

  while (true) {
    my_print(">> ");

    StrBuf line_buf = sb_empty();
    StrView line_view = sv_empty();
    read_stdin_line(&line_buf, &arena);

    if (sb_is_empty(line_buf))
      return EXIT_SUCCESS;
    line_view = sv_from_sb(line_buf);

    if (sv_match_char(&line_view, '\0')) { } // TODO: run previous command

    if (sv_match_char(&line_view, 'h')) help_command();
    else if (sv_match_char(&line_view, 'q')) { quit_command(mmm); return EXIT_SUCCESS; }
    else if (sv_match_char(&line_view, 'l')) { if (!reset_command(mmm)) return EXIT_FAILURE; }
    else if (sv_match_char(&line_view, 'c')) cpu_command(mmm);
    else if (sv_match_cstr(&line_view, "io")) io_command(mmm);
    else if (sv_match_char(&line_view, '+')) attach_command(&line_view, mmm);
    else if (sv_match_char(&line_view, '-')) detach_command(&line_view, mmm);
    else if (sv_match_char(&line_view, 'g')) go_command(&line_view, mmm);
    else if (sv_match_char(&line_view, 's')) step_command(mmm);
    else if (sv_match_char(&line_view, 'b')) breakpoint_command(&line_view, mmm);
    else if (sv_match_char(&line_view, 'v')) view_command(&line_view, mmm);
    else if (sv_match_char(&line_view, 't')) time_command(mmm);
    else if (sv_match_char(&line_view, 'a')) analyze_command(&line_view, mmm);
    else my_eprint(BLUE("Hold up, I don't understand that command\n"));
  }
}