#ifndef _ASSEMBLER_H
#define _ASSEMBLER_H

#include <ctype.h>
#include <stdlib.h>
#include "emulator.h"
#include "mylib/str.h"
#include "mylib/arena.h"

// Stores information about a future reference or a literal constant
// to be filled in later.
typedef struct {
  int addr;
  bool is_resolved;
  bool is_literal;
  Str sym;
  word literal;
} FutureRef;

#define ASSEMBLER_MAXSYMS 1000
typedef struct {
  int star;
  int num_syms;
  int num_future_refs;
  Str syms[ASSEMBLER_MAXSYMS];
  word sym_vals[ASSEMBLER_MAXSYMS];
  FutureRef future_refs[ASSEMBLER_MAXSYMS];
  int local_sym_counts[10];  // The current number of instances of each local symbol <n>H.
  Arena *arena;
} ParseState;

// Returned by reference in parse_line() for mmm to use.
typedef struct {
  bool is_source_line;
  bool is_con_or_alf;
  bool is_end;
} ExtraParseInfo;

void parse_state_init(ParseState *ps, Arena *arena);

bool lookup_sym(char *sym, word *val, ParseState *ps);
bool parse_sym(char **s, char *sym);
bool parse_operator(char **s, byte *C, byte *F);
bool parse_num(char **s, int *val);
// parse_atomic() and the rest all write into a word instead of an int,
// because words and ints are NOT equivalent.
// Specifically, +0 != -0.
bool parse_atomic(char **s, word *val, ParseState *ps);
bool parse_expr(char **s, word *val, ParseState *ps);
bool parse_A(char **s, word *A, ParseState *ps);
bool parse_I(char **s, byte *I, ParseState *ps);
bool parse_F(char **s, byte *F, ParseState *ps);
bool parse_W(char **s, word *W, ParseState *ps);
bool parse_line(char *line, ParseState *ps, Mix *mix, ExtraParseInfo *extra);

#endif

#ifdef ASSEMBLER_IMPLEMENTATION

static char *mix_op_names[] = {
  "NOP", "ADD", "SUB", "MUL", "DIV", "NUM", "CHAR", "HLT",
  "XOR", "SLA", "SRA", "SLAX", "SRAX", "SLC", "SRC", "MOVE",
  "LDA", "LD1", "LD2", "LD3", "LD4", "LD5", "LD6", "LDX",
  "LDAN", "LD1N", "LD2N", "LD3N", "LD4N", "LD5N", "LD6N", "LDXN",
  "STA", "ST1", "ST2", "ST3", "ST4", "ST5", "ST6", "STX",
  "STJ", "STZ", "JBUS", "IOC", "IN", "OUT", "JRED", "JMP",
  "JSJ", "JOV", "JNOV", "JL", "JE", "JG", "JGE", "JNE", "JLE",
  "JAN", "JAZ", "JAP", "JANN", "JANZ", "JANP",
  "J1N", "J1Z", "J1P", "J1NN", "J1NZ", "J1NP",
  "J2N", "J2Z", "J2P", "J2NN", "J2NZ", "J2NP",
  "J3N", "J3Z", "J3P", "J3NN", "J3NZ", "J3NP",
  "J4N", "J4Z", "J4P", "J4NN", "J4NZ", "J4NP",
  "J5N", "J5Z", "J5P", "J5NN", "J5NZ", "J5NP",
  "J6N", "J6Z", "J6P", "J6NN", "J6NZ", "J6NP",
  "JXN", "JXZ", "JXP", "JXNN", "JXNZ", "JXNP",
  "INCA", "DECA", "ENTA", "ENNA", "INC1", "DEC1", "ENT1", "ENN1",
  "INC2", "DEC2", "ENT2", "ENN2", "INC3", "DEC3", "ENT3", "ENN3",
  "INC4", "DEC4", "ENT4", "ENN4", "INC5", "DEC5", "ENT5", "ENN5",
  "INC6", "DEC6", "ENT6", "ENN6", "INCX", "DECX", "ENTX", "ENNX",
  "CMPA", "CMP1", "CMP2", "CMP3", "CMP4", "CMP5", "CMP6", "CMPX",
};

