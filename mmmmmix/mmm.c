#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include "emulator.h"
#include "assembler.h"
#include "analyzeprogram.h"
#include "mmm.h"

#define RED(s)    "\033[31m" s "\033[37m"
#define GREEN(s)  "\033[32m" s "\033[37m"
#define YELLOW(s) "\033[33m" s "\033[37m"
#define BLUE(s)   "\033[34m" s "\033[37m"
#define CYAN(s)   "\033[36m" s "\033[37m"
#define GRAY(s)   "\033[90m" s "\033[37m"
#define SHOW_CURSOR "\033[?25h"
#define HIDE_CURSOR "\033[?25l"

// Similar to printf, the following display functions return the
// *visual width* of the printed string.
// Thus a string like "\t" has width 8.

int display_short(word w) {
  printf("%c %02d %02d", w.sign ? '+' : '-', w.bytes[3], w.bytes[4]);
  return 1 + 2*3;
}

int display_word(word w) {
  printf("%c %02d %02d %02d %02d %02d", w.sign ? '+' : '-', w.bytes[0], w.bytes[1], w.bytes[2], w.bytes[3], w.bytes[4]);
  return 1 + 5*3;
}

// Display instruction in a format similar to display_word, but with
// first two bytes combined
int display_instr_raw(word w) {
  signmag sm_A = get_A(w);
  byte I = get_I(w);
  byte F = get_F(w);
  byte C = get_C(w);
  printf("%c %04ld %02d %02d %02d", sm_A.sign ? '+' : '-', sm_A.mag, I, F, C);
  return 1 + 5 + 3*3;
}

// Display the A,I(F) portion of the canonical representation of an
// instruction
int display_instr_fields(bool sign, int A, byte I, byte F, byte default_F) {
  int num_chars = 0;
  if (!sign) {
    num_chars++;
    putchar('-');
  }
  num_chars += printf("%d", A);
  if (I != 0) num_chars += printf(",%d", I);
  if (F != default_F) num_chars += printf("(%d)", F);
  return num_chars;
}