static byte default_op_fields[] = {
  0, 5, 5, 5, 5, 0, 1, 2,
  5, 0, 1, 2, 3, 4, 5, 1,
  5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5,
  2, 5, 0, 0, 0, 0, 0, 0,
  1, 2, 3, 4, 5, 6, 7, 8, 9,
  0, 1, 2, 3, 4, 5,
  0, 1, 2, 3, 4, 5,
  0, 1, 2, 3, 4, 5,
  0, 1, 2, 3, 4, 5,
  0, 1, 2, 3, 4, 5,
  0, 1, 2, 3, 4, 5,
  0, 1, 2, 3, 4, 5,
  0, 1, 2, 3, 4, 5,
  0, 1, 2, 3, 0, 1, 2, 3,
  0, 1, 2, 3, 0, 1, 2, 3,
  0, 1, 2, 3, 0, 1, 2, 3,
  0, 1, 2, 3, 0, 1, 2, 3,
  5, 5, 5, 5, 5, 5, 5, 5,
};

static byte opcodes[] = {
   0,  1,  2,  3,  4,  5,  5,  5,
   5,  6,  6,  6,  6,  6,  6,  7,
   8,  9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23,
  24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39,
  39, 39, 39, 39, 39, 39, 39, 39, 39,
  40, 40, 40, 40, 40, 40,
  41, 41, 41, 41, 41, 41,
  42, 42, 42, 42, 42, 42,
  43, 43, 43, 43, 43, 43,
  44, 44, 44, 44, 44, 44,
  45, 45, 45, 45, 45, 45,
  46, 46, 46, 46, 46, 46,
  47, 47, 47, 47, 47, 47,
  48, 48, 48, 48, 49, 49, 49, 49,
  50, 50, 50, 50, 51, 51, 51, 51,
  52, 52, 52, 52, 53, 53, 53, 53,
  54, 54, 54, 54, 55, 55, 55, 55,
  56, 57, 58, 59, 60, 61, 62, 63,
};

void parse_state_init(ParseState *ps, Arena *arena) {
  ps->arena = arena;
  ps->star = 0;
  ps->num_syms = 0;
  ps->num_future_refs = 0;
  for (int i = 0; i < 10; i++)
    ps->local_sym_counts[i] = 0;
}

bool lookup_sym(char *sym, word *val, ParseState *ps) {
  for (int i = 0; i < ps->num_syms; i++) {
    if (!strcmp(ps->syms[i].s, sym)) {
      *val = ps->sym_vals[i];
      return true;
    }
  }
  return false;
}

static void add_sym(char *sym, word val, ParseState *ps) {
  Str s = str_new(ps->arena);
  str_append_cstr(&s, sym, ps->arena);
  ps->syms[ps->num_syms] = s;
  ps->sym_vals[ps->num_syms] = val;
  ps->num_syms++;
}

// A symbol is a string of one to ten letters and/or digits,
// containing at least one letter.
bool parse_sym(char **s, char *sym) {
  char *t = *s, *start = *s;
  int i = 0;
  char c;
  bool hasletter = false;
  while (i++, !isspace(c = *(t++))) {
    if (i > 10) goto err;
    if (isalnum(c)) *(sym++) = toupper(c);
    else break;
    if (isalpha(c)) hasletter = true;
  }
  if (!hasletter) goto err;
  *s = t-1;
  *sym = '\0';
  return true;
err:
  *s = start;
  return false;
}

bool parse_operator(char **s, byte *C, byte *F) {
  char *t = *s, *start = *s;
  int k = 0;
  char c;
  // A MIX operator has at most 5 characters
  while (k++, !isspace(c = *(t++)))
    if (k > 4)
      goto err;
  *s = t-1;
  k--;
  int num_ops = (int)(sizeof(mix_op_names) / sizeof(char *));
  for (int i = 0; i < num_ops; i++) {
    if (!strncmp(start, mix_op_names[i], k)) {
      *F = default_op_fields[i];
      *C = opcodes[i];
      return true;
    }
  }
err:
  *s = start;
  return false;
}