// Display the canonical representation of an instruction, or ??? if
// invalid instruction.
int display_instr_canonical(word w) {
  signmag sm_A = get_A(w);
  byte I = get_I(w);
  byte F = get_F(w);
  byte C = get_C(w);

#define PP(s,DEFAULTF) do { printf(s "\t"); return 8 + display_instr_fields(sm_A.sign, sm_A.mag, I, F, DEFAULTF); } while (0)
#define UNKNOWN()      do { printf("???"); return 3; } while (0)

  if (C == 0) PP("NOP", 0);
  else if (C == 1) PP("ADD", 5);
  else if (C == 2) PP("SUB", 5);
  else if (C == 3) PP("MUL", 5);
  else if (C == 4) PP("DIV", 5);
  else if (C == 5) {
    if (F == 0) PP("MUL", 0);
    else if (F == 1) PP("CHAR", 1);
    else if (F == 2) PP("HLT", 2);
    else UNKNOWN();
  }
  else if (C == 6) {
    if (F == 0) PP("SLA", 0);
    else if (F == 1) PP("SRA", 1);
    else if (F == 2) PP("SLAX", 2);
    else if (F == 3) PP("SRAX", 3);
    else if (F == 4) PP("SLC", 4);
    else if (F == 5) PP("SRC", 5);
    else UNKNOWN();
  }
  else if (C == 7) PP("MOVE", 1);
  else if (C == 8) PP("LDA", 5);
  else if (C == 9) PP("LD1", 5);
  else if (C == 10) PP("LD2", 5);
  else if (C == 11) PP("LD3", 5);
  else if (C == 12) PP("LD4", 5);
  else if (C == 13) PP("LD5", 5);
  else if (C == 14) PP("LD6", 5);
  else if (C == 15) PP("LDX", 5);
  else if (C == 16) PP("LDAN", 5);
  else if (C == 17) PP("LD1N", 5);
  else if (C == 18) PP("LD2N", 5);
  else if (C == 19) PP("LD3N", 5);
  else if (C == 20) PP("LD4N", 5);
  else if (C == 21) PP("LD5N", 5);
  else if (C == 22) PP("LD6N", 5);
  else if (C == 23) PP("LDXN", 5);
  else if (C == 24) PP("STA", 5);
  else if (C == 25) PP("ST1", 5);
  else if (C == 26) PP("ST2", 5);
  else if (C == 27) PP("ST3", 5);
  else if (C == 28) PP("ST4", 5);
  else if (C == 29) PP("ST5", 5);
  else if (C == 30) PP("ST6", 5);
  else if (C == 31) PP("STX", 5);
  else if (C == 32) PP("STJ", 2);
  else if (C == 33) PP("STZ", 5);
  else if (C == 34) PP("JBUS", 0);
  else if (C == 35) PP("IOC", 0);
  else if (C == 36) PP("IN", 0);
  else if (C == 37) PP("OUT", 0);
  else if (C == 38) PP("JRED", 0);
  else if (C == 39) {
    if (F == 0) PP("JMP", 0);
    else if (F == 1) PP("JSJ", 1);
    else if (F == 2) PP("JOV", 2);
    else if (F == 3) PP("JNOV", 3);
    else if (F == 4) PP("JL", 4);
    else if (F == 5) PP("JE", 5);
    else if (F == 6) PP("JG", 6);
    else if (F == 7) PP("JGE", 7);
    else if (F == 8) PP("JNE", 8);
    else if (F == 9) PP("JLE", 9);
    else UNKNOWN();
  }
  else if (C == 40) {
    if (F == 0) PP("JAN", 0);
    else if (F == 1) PP("JAZ", 1);
    else if (F == 2) PP("JAP", 2);
    else if (F == 3) PP("JANN", 3);
    else if (F == 4) PP("JANZ", 4);
    else if (F == 5) PP("JANP", 5);
    else UNKNOWN();
  }
  else if (C == 41) {
    if (F == 0) PP("J1N", 0);
    else if (F == 1) PP("J1Z", 1);
    else if (F == 2) PP("J1P", 2);
    else if (F == 3) PP("J1NN", 3);
    else if (F == 4) PP("J1NZ", 4);
    else if (F == 5) PP("J1NP", 5);
    else UNKNOWN();
  }
  else if (C == 42) {
    if (F == 0) PP("J2N", 0);
    else if (F == 1) PP("J1Z", 1);
    else if (F == 2) PP("J2P", 2);
    else if (F == 3) PP("J2NN", 3);
    else if (F == 4) PP("J2NZ", 4);
    else if (F == 5) PP("J2NP", 5);
    else UNKNOWN();
  }
  else if (C == 43) {
    if (F == 0) PP("J3N", 0);
    else if (F == 1) PP("J3Z", 1);
    else if (F == 2) PP("J3P", 2);
    else if (F == 3) PP("J3NN", 3);
    else if (F == 4) PP("J3NZ", 4);
    else if (F == 5) PP("J3NP", 5);
    else UNKNOWN();
  }
  else if (C == 44) {
    if (F == 0) PP("J4N", 0);
    else if (F == 1) PP("J4Z", 1);
    else if (F == 2) PP("J4P", 2);
    else if (F == 3) PP("J4NN", 3);
    else if (F == 4) PP("J4NZ", 4);
    else if (F == 5) PP("J4NP", 5);
    else UNKNOWN();
  }
  else if (C == 45) {
    if (F == 0) PP("J5N", 0);
    else if (F == 1) PP("J5Z", 1);
    else if (F == 2) PP("J5P", 2);
    else if (F == 3) PP("J5NN", 3);
    else if (F == 4) PP("J5NZ", 4);
    else if (F == 5) PP("J5NP", 5);
    else UNKNOWN();
  }
  else if (C == 46) {
    if (F == 0) PP("J6N", 0);
    else if (F == 1) PP("J6Z", 1);
    else if (F == 2) PP("J6P", 2);
    else if (F == 3) PP("J6NN", 3);
    else if (F == 4) PP("J6NZ", 4);
    else if (F == 5) PP("J6NP", 5);
    else UNKNOWN();
  }
  else if (C == 47) {
    if (F == 0) PP("JXN", 0);
    else if (F == 1) PP("JXZ", 1);
    else if (F == 2) PP("JXP", 2);
    else if (F == 3) PP("JXNN", 3);
    else if (F == 4) PP("JXNZ", 4);
    else if (F == 5) PP("JXNP", 5);
    else UNKNOWN();
  }
  else if (C == 48) {
    if (F == 0) PP("INCA", 0);
    else if (F == 1) PP("DECA", 1);
    else if (F == 2) PP("ENTA", 2);
    else if (F == 3) PP("ENNA", 3);
    else UNKNOWN();
  }
  else if (C == 49) {
    if (F == 0) PP("INC1", 0);
    else if (F == 1) PP("DEC1", 1);
    else if (F == 2) PP("ENT1", 2);
    else if (F == 3) PP("ENN1", 3);
    else UNKNOWN();
  }
  else if (C == 50) {
    if (F == 0) PP("INC2", 0);
    else if (F == 1) PP("DEC2", 1);
    else if (F == 2) PP("ENT2", 2);
    else if (F == 3) PP("ENN2", 3);
    else UNKNOWN();
  }
  else if (C == 51) {
    if (F == 0) PP("INC3", 0);
    else if (F == 1) PP("DEC3", 1);
    else if (F == 2) PP("ENT3", 2);
    else if (F == 3) PP("ENN3", 3);
    else UNKNOWN();
  }
  else if (C == 52) {
    if (F == 0) PP("INC4", 0);
    else if (F == 1) PP("DEC4", 1);
    else if (F == 2) PP("ENT4", 2);
    else if (F == 3) PP("ENN4", 3);
    else UNKNOWN();
  }
  else if (C == 53) {
    if (F == 0) PP("INC5", 0);
    else if (F == 1) PP("DEC5", 1);
    else if (F == 2) PP("ENT5", 2);
    else if (F == 3) PP("ENN5", 3);
    else UNKNOWN();
  }
  else if (C == 54) {
    if (F == 0) PP("INC6", 0);
    else if (F == 1) PP("DEC6", 1);
    else if (F == 2) PP("ENT6", 2);
    else if (F == 3) PP("ENN6", 3);
    else UNKNOWN();
  }
  else if (C == 55) {
    if (F == 0) PP("INCX", 0);
    else if (F == 1) PP("DECX", 1);
    else if (F == 2) PP("ENTX", 2);
    else if (F == 3) PP("ENNX", 3);
    else UNKNOWN();
  }
  else if (C == 56) PP("CMPA", 5);
  else if (C == 57) PP("CMP1", 5);
  else if (C == 58) PP("CMP2", 5);
  else if (C == 59) PP("CMP3", 5);
  else if (C == 60) PP("CMP4", 5);
  else if (C == 61) PP("CMP5", 5);
  else if (C == 62) PP("CMP6", 5);
  else if (C == 63) PP("CMPX", 5);
  else UNKNOWN();
}

#undef PP
#undef UNKNOWN

// Display the MIXAL source resulting in that instruction.
// This differs from the canonical representation in that there may be
// user-defined constants.
// If MIXAL source is not available, fall back to the canonical representation.
void display_instr_mixal(int i, MMM *mmm) {
  if (i < 0 || i >= 4000) {
    printf(RED("Invalid memory address %04d\n"), i);
    return;
  }
  if (!mmm->extra_parse_infos[i].is_source_line) {
    putchar('\t');
    display_instr_canonical(mmm->mix.mem[i]);
  }
  else printf(mmm->source_lines[i]);
}

// Display the instruction as a complete line, for debugging purposes.
// The following format is used:
// LINE_NUM:EXEC_COUNT +- AAAA I F C            MIXAL or canonical
void display_instr_debug(int i, MMM *mmm) {
  int exec_count = mmm->mix.exec_counts[i];
  if (mmm->mix.is_overwritten[i]) {
    printf("\033[31m%04d:%d ", i, exec_count+1);
    display_instr_raw(mmm->mix.mem[i]);
    putchar('\t');
    display_instr_mixal(i, mmm);
    printf("\033[37m\n");
  }
  else {
    printf(BLUE("%04d:%d "), i, exec_count+1);
    printf("\033[33m");
    display_instr_raw(mmm->mix.mem[i]);
    printf("\033[37m\t");
    display_instr_mixal(i, mmm);
    putchar('\n');
  }
}

// Display the memory address as a complete line, with lots of information.
// The following format is used:
// LINE_NUM +- AAAA I F C  +- B1 B2 B3 B4 B5  (integer value)           MIXAL or canonical
void display_addr_verbose(int i, MMM *mmm) {
  if (i < 0 || i >= 4000) {
    printf(RED("Invalid memory address %04d\n"), i);
    return;
  }
  // Display currently running instruction in green,
  // and is_overwritten instructions in red.
  if (i == mmm->mix.PC || mmm->mix.is_overwritten[i]) {
    if (i == mmm->mix.PC) printf("\033[32m");
    else if (mmm->mix.is_overwritten[i]) printf("\033[31m");
    printf("%04d ", i);
    display_instr_raw(mmm->mix.mem[i]);
    printf("  ");
    display_word(mmm->mix.mem[i]);
    printf("  ");
    // If the number of chars to display the integer value of memory
    // contents exceeds a certain amount
    if (word_to_int(mmm->mix.mem[i]) >= 100000 || word_to_int(mmm->mix.mem[i]) <= -10000)
      printf("(%ld)\t", word_to_int(mmm->mix.mem[i]));
    else
      printf("(%ld)\t\t", word_to_int(mmm->mix.mem[i]));
    display_instr_mixal(i, mmm);
    printf("\033[37m\n");
  }
  // Display line in normal formatting
  else {
    printf(BLUE("%04d "), i);
    printf("\033[33m");
    display_instr_raw(mmm->mix.mem[i]);
    printf("  ");
    display_word(mmm->mix.mem[i]);
    printf("\033[37m  ");
    if (word_to_int(mmm->mix.mem[i]) >= 100000 || (int)word_to_int(mmm->mix.mem[i]) <= -10000)
      printf(CYAN("(%ld)\t"), word_to_int(mmm->mix.mem[i]));
    else
      printf(CYAN("(%ld)\t\t"), word_to_int(mmm->mix.mem[i]));
    display_instr_mixal(i, mmm);
    putchar('\n');
  }
}

void state_command(MMM *mmm) {
  printf(" A: ");
  printf("\033[33m"); display_word(mmm->mix.A); printf("\033[37m");
  printf("\t");
  printf(CYAN("(%ld)"), word_to_int(mmm->mix.A));

  printf("\n X: ");
  printf("\033[33m"); display_word(mmm->mix.X); printf("\033[37m");
  printf("\t");
  printf(CYAN("(%ld)"), word_to_int(mmm->mix.X));

  printf("\nI1: ");
  printf("\033[33m"); display_short(mmm->mix.I[0]); printf("\033[37m");
  printf("\t"); printf(CYAN("(%ld)"), word_to_int(mmm->mix.I[0]));

  printf("\nI2: ");
  printf("\033[33m"); display_short(mmm->mix.I[1]); printf("\033[37m");
  printf("\t"); printf(CYAN("(%ld)"), word_to_int(mmm->mix.I[1]));

  printf("\nI3: ");
  printf("\033[33m"); display_short(mmm->mix.I[2]); printf("\033[37m");
  printf("\t"); printf(CYAN("(%ld)"), word_to_int(mmm->mix.I[2]));

  printf("\nI4: ");
  printf("\033[33m"); display_short(mmm->mix.I[3]); printf("\033[37m");
  printf("\t"); printf(CYAN("(%ld)"), word_to_int(mmm->mix.I[3]));

  printf("\nI5: ");
  printf("\033[33m"); display_short(mmm->mix.I[4]); printf("\033[37m");
  printf("\t"); printf(CYAN("(%ld)"), word_to_int(mmm->mix.I[4]));

  printf("\nI6: ");
  printf("\033[33m"); display_short(mmm->mix.I[5]); printf("\033[37m");
  printf("\t"); printf(CYAN("(%ld)"), word_to_int(mmm->mix.I[5]));

  printf("\n J: ");
  printf("\033[33m"); display_short(mmm->mix.J); printf("\033[37m");
  printf("\t"); printf(CYAN("(%ld)"), word_to_int(mmm->mix.J));

  printf("\n\nOverflow: ");
  if (mmm->mix.overflow) printf(CYAN("ON\n"));
  else printf(CYAN("OFF\n"));
  printf("Comparison: ");
  if (mmm->mix.cmp == -1) printf(CYAN("<"));
  else if (mmm->mix.cmp == 1) printf(CYAN(">"));
  else printf(CYAN("="));

  printf("\n\nBusy IO devices:\n");
  for (int i = 0; i < 21; i++) {
    IOTask iotask = mmm->mix.iotasks[i];
    if (iotask.timer == 0) continue;
    if (iotask.C == 35)      printf(GREEN("%d") CYAN("  IOC") "  (%du left)\n", i, iotask.timer);
    else if (iotask.C == 36) printf(GREEN("%d") CYAN("  IN ") "  (%du left), address = %04d\n", i, iotask.timer, iotask.M);
    else if (iotask.C == 37) printf(GREEN("%d") CYAN("  OUT") "  (%du left), address = %04d\n", i, iotask.timer, iotask.M);
  }

  printf("\nCur instruction:\n");
  display_instr_debug(mmm->mix.PC, mmm);
}

int numdigits(int x) {
  if (x == 0) return 1;
  int i = 0;
  while ((x = x/10) > 0)
    i++;
  return i;
}

void time_command(MMM *mmm) {
  int totaltime = 0;
  int maxdigits = 0;
  for (int i = 0; i < 4000; i++) {
    totaltime += mmm->mix.exec_cycles[i];
    int nd = numdigits(mmm->mix.exec_counts[i]);
    if (nd > maxdigits) maxdigits = nd;
  }
  printf("Time taken: %du\n\n", totaltime);
  printf(CYAN("BREAKDOWN\n"));
  for (int i = 0; i < 4000; i++) {
    int count = mmm->mix.exec_counts[i];
    int num_chars;
    if (count == 1)
      num_chars = printf(BLUE("%04d ") YELLOW("%*c%d  time, %du"), i,
                        maxdigits-numdigits(count)+1, ' ', count, mmm->mix.exec_cycles[i]);
    else if (count > 1)
      num_chars = printf(BLUE("%04d ") YELLOW("%*c%d times, %du"), i,
                        maxdigits-numdigits(count)+1, ' ', count, mmm->mix.exec_cycles[i]);

    // Compensate for the \033 sequences using up 20 characters, since
    // they don't move the cursor right
    num_chars -= 20;

    if (count >= 1) {
      if (num_chars < 24) printf("\t\t");
      else if (num_chars < 32) printf("\t");
      display_instr_mixal(i, mmm);
      putchar('\n');
    }
  }
}