bool parse_num(char **s, int *val) {
  char *t = *s, *start = *s;
  int digits[10], *d = digits;
  int i = 0;
  char c;
  while (i++, isdigit(c = *(t++))) {
    if (i > 10) goto err;
    *(d++) = c-'0';
  }
  i--;
  *s = t-1;
  // Strings like "20BY20" should be parsed as symbols.
  if (isalpha(**s))
    goto err;

  *val = digits[0];
  for (int j = 1; j < i; j++)
    *val = 10 * (*val) + digits[j];
  return true;
err:
  *s = start;
  return false;
}

// parse_atomic() and the rest all write into a word instead of an int,
// because words and ints are NOT equivalent.
// Specifically, +0 != -0.

bool parse_atomic(char **s, word *val, ParseState *ps) {
  char *start = *s;
  int i;

  // Number?
  if (isdigit(**s) && parse_num(s, &i))
    *val = pos_word(i);

  // * refers to the current line
  else if (**s == '*') {
    (*s)++;
    *val = pos_word(ps->star);
  }

  // Treat it as a symbol
  else {
    char sym[11];
    if (!parse_sym(s, sym)) goto err;
    if (isdigit(sym[0]) && sym[1] == 'B' && sym[2] == '\0') {
      sym[1] = 'H';
      sym[2] = '#';
      int n = ps->local_sym_counts[sym[0]-'0'] - 1;
      sym[3] = '0' + n/10;
      sym[4] = '0' + n%10;
      sym[5] = '\0';
    }
    if (!lookup_sym(sym, val, ps)) goto err;
  }
  return true;
err:
  *s = start;
  return false;
}

bool parse_expr(char **s, word *val, ParseState *ps) {
  char *start = *s;
  word final = pos_word(0);
  word w;
  char prev_sign = '\0';
  if (**s == '+') {
    (*s)++;
    prev_sign = '+';
  }
  else if (**s == '-') {
    (*s)++;
    prev_sign = '-';
  }
start:
  if (!parse_atomic(s, &w, ps)) goto err;

  if (prev_sign == '\0') final = w;
  else if (prev_sign == '+') add_word(&final, w);
  else if (prev_sign == '-') sub_word(&final, w);
  else if (prev_sign == '*') {
    word v;
    mul_word(&final, &v, w);
    final = v;
  }
  else if (prev_sign == '/') {
    word v = pos_word(0);
    div_word(&v, &final, w);
    final = v;
  }
  else if (prev_sign == ':') {
    word v;
    mul_word(&final, &v, pos_word(8));
    add_word(&v, w);
    final = v;
  }
  else if (prev_sign == '|') {
    word v = pos_word(0);
    div_word(&final, &v, w);
  }

  if (**s == '+' || **s == '-' || **s == '*' || **s == ':' ||
      (**s == '/' && *(*s+1) != '/')) {
    prev_sign = **s;
    (*s)++;
    goto start;
  }
  else if (**s == '/' && *(*s+1) == '/') {
    prev_sign = '|';
    *s += 2;
    goto start;
  }
  *val = final;
  return true;
err:
  *s = start;
  return false;
}

bool parse_W(char **s, word *W, ParseState *ps);
bool parse_A(char **s, word *A, ParseState *ps) {
  char *start = *s;
  char sym[11];
  // Expression?
  if (parse_expr(s, A, ps)) return true;

  // Future reference?
  else if (parse_sym(s, sym)) {
    if (isdigit(sym[0]) && sym[1] == 'F' && sym[2] == '\0') {
      sym[1] = 'H';
      sym[2] = '#';
      int n = ps->local_sym_counts[sym[0]-'0'];
      sym[3] = '0' + n/10;
      sym[4] = '0' + n%10;
      sym[5] = '\0';
    }
    FutureRef fr;
    fr.is_resolved = false;
    fr.addr = ps->star;
    fr.is_literal = false;
    fr.sym = str_new(ps->arena);
    str_append_cstr(&fr.sym, sym, ps->arena);
    ps->future_refs[ps->num_future_refs++] = fr;
    *A = pos_word(0);  // The A-field will be filled in later.
    return true;
  }

  // Local constant?
  else if (**s == '=') {
    word w;
    (*s)++;
    if (!parse_W(s, &w, ps)) goto err;
    if (**s != '=') goto err;
    FutureRef fr;
    fr.is_resolved = false;
    fr.addr = ps->star;
    fr.is_literal = true;
    fr.literal = w;
    ps->future_refs[ps->num_future_refs++] = fr;
    *A = pos_word(0);  // The A-field will be filled in later.
    return true;
  }

  // There is no A-field (we will move on to the I or F field or the next line)
  else if (**s == '\t' || **s == ',' || **s == '(') {
    *A = pos_word(0);
    return true;
  }

err:
  *s = start;
  return false;
}

bool parse_I(char **s, byte *I, ParseState *ps) {
  // Don't skip spaces here; if there is whitespace before the comma,
  // we take it to be a comment, e.g.
  // LDA     0       ,comma
  word w;
  char sym[11];
  if (**s == ',') {
    if (!parse_expr((++*s, s), &w, ps)) return false;
    *I = w.bytes[4];
  }
  else
    *I = 0;
  return true;
}

bool parse_F(char **s, byte *F, ParseState *ps) {
  // Don't skip spaces here; if there is whitespace before the opening
  // bracket, we take it to be a comment, e.g.
  // LDA     0       (test comment)
  word w;
  char *start = *s;
  if (**s == '(') {
    (*s)++;
    if (!parse_expr(s, &w, ps) || **s != ')') return false;
    *F = w.bytes[4];
    (*s)++;
    return true;
  }
  *s = start;
  return true;
}

bool parse_W(char **s, word *W, ParseState *ps) {
  word w; byte F;
loop:
  F = 5;
  if (!parse_expr(s, &w, ps)) return false;
  if (**s == '(' && !parse_F(s, &F, ps)) return false;
  store_word(W, w, F);
  if (**s == ',') {
    (*s)++;
    goto loop;
  }
  return true;
}

bool parse_ALF(char **s, char *alf, ParseState *ps) {
  char *start = *s;
  for (int i = 0; i < 5; i++) {
    if (isalnum(**s) || **s == ' ' || **s == '.' || **s == ',' || **s == '(' ||
        **s == ')' || **s == '+' || **s == '-' || **s == '*' || **s == '/' ||
        **s == '=' || **s == '$' || **s == '<' || **s == '>' || **s == '@' ||
        **s == ';' || **s == ':' || **s == '\'')
      *(alf++) = toupper(*((*s)++));
    else
      goto err;
  }
  return true;
err:
  *s = start;
  return false;
}