bool mix_step_wrapper(int trace_max, MMM *mmm) {
  int exec_count = mmm->mix.exec_counts[mmm->mix.PC];
  if (exec_count < trace_max) {
    if (!mmm->should_trace) {
      printf("----------------------------------------------------------------------------------------------\n");
      mmm->should_trace = true;
    }
    display_instr_debug(mmm->mix.PC, mmm);
  }
  else {
    mmm->should_trace = false;
  }
  mix_step(&mmm->mix);
  if (mmm->mix.err) {
    printf(RED("Emulator stopped at %d: %s\n"), mmm->mix.PC, mmm->mix.err);
    return false;
  }
  return true;
}

bool get_sym_value(char *s, MMM *mmm, word *w) {
  // Find the symbol in parse state
  for (int i = 0; i < ASSEMBLER_MAXSYMS; i++) {
    if (strncmp(s, mmm->ps.syms[i], 11) == 0) {
      *w = mmm->ps.sym_vals[i];
      return true;
    }
  }
  return false;
}

// Parse an expression of the form "<number>-<number>".
// A single "<number>" will also parse fine as a range with one
// element.
void parse_range(char *arg, int *from, int *to) {
  char *fromstart = arg, *tostart;
  while (!isspace(*arg) && *arg != '-' && *arg != '\0')
    arg++;
  if (*arg == '-') {
    *(arg++) = '\0';
    tostart = arg;
    while (!isspace(*arg) && *arg != '\0')
      arg++;
    *arg = '\0';
    *from = atoi(fromstart);
    *to = atoi(tostart);
  }
  else {
    *arg = '\0';
    *from = atoi(fromstart);
    *to = *from;
  }
}

void analyze_command(char *arg, MMM *mmm) {
  if (arg[0] == '\0') {
    printf(RED("Please provide a range <start>-<end>!\n"));
    return;
  }
  if (!isdigit(arg[0])) {
    printf(RED("Please provide a range <start>-<end>!\n"));
    return;
  }
  int startaddr, endaddr;
  parse_range(arg, &startaddr, &endaddr);
  if (startaddr < 0 || startaddr > endaddr || endaddr >= 4000) {
    printf(RED("Please make sure that 0<=start<end<4000!\n"));
    return;
  }
  analyzeprogram(mmm, startaddr, endaddr);
}

void view_command(char *arg, MMM *mmm) {
  // Print all nonzero memory addresses
  if (arg[0] == '\0') {
    for (int i = 0; i < 4000; i++) {
      if (!word_eq(mmm->mix.mem[i], pos_word(0)))
        display_addr_verbose(i, mmm);
    }
  }
  // Print addresses in a range
  else if (isdigit(arg[0])) {
    int from, to;
    parse_range(arg, &from, &to);
    if (from < 0 || from > to || from >= 4000) {
      printf(RED("Please make sure that 0<=start<end<4000!\n"));
      return;
    }
    for (int i = from; i <= to; i++)
      display_addr_verbose(i, mmm);
  }
  // Print address corresponding to the given symbol
  else if (arg[0] == '.') {
    word w;
    if (get_sym_value(arg+1, mmm, &w))
      display_addr_verbose(word_to_int(w), mmm);
    else
      printf(BLUE("I'm not aware of the symbol %s\n"), arg);
  }
}

void breakpoint_command(char *arg, MMM *mmm) {
  int bp = -1;
  if (isdigit(arg[0])) bp = atoi(arg);
  else if (arg[0] == '.') {
    word w;
    if (!get_sym_value(arg+1, mmm, &w)) {
      printf(BLUE("I'm not aware of the symbol %s\n"), arg);
      return;
    }
    bp = word_to_int(w);
  }

  if (bp < 0 || bp >= 4000) {
    printf(RED("Breakpoint must be between 0-4000\n"));
    return;
  }
  do {
    if (!mix_step_wrapper(0, mmm)) break;
  } while (mmm->mix.PC != bp);
  display_instr_debug(bp, mmm);
}

void step_command(MMM *mmm) {
  if (mmm->mix.done) {
    if (mmm->mix.err)
      printf(RED("ERROR: %s; type l to reset\n"), mmm->mix.err);
    else
      printf(GREEN("Program has finished running with exit code %d; type l to reset\n"), mmm->mix.exit_code);
  }
  else {
    mix_step_wrapper(0, mmm);
    display_instr_debug(mmm->mix.next_PC, mmm);
  }
}

void go_command(char *arg, MMM *mmm) {
  int trace_max;
  if (isdigit(arg[0]))
    trace_max = arg[0] - '0';
  else if (arg[0] == '+')
    trace_max = 2147483647;
  else
    trace_max = 0;
  mmm->should_trace = true;
  while (!mmm->mix.done)
    mix_step_wrapper(trace_max, mmm);
  printf(GREEN("Program has finished running with exit code %d; type l to reset\n"), mmm->mix.exit_code);
}

bool load_card_reader_file(char *filename, MMM *mmm) {
  if (filename[0] == '\0') return true;
  FILE *fp;
  if ((fp = fopen(filename, "r")) == NULL)
    return false;
  if (mmm->mix.card_reader_file)
    fclose(mmm->mix.card_reader_file);
  mmm->mix.card_reader_file = fp;
  return true;
}

bool load_card_punch_file(char *filename, MMM *mmm) {
  if (filename[0] == '\0') return true;
  FILE *fp;
  if ((fp = fopen(filename, "w")) == NULL)
    return false;
  if (mmm->mix.card_punch_file)
    fclose(mmm->mix.card_punch_file);
  mmm->mix.card_punch_file = fp;
  return true;
}

bool load_tape_file(char *filename, int n, MMM *mmm) {
  if (filename[0] == '\0') return true;
  if (n < 0 || n > 7) {
    printf(RED("Invalid tape number %d\n"), n);
    return false;
  }
  FILE *fp;
  if ((fp = fopen(filename, "r+")) == NULL) {
    printf(RED("Could not open tape file %s\n"), filename);
    return false;
  }
  printf(GREEN("Loaded tape file %s\n"), filename);
  if (mmm->mix.tape_files[n])
    fclose(mmm->mix.tape_files[n]);
  mmm->mix.tape_files[n] = fp;
  return true;
}

bool load_mixal_file(char *filename, MMM *mmm) {
  mix_init(&mmm->mix);
  parse_state_init(&mmm->ps);

  FILE *fp;
  if ((fp = fopen(filename, "r")) == NULL) {
    printf(RED("Could not open MIXAL file %s\n"), filename);
    return false;
  }
  int line_num = 0;
  char line[MMM_LINE_LEN];
  ExtraParseInfo extra;
  while (++line_num && fgets(line, MMM_LINE_LEN, fp) != NULL) {
    if (!parse_line(line, &mmm->ps, &mmm->mix, &extra)) {
      printf(RED("Assembler error at line %d: %s"), line_num, line);
      mix_init(&mmm->mix);
      return false;
    }
    if (extra.is_source_line) {
      strncpy(mmm->source_lines[mmm->ps.star-1], line, MMM_LINE_LEN);
      int len = strlen(line);
      mmm->source_lines[mmm->ps.star-1][len-1] = '\0';
      mmm->extra_parse_infos[mmm->ps.star-1] = extra;
    }
    if (extra.is_end) break;
  }
  printf(GREEN("Loaded MIXAL file %s\n"), filename);
  return true;
}

static unsigned char mix_chr_ascii(byte b) {
  unsigned char extra;
  char c = mix_chr(b, &extra);
  if (extra == 0x94)
    return '!';
  else if (extra == 0xa3)
    return '[';
  else if (extra == 0xa4)
    return ']';
  assert(false && "unreachable");
}

void export_command(MMM *mmm) {
  // Find the last memory cell that is not +0
  int program_end = 0;
  for (int i = 0; i < 4000; i++) {
    if (!word_eq(mmm->mix.mem[i], pos_word(0)))
      program_end = i;
  }
  // "Snap" program_end to the end of card
  // e.g. 0-15 -> 15, 16-31 -> 31
  program_end += 15 - program_end%16;
  // If the program extends to cell 3400 (which would be the end of
  // the 218th card), then it won't fit when loaded as cards into
  // mixsim.mixal. (Cells 3400-3999 are reserved for the simulator
  // logic.)
  if (program_end >= 3399) {
    printf(RED("The program goes beyond cell 3500. It cannot be loaded as cards into mixsim.mixal.\n"));
    return;
  }

  FILE *fp;
  if ((fp = fopen("program.cards", "w")) == NULL)
    printf(RED("Could not create file program.cards\n"));

  for (int i = 0; i <= program_end; i++) {
    word w = mmm->mix.mem[i];
    if (!w.sign) {
      printf(RED("Negative words cannot be loaded into cards\n"));
      return;
    }
    printf("%d/%d: %d %d %d %d %d\n", i,program_end, w.bytes[0], w.bytes[1], w.bytes[2], w.bytes[3], w.bytes[4]);
    fputc(mix_chr_ascii(w.bytes[0]), fp);
    fputc(mix_chr_ascii(w.bytes[1]), fp);
    fputc(mix_chr_ascii(w.bytes[2]), fp);
    fputc(mix_chr_ascii(w.bytes[3]), fp);
    fputc(mix_chr_ascii(w.bytes[4]), fp);
    if (i%16 == 15)  // End of card
      fputc('\n', fp);
  }

  // End-of-program card
  fputs(".....", fp);
  for (int i = 0; i < 75; i++)
    fputc(' ', fp);

  printf(GREEN("Saved program into program.cards.\n"));
}

void help_command() {
  printf(
    CYAN("COMMANDS:\n")
    "<empty>\t\trepeat previous command\n"
    "l\t\treload MIXAL, card and tape files\n"
    "@<file>\t\tuse card file\n"
    "#<n><file>\tuse tape file\n"
    "s\t\trun one step\n"
    "b<addr>\t\trun till address\n"
    "b.<sym>\t\trun till address at symbol\n"
    "g\t\trun whole program\n"
    "g<digit>\ttrace first few executions of each address\n"
    "g+\t\ttrace everything\n"
    "v\t\tview nonempty memory addresses\n"
    "v<range>\tview memory addresses\n"
    "v.<sym>\t\tview memory address at symbol\n"
    "r\t\tview internal state\n"
    "t\t\tprint timing statistics\n"
    "a<range>\tanalyze program\n"
    "C\t\texport program into cards\n"
    "d\t\tdashboard\n"
    "h\t\tprint this help\n"
    "q\t\tquit\n"
    "\n"
    "<range> is either <addr>-<addr> or <addr>\n"
  );
}

void quit_command(MMM *mmm) {
  if (mmm->mix.card_reader_file)
    fclose(mmm->mix.card_reader_file);
  if (mmm->mix.card_punch_file)
    fclose(mmm->mix.card_punch_file);
  for (int i = 0; i < 8; i++) {
    if (mmm->mix.tape_files[i])
      fclose(mmm->mix.tape_files[i]);
  }
}

void mmm_init(MMM *mmm) {
  // mmm->mix will be initialized in load_mixal_file()
  mmm->card_reader_file[0] = '\0';
  for (int i = 0; i < 8; i++)
    mmm->tape_files[i][0] = '\0';
  mmm->prev_line[0] = '\0';
  for (int i = 0; i < 4000; i++) {
    mmm->extra_parse_infos[i].is_source_line = false;
    mmm->extra_parse_infos[i].is_con_or_alf = false;
  }
  mmm->should_trace = true;
  mmm->dashboard_run_continuously = false;
  mmm->dashboard_buf = malloc(MMM_DASHBOARD_BUFSIZE);

  // Default IO operation times
  mmm->mix.IN_times[CARD_READER] = 10000;
  mmm->mix.IOC_times[LINE_PRINTER] = 10000;
  mmm->mix.OUT_times[LINE_PRINTER] = 7500;
  mmm->mix.OUT_times[CARD_PUNCH] = 10000;
  // I chose a value arbitrarily for tape IO because I haven't seen an
  // official figure yet
  for (int i = 0; i < 8; i++) {
    mmm->mix.IN_times[i] = 30000;
    mmm->mix.OUT_times[i] = 30000;
    mmm->mix.IOC_times[i] = 30000;
  }
}