// Parse line, returning the status and updating the parse state and
// mix instance.
bool parse_line(char *line, ParseState *ps, Mix *mix, ExtraParseInfo *extra) {
  extra->is_source_line = false;
  extra->is_con_or_alf = false;
  extra->is_end = false;

  // Ignore comments
  if (line[0] == '*') return true;

  // Parse LOC field
  bool has_LOC = !isspace(line[0]);
  char sym[11];
  if (has_LOC) {
    if (!parse_sym(&line, sym)) return false;
    if (*line != '\t') return false;
    line++;  // Skip a \t

    // Is LOC a local symbol?
    // Internally, the kth instance of nH is mapped to the symbol
    // "nH#k".  The presence of the non-alphanumeric symbol '#'
    // prevents it from colliding with a user-defined symbol.
    if (isdigit(sym[0]) && sym[1] == 'H' && sym[2] == '\0') {
      int i = sym[0] - '0';
      if (ps->local_sym_counts[i] >= 99)
        return false;
      sym[2] = '#';
      sym[3] = '0' + (ps->local_sym_counts[i] / 10);
      sym[4] = '0' + (ps->local_sym_counts[i] % 10);
      sym[5] = '\0';
      ps->local_sym_counts[i]++;
    }
  }
  else line++;  // Skip a \t

  // Parse OP field

  if (!strncmp(line, "EQU\t", 4)) {
    if (!has_LOC)
      return false;
    line += 4;
    word val;
    if (!parse_W(&line, &val, ps))
      return false;
    add_sym(sym, val, ps);
  }

  else if (!strncmp(line, "ORIG\t", 5)) {
    line += 5;
    if (has_LOC)
      add_sym(sym, pos_word(ps->star), ps);
    word val;
    int i;
    if (!parse_W(&line, &val, ps))
      return false;
    i = word_to_int(val);
    if (i < 0 || i >= 4000) return false;
    ps->star = i;
  }

  else if (!strncmp(line, "CON\t", 4)) {
    line += 4;
    if (has_LOC)
      add_sym(sym, pos_word(ps->star), ps);
    word val;
    if (!parse_W(&line, &val, ps))
      return false;
    mix->mem[ps->star++] = val;
    extra->is_source_line = true;
    extra->is_con_or_alf = true;
  }

  else if (!strncmp(line, "ALF\t", 4)) {
    line += 4;
    if (has_LOC)
      add_sym(sym, pos_word(ps->star), ps);
    char alf[5];
    if (!parse_ALF(&line, alf, ps))
      return false;
    for (int i = 0; i < 5; i++)
      alf[i] = mix_ord(alf[i]);
    mix->mem[ps->star++] = build_word(true, alf[0], alf[1], alf[2], alf[3], alf[4]);
    extra->is_source_line = true;
    extra->is_con_or_alf = true;
  }

  else if (!strncmp(line, "END\t", 4)) {
    line += 4;
    // Handle future references
    for (int i = 0; i < ps->num_future_refs; i++) {
      FutureRef *fr = &ps->future_refs[i];
      word val;
      if (!fr->is_literal && lookup_sym(fr->sym.s, &val, ps)) {
        fr->is_resolved = true;
        word instr = mix->mem[fr->addr];
        mix->mem[fr->addr] = build_instr(word_to_signmag(val), get_I(instr), get_F(instr), get_C(instr));
      }
    }

    // Handle literal constants and undefined symbols
    for (int i = 0; i < ps->num_future_refs; i++) {
      FutureRef *fr = &ps->future_refs[i];
      word val = fr->is_literal ? fr->literal : pos_word(0);
      int addr = ps->star;
      if (!fr->is_resolved) {
        // Check if the symbol is already defined
        // (happens with the non-first instances of an undefined symbol).
        if (!fr->is_literal) {
          word tmp;
          if (lookup_sym(fr->sym.s, &tmp, ps))
            addr = word_to_int(tmp);
          else
            add_sym(fr->sym.s, pos_word(ps->star), ps);
        }
        word instr = mix->mem[fr->addr];
        mix->mem[fr->addr] = build_instr((signmag) {true, addr}, get_I(instr), get_F(instr), get_C(instr));
        mix->mem[ps->star] = val;
        ps->star++;
        fr->is_resolved = true;
      }
    }

    if (has_LOC) add_sym(sym, pos_word(ps->star), ps);

    word val;
    int i;
    if (!parse_W(&line, &val, ps)) return false;
    i = word_to_int(val);
    if (i < 0 || i >= 4000) return false;
    mix->PC = i;
    mix->next_PC = i;
    extra->is_end = true;
  }

  // Treat the OP field as a MIX operator
  else {
    word A = build_word(false, 0, 0, 0, 0, 0);
    byte I = 0, C = 0, F = 0;
    if (!parse_operator(&line, &C, &F)) return false;
    if (has_LOC) add_sym(sym, pos_word(ps->star), ps);
    if (*line == '\n') goto skip;
    line++;  // Skip the tab
    if (!parse_A(&line, &A, ps)) return false;
    if (*line == '\n') goto skip;
    if (!parse_I(&line, &I, ps)) return false;
    if (*line == '\n') goto skip;
    if (!parse_F(&line, &F, ps)) return false;

skip:
    if (I > 6) return false;
    if (F >= 64) return false;
    mix->mem[ps->star++] = build_instr(word_to_signmag(A), I, F, C);
    extra->is_source_line = true;
  }

  return true;
}

#endif