bool reload_command(MMM *mmm) {
  if (!load_mixal_file(mmm->mixal_file, mmm)) {
    printf(RED("Could not reload %s"), mmm->mixal_file);
    return false;
  }
  load_card_reader_file(mmm->card_reader_file, mmm);
  load_card_punch_file(mmm->card_punch_file, mmm);
  for (int i = 0; i < 8; i++) {
    if (*mmm->tape_files[i] != '\0')
      load_tape_file(mmm->tape_files[i], i, mmm);
  }
  return true;
}

void load_card_reader_command(char *arg, MMM *mmm) {
  if (!load_card_reader_file(arg, mmm))
    printf(RED("Could not open card reader file %s\n"), arg);
  else {
    printf(GREEN("Loaded card reader file %s\n"), arg);
    strncpy(mmm->card_reader_file, arg, MMM_LINE_LEN);
  }
}

void load_card_punch_command(char *arg, MMM *mmm) {
  if (!load_card_punch_file(arg, mmm))
    printf(RED("Could not open card punch file %s\n"), arg);
  else {
    printf(GREEN("Loaded card punch file %s\n"), arg);
    strncpy(mmm->card_punch_file, arg, MMM_LINE_LEN);
  }
}

void load_tape_command(char *arg, MMM *mmm) {
  if (*arg == '\0')
    printf(RED("You have to provide a tape number!\n"));
  else {
    int n = *arg - '0';
    if (load_tape_file(arg+1, n, mmm))
      strncpy(mmm->tape_files[n], arg+1, MMM_LINE_LEN);
  }
}

static int dashboard_display_word(word w, char *buf) {
  return sprintf(buf, "%c %02d %02d %02d %02d %02d",
		 w.sign ? '+' : '-', w.bytes[0], w.bytes[1], w.bytes[2], w.bytes[3], w.bytes[4]);
}

static int dashboard_display_value_padded(word w, char *buf) {
  char *old_buf = buf;
  int x = sprintf(buf, "(%ld)", word_to_int(w));
  buf += x;
  buf += sprintf(buf, "%*c", 20 - x, ' ');
  return buf - old_buf;
}

static int dashboard_display_register(char *name, word w, char *buf, bool is_in_operand, bool is_out_operand) {
  // Example format:
  // rA  + 00 00 00 00 00  (0)
  // with trailing spaces for the next register on same line
  char *old_buf = buf;

  // To make alignment possible, ensure the same number of characters is printed regardless of the color

  if (is_out_operand) buf += sprintf(buf, "\033[33m");
  else if (is_in_operand) buf += sprintf(buf, "\033[34m");
  else buf += sprintf(buf, "\033[90m");

  buf += sprintf(buf, "%s ", name);

  if (is_out_operand) buf += sprintf(buf, "\033[33m");
  else if (is_in_operand) buf += sprintf(buf, "\033[34m");
  else buf += sprintf(buf, "\033[37m");

  buf += dashboard_display_word(w, buf);

  if (is_out_operand) buf += sprintf(buf, "\033[33m");
  else if (is_in_operand) buf += sprintf(buf, "\033[34m");
  else buf += sprintf(buf, "\033[36m");

  buf += sprintf(buf, "  ");
  buf += dashboard_display_value_padded(w, buf);
  buf += sprintf(buf, "\033[37m");

  return buf - old_buf;
}

static void get_instr_operands(Mix *mix, word instr, int *in_r1, int *in_r2, int *in_addr, int *out_r1, int *out_r2, int *out_addr) {
  *in_r1 = *in_r2 = *out_r1 = *out_r2 = -1;
  // So we can test conditions like
  // in_addr <= A <= in_addr + F
  // when deciding whether to color a character in the memory map.
  *in_addr = -4000; *out_addr = -4000;

  byte C = get_C(instr);
  byte F = get_F(instr);
  int M = get_M(instr, mix);
  if (C == ADD || C == SUB) { *in_r1 = 0; *in_addr = M; *out_r1 = 0; }
  else if (C == MUL) { *in_r1 = 0; *in_addr = M; *out_r1 = 0; *out_r2 = 7; }
  else if (C == DIV) { *in_r1 = 0; *in_r2 = 7; *in_addr = M; *out_r1 = 0; *out_r2 = 7; }
  else if (C == SPEC && F == 0) { *in_r1 = 0; *in_r2 = 7; *out_r1 = 0; }  // NUM
  else if (C == SPEC && F == 1) { *in_r1 = 0; *out_r1 = 0; *out_r2 = 7; } // CHAR
  else if (C == SHIFT && (F == 0 || F == 1)) { *in_r1 = 0; *out_r1 = 0; } // SLA,SRA
  else if (C == SHIFT && F >= 2) { *in_r1 = 0; *in_r2 = 7; *out_r1 = 0; *out_r2 = 7; } // SLAX,SRAX,SLC,SRC
  else if (C == MOVE) { *in_addr = M; *out_r1 = 1; *out_addr = word_to_int(mix->I[0]); } // Special case, all the moved words are to be highlighted in the map
  else if (C >= LDA && C <= LDX) { *in_addr = M; *out_r1 = C - LDA; }
  else if (C >= LDAN && C <= LDXN) { *in_addr = M; *out_r1 = C - LDAN; }
  else if (C >= STA && C <= STX) { *in_r1 = C - STA; *out_addr = M; }
  else if (C == STJ) { *in_r1 = 8; *out_addr = M; }
  else if (C == STZ) { *out_addr = M; }
  else if (C == IN) { *out_addr = M; } // Special case like MOVE
  else if (C == OUT) { *in_addr = M; } // Special case like MOVE
  else if (C >= JMPA && C <= JMPX) { *in_r1 = C - JMPA; *in_r2 = C - JMPA; }
  else if (C >= INCA && C <= INCX) { *in_r1 = C - INCA; *in_r2 = C - INCA; *out_r1 = C - INCA; *out_r2 = C - INCA; }
  else if (C >= CMPA && C <= CMPX) { *in_r1 = C - CMPA; *in_r2 = C - CMPA; *in_addr = M; }
}

void dashboard_command(MMM *mmm) {
  tcgetattr(STDIN_FILENO, &mmm->orig_termios);
  mmm->cbreak = mmm->orig_termios;
  mmm->cbreak.c_lflag &= ~(ECHO | ICANON);
  mmm->cbreak.c_cc[VMIN] = 1;
  mmm->cbreak.c_cc[VTIME] = 0;
  printf(HIDE_CURSOR);
  tcsetattr(STDIN_FILENO, TCSANOW, &mmm->cbreak);

  // Make stdin non-blocking
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, flags|O_NONBLOCK);

  ssize_t num_read = 0;
  char key_buf[10];
  char *buf;

  int in_r1, in_r2, in_addr, out_r1, out_r2, out_addr;

  while (1) {
    buf = mmm->dashboard_buf;
    buf += sprintf(buf, "\033[2J"); // clear screen
    buf += sprintf(buf, "\033[;H"); // move cursor to top left

    // Keypress bytes for debugging

    //for (int i = 0; i < (int)num_read; i++)
    //  buf += sprintf(buf, "%d ", key_buf[i]);
    //buf += sprintf(buf, "\033[E");

    // Display registers

    word instr = mmm->mix.mem[mmm->mix.PC];
    signmag A = get_A(instr);
    byte C = get_C(instr);
    byte F = get_F(instr);

    get_instr_operands(&mmm->mix, instr, &in_r1, &in_r2, &in_addr,
		       &out_r1, &out_r2, &out_addr);

    buf += dashboard_display_register("rA ", mmm->mix.A, buf, in_r1==0, out_r1==0);
    buf += dashboard_display_register("rX ", mmm->mix.X, buf, in_r2==7, out_r2==7);
    buf += dashboard_display_register("rI1", mmm->mix.I[0], buf, in_r1==1, out_r1==1);
    buf += sprintf(buf, "\033[E");
    buf += dashboard_display_register("rI2", mmm->mix.I[1], buf, in_r1==2, out_r1==2);
    buf += dashboard_display_register("rI3", mmm->mix.I[2], buf, in_r1==3, out_r1==3);
    buf += dashboard_display_register("rI4", mmm->mix.I[3], buf, in_r1==4, out_r1==4);
    buf += sprintf(buf, "\033[E");
    buf += dashboard_display_register("rI5", mmm->mix.I[4], buf, in_r1==5, out_r1==5);
    buf += dashboard_display_register("rI6", mmm->mix.I[5], buf, in_r1==6, out_r1==6);
    buf += dashboard_display_register("rJ ", mmm->mix.J, buf, in_r1==8, out_r1==8);
    buf += sprintf(buf, "\033[E");

    // Display status flags

    buf += sprintf(buf, "\033[1;133H");
    buf += sprintf(buf, "Flags: ");

    if (mmm->mix.overflow) buf += sprintf(buf, "OV ");
    else buf += sprintf(buf, GRAY("OV "));

    if (mmm->mix.cmp == -1) buf += sprintf(buf, "<");
    else if (mmm->mix.cmp == 1) buf += sprintf(buf, ">");
    else buf += sprintf(buf, "=");

    // Display devices

    buf += sprintf(buf, "\033[2;133H");
    buf += sprintf(buf, "Devices: ");
    for (int i = 0; i < 21; i++) {
      IOTask *iotask = mmm->mix.iotasks + i;
      if (!iotask->is_active) continue;
      if (iotask->C == 35)
	buf += sprintf(buf, GREEN("%d") " " CYAN("IOC") " (%du left), ", i, iotask->timer);
      else if (iotask->C == 36)
	buf += sprintf(buf, GREEN("%d") " " CYAN("IN ") " (%du left), addr = %04d, ", i, iotask->timer, iotask->M);
      else if (iotask->C == 37)
	buf += sprintf(buf, GREEN("%d") " " CYAN("OUT") " (%du left), addr = %04d, ", i, iotask->timer, iotask->M);
    }
    buf += sprintf(buf, "\033[2D ");

    // Display memory map

    buf += sprintf(buf, "\033[5;H");

    char cur_sym_label = 'A';
    char c;
    int color = 0;
    // Keep track of previous color so we don't send ANSI escape codes every char; very wasteful
    int prev_color = -1;
    bool underline;

    for (int i = 0; i < 4000; i++) {
      if (i == mmm->mix.next_PC) color = 3;
      else if (C == MOVE && i >= out_addr && i < out_addr + F) color = 4;
      else if (C == MOVE && i >= in_addr && i < in_addr + F) color = 5;
      else if (C != MOVE && i == out_addr) color = 4;
      else if (C != MOVE && i == in_addr) color = 5;
      else if (mmm->mix.is_overwritten[i]) color = 2;
      else color = -1; // Undefined color, will be defined below
      underline = false;

      ExtraParseInfo extra = mmm->extra_parse_infos[i];
      if (!extra.is_source_line) {
	c = '.';
	if (color == -1) color = 1;
      }
      else if (extra.is_con_or_alf) {
	c = '$';
	if (color == -1) color = 0;
      }
      else {
	c = '-';
	if (color == -1) color = 0;
      }

      if (underline) buf += sprintf(buf, "\033[4m");

      assert(color != -1);
      if (color == prev_color) buf += sprintf(buf, "%c", c);
      else if (color == 0) buf += sprintf(buf, "\033[37m%c", c);
      else if (color == 1) buf += sprintf(buf, "\033[90m%c", c);
      else if (color == 2) buf += sprintf(buf, "\033[31m%c", c);
      else if (color == 3) buf += sprintf(buf, "\033[32m%c", c);
      else if (color == 4) buf += sprintf(buf, "\033[33m%c", c);
      else if (color == 5) buf += sprintf(buf, "\033[34m%c", c);
      prev_color = color;

      if (underline) buf += sprintf(buf, "\033[24m");
    }
    buf += sprintf(buf, "\033[37m");

    // Display assembly code

    buf += sprintf(buf, "\033[E");
    buf += sprintf(buf, "\0337"); // save cursor position for printing constants later

    for (int i = mmm->mix.next_PC - 10; i < mmm->mix.next_PC + 10; i++) {
      buf += sprintf(buf, "\033[E");
      if (i < 0 || i >= 4000) {
	buf += sprintf(buf, "        ...");
	continue;
      }

      if (i == mmm->mix.next_PC) buf += sprintf(buf, GREEN("%4d\t"), i);
      else buf += sprintf(buf, GRAY("%4d\t"), i);

      if (i < 0 || i >= 4000 || !mmm->extra_parse_infos[i].is_source_line) {
	if (i == mmm->mix.next_PC) buf += sprintf(buf, GREEN("..."));
	else buf += sprintf(buf, "...");
      }
      else {
	if (i == mmm->mix.next_PC) buf += sprintf(buf, GREEN("%s"), mmm->source_lines[i]);
	else buf += sprintf(buf, "%s", mmm->source_lines[i]);
      }
    }

    // Display constants

    buf += sprintf(buf, "\0338"); // restore cursor position (beginning of first assembly line)
    for (int i = 0; i < 4000; i++) {
      if (!mmm->extra_parse_infos[i].is_con_or_alf) continue;

      buf += sprintf(buf, "\033[E\033[100C");

      word w = mmm->mix.mem[i];
      if (i == out_addr) buf += sprintf(buf, "\033[33m");
      else if (i == in_addr) buf += sprintf(buf, "\033[34m");
      else if (mmm->mix.is_overwritten[i]) buf += sprintf(buf, "\033[31m");
      else buf += sprintf(buf, "\033[90m");

      buf += sprintf(buf, "%4d\t", i);

      if (i != out_addr && i != in_addr && !mmm->mix.is_overwritten[i])
	buf += sprintf(buf, "\033[37m");
      buf += dashboard_display_word(w, buf);
      buf += sprintf(buf, "  ");

      if (i != out_addr && i != in_addr && !mmm->mix.is_overwritten[i])
	buf += sprintf(buf, "\033[36m");
      buf += dashboard_display_value_padded(w, buf);

      if (i != out_addr && i != in_addr && !mmm->mix.is_overwritten[i])
	buf += sprintf(buf, "\033[37m");
      buf += sprintf(buf, "%s", mmm->source_lines[i]);
    }

    // Flush buffer

    int n_remaining = buf - mmm->dashboard_buf;
    buf = mmm->dashboard_buf;
    while (1) {
      int n_written = (int) write(STDOUT_FILENO, buf, n_remaining);
      if (n_written == -1) {
	if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
	printf("\033[2J");
	printf("\033[;H");
	printf(RED("write() failed: %s\n"), strerror(errno));
	mmm->dashboard_run_continuously = false;
	break;
      }
      if (n_written == n_remaining) break;
      n_remaining -= n_written;
      buf += n_written;
    }

    if (mmm->dashboard_run_continuously)
      mix_step(&mmm->mix);
    if (mmm->mix.done)
      mmm->dashboard_run_continuously = false;

    // Read input
try_read:
    num_read = read(STDIN_FILENO, key_buf, 10);
    if (num_read == -1) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
	printf("\033[2J");
	printf("\033[;H");
	printf(RED("read() failed: %s\n"), strerror(errno));
      }
      if (!mmm->dashboard_run_continuously)
	goto try_read;
    }
    else if (num_read == 1 && key_buf[0] == 's') {
      if (!mmm->dashboard_run_continuously)
	mix_step(&mmm->mix);
    }
    else if (num_read == 1 && key_buf[0] == 'l') {
      reload_command(mmm);
      mmm->dashboard_run_continuously = false;
    }
    else if (num_read == 1 && key_buf[0] == 'S') {
      mmm->dashboard_run_continuously = !mmm->dashboard_run_continuously;
    }
    else if (num_read == 1 && key_buf[0] == 'q') {
      tcsetattr(STDIN_FILENO, TCSANOW, &mmm->orig_termios);
      printf(SHOW_CURSOR);
      printf("\033[37m");
      printf("\033[2J");
      printf("\033[;H");
      // Make stdin blocking again
      flags = fcntl(STDIN_FILENO, F_GETFL, 0);
      fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
      return;
    }
  }
}

int main(int argc, char **argv) {
  MMM mmm;
  mmm_init(&mmm);

  // Handle arguments
  if (argc < 2) {
    printf(RED("Please specify a filename!\n"));
    return 0;
  }
  if (!load_mixal_file(argv[1], &mmm))
    return 0;
  strncpy(mmm.mixal_file, argv[1], MMM_LINE_LEN);
  if (argc >= 3 && load_card_reader_file(argv[2], &mmm))
    strncpy(mmm.card_reader_file, argv[2], MMM_LINE_LEN);
  if (argc >= 4 && load_card_punch_file(argv[3], &mmm))
    strncpy(mmm.card_punch_file, argv[3], MMM_LINE_LEN);

  printf(CYAN("MIX Management Module, by wyan\n"));
  printf("Type h for help\n");

  char line[MMM_LINE_LEN+1];
  while (true) {
    printf(">> ");
    fgets(line, MMM_LINE_LEN, stdin);
    // Strip the last newline
    line[strnlen(line, MMM_LINE_LEN)-1] = '\0';
    if (feof(stdin))
      return 0;

    if (*line == '\0')     strncpy(line, mmm.prev_line, MMM_LINE_LEN); // Run previous command
    strncpy(mmm.prev_line, line, MMM_LINE_LEN);
    if (*line == 'l')      { if (!reload_command(&mmm)) return 1; }
    else if (*line == '@') load_card_reader_command(line+1, &mmm);
    else if (*line == '!') load_card_punch_command(line+1, &mmm);
    else if (*line == '#') load_tape_command(line+1, &mmm);
    else if (*line == 's') step_command(&mmm);
    else if (*line == 'b') breakpoint_command(line+1, &mmm);
    else if (*line == 'g') go_command(line+1, &mmm);
    else if (*line == 'v') view_command(line+1, &mmm);
    else if (*line == 'r') state_command(&mmm);
    else if (*line == 't') time_command(&mmm);
    else if (*line == 'a') analyze_command(line+1, &mmm);
    else if (*line == 'C') export_command(&mmm);
    else if (*line == 'd') dashboard_command(&mmm);
    else if (*line == 'h') help_command();
    else if (*line == 'q') { quit_command(&mmm); return 0; }
    else printf(BLUE("Hold up, I don't understand that command\n"));
  }
}