#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <assert.h>
#include <ctype.h>     // for isalpha, isalnum
#include <stdbool.h>
#include <stdlib.h>
#include "mylib/arena.h"
#include "mylib/str.h"
#include "emulator.h"

// Stores information about a future reference or a literal constant
// to be filled in later.
typedef struct {
  int addr;
  bool is_resolved;
  bool is_literal;
  // Ownership is transferred to ParseState when future refs are
  // resolved upon an END.
  StrBuf sym;
  word literal;
} FutureRef;

#define ASSEMBLER_MAX_SYMS 1000
typedef struct {
  Arena *arena;
  int star;
  int num_syms;
  int num_future_refs;
  // Symbols are copied into a StrBuf because for local symbols nH, an
  // id of the form "#XX" is appended to the end. This is to
  // disambiguate multiple instances of a local symbol with a
  // particular number.
  StrBuf syms[ASSEMBLER_MAX_SYMS];
  word sym_vals[ASSEMBLER_MAX_SYMS];
  FutureRef future_refs[ASSEMBLER_MAX_SYMS];
  // The current number of instances of each local symbol nH.
  int local_sym_counts[10];
} ParseState;

// Returned by reference in parse_line() for mmm to use.
typedef struct {
  bool is_mixal_line;
  bool is_con_or_alf;
  bool is_end;
  // Contains valid data only if is_mixal_line is true.
  StrView mixal_line;
} LineInfo;

void parse_state_init(ParseState *ps, Arena *arena);

bool lookup_sym(StrView sv, word *val, ParseState *ps);
bool parse_sym(StrView *sv, StrView *sym);
bool parse_operator(StrView *sv, byte *C, byte *F);
bool parse_num(StrView *sv, int *val);
// parse_atomic() and the rest all write into a word instead of an
// int, because words and ints are NOT equivalent. Specifically, +0
// != -0.
bool parse_atomic(StrView *sv, word *val, ParseState *ps);
bool parse_expr(StrView *sv, word *val, ParseState *ps);
bool parse_A(StrView *sv, word *A, ParseState *ps);
bool parse_I(StrView *sv, byte *I, ParseState *ps);
bool parse_F(StrView *sv, byte *F, ParseState *ps);
bool parse_W(StrView *sv, word *W, ParseState *ps);
bool parse_line(StrView *sv, ParseState *ps, Mix *mix, LineInfo *line_info);

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
  ps->star = 0;
  ps->num_syms = 0;
  ps->num_future_refs = 0;
  for (int i = 0; i < 10; i++)
    ps->local_sym_counts[i] = 0;
  ps->arena = arena;
}

bool lookup_sym(StrView sv, word *val, ParseState *ps) {
  for (int i = 0; i < ps->num_syms; i++) {
    if (sv_eq_sb(sv, ps->syms[i])) {
      *val = ps->sym_vals[i];
      return true;
    }
  }
  return false;
}

static void add_sym(StrBuf sym_buf, word val, ParseState *ps) {
  ps->syms[ps->num_syms] = sym_buf;
  ps->sym_vals[ps->num_syms] = val;
  ps->num_syms++;
}

// A symbol is a string of one to ten letters and/or digits,
// containing at least one letter.
bool parse_sym(StrView *sv, StrView *sym) {
  char *start = sv->s;
  int i = 0;
  bool has_letter = false;

  while (1) {
    if (i >= 10)
      return false;

    char c = sv->s[i];
    if (isalpha(c))
      has_letter = true;
    else if (!isalnum(c))
      break;

    i++;
  }
  if (!has_letter)
    return false;

  assert(sv_advance(sv, i));
  sym->s = start;
  sym->len = i;
  return true;
}

bool parse_operator(StrView *sv, byte *C, byte *F) {
  char *start = sv->s;
  int i = 0;

  while (1) {
    i++;
    // A MIX operator has at most 5 characters.
    if (i > 5)
      return false;

    char c = sv->s[i];
    if (c == '\t' || c == '\n')
      break;
  }

  StrView sv2 = { .s = start, .len = i };

  int num_ops = (int)(sizeof(mix_op_names) / sizeof(char *));
  for (int idx = 0; idx < num_ops; idx++) {
    if (sv_eq_cstr(sv2, mix_op_names[idx])) {
      *F = default_op_fields[idx];
      *C = opcodes[idx];
      assert(sv_advance(sv, i));
      return true;
    }
  }
  return false;
}

bool parse_num(StrView *sv, int *val) {
  int digits[10];
  int i = 0;
  char c;
  while (1) {
    if (i >= 10)
      return false;

    c = sv->s[i];
    if (!isdigit(c))
      break;
    digits[i] = c - '0';

    i++;
  }
  // Strings like "20BY20" should be parsed as symbols.
  if (isalpha(c))
    return false;

  assert(sv_advance(sv, i));
  *val = digits[0];
  for (int idx = 1; idx < i; idx++)
    *val = 10 * (*val) + digits[idx];
  return true;
}

// parse_atomic() and the rest all write into a word instead of an int,
// because words and ints are NOT equivalent.
// Specifically, +0 != -0.
bool parse_atomic(StrView *sv, word *val, ParseState *ps) {
  StrView old = *sv;
  int num;

  // Number?
  // Note parse_num might fail for an atomic like 20BY20, in case we
  // parse it as symbol in the third branch.
  if (isdigit(sv->s[0]) && parse_num(sv, &num))
    *val = pos_word(num);

  // * refers to the current line
  else if (sv_match_char(sv, '*'))
    *val = pos_word(ps->star);

  // Treat it as a symbol
  else {
    StrView sym_view;
    // Needs to be in scope for lookup_sym()
    char s[5];

    if (!parse_sym(sv, &sym_view))
      goto err;
    // Backward ref?
    if (sym_view.len == 2 && isdigit(sym_view.s[0]) && sym_view.s[1] == 'B') {
      s[0] = sym_view.s[0];
      s[1] = 'H';
      s[2] = '#';
      int n = ps->local_sym_counts[sym_view.s[0] - '0'] - 1;
      if (n < 0)
        goto err;
      s[3] = '0' + (n / 10);
      s[4] = '0' + (n % 10);
      sym_view = sv_from_cstr_n(s, 5);
    }
    if (!lookup_sym(sym_view, val, ps))
      goto err;
  }
  return true;

err:
  *sv = old;
  return false;
}

bool parse_expr(StrView *sv, word *val, ParseState *ps) {
  StrView old = *sv;
  word final = pos_word(0);
  word w;
  char prev_op = '\0';

  if (sv_match_char(sv, '+'))
    prev_op = '+';
  else if (sv_match_char(sv, '-'))
    prev_op = '-';

start:
  if (!parse_atomic(sv, &w, ps))
    goto err;

  // Arithmetic ops

  if (prev_op == '\0')
    final = w;

  else if (prev_op == '+')
    add_word(&final, w);

  else if (prev_op == '-')
    sub_word(&final, w);

  else if (prev_op == '*') {
    word v;
    mul_word(&final, &v, w);
    final = v;
  }

  else if (prev_op == '/') {
    word v = pos_word(0);
    div_word(&v, &final, w);
    final = v;
  }

  else if (prev_op == ':') {
    word v;
    mul_word(&final, &v, pos_word(8));
    add_word(&v, w);
    final = v;
  }

  else if (prev_op == '|') {
    word v = pos_word(0);
    div_word(&final, &v, w);
  }

  // End arithmetic ops

  if (sv_is_empty(*sv)) {
    *val = final;
    return true;
  }

  char c = sv->s[0];
  if (sv_match_one(sv, "+-*:")) {
    prev_op = c;
    goto start;
  }
  else if (sv->s[0] == '/' && sv->s[1] != '/') {
    prev_op = c;
    assert(sv_match_char(sv, '/'));
    goto start;
  }
  else if (sv->s[0] == '/' && sv->s[1] == '/') {
    prev_op = '|';
    assert(sv_match_char(sv, '/'));
    assert(sv_match_char(sv, '/'));
    goto start;
  }
  else {
    *val = final;
    return true;
  }

err:
  *sv = old;
  return false;
}

bool parse_W(StrView *sv, word *W, ParseState *ps);

bool parse_A(StrView *sv, word *A, ParseState *ps) {
  StrView old = *sv;
  StrView sym;

  // Expression?
  if (parse_expr(sv, A, ps))
    return true;

  // Future reference?
  else if (parse_sym(sv, &sym)) {
    StrBuf sym_buf;
    if (sym.len == 2 && isdigit(sym.s[0]) && sym.s[1] == 'F') {
      char s[5];
      s[0] = sym.s[0];
      s[1] = 'H';
      s[2] = '#';
      int n = ps->local_sym_counts[sym.s[0] - '0'];
      s[3] = '0' + n/10;
      s[4] = '0' + n%10;
      sym_buf = sb_from_cstr_n(s, 5, ps->arena);
    }
    else
      sym_buf = sb_from_sv(sym, ps->arena);

    FutureRef fr;
    fr.is_resolved = false;
    fr.addr = ps->star;
    fr.is_literal = false;
    fr.sym = sym_buf;
    ps->future_refs[ps->num_future_refs++] = fr;
    *A = pos_word(0);  // The A-field will be filled in later.
    return true;
  }

  // Local constant?
  else if (sv_match_char(sv, '=')) {
    word w;
    if (!parse_W(sv, &w, ps))
      goto err;
    if (sv->s[0] != '=')
      goto err;
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
  else if (sv->s[0] == '\t' || sv->s[0] == ',' || sv->s[0] == '(') {
    *A = pos_word(0);
    return true;
  }

err:
  *sv = old;
  return false;
}

bool parse_I(StrView *sv, byte *I, ParseState *ps) {
  // Don't skip spaces here; if there is whitespace before the comma,
  // we take it to be a comment, e.g.
  // LDA     0       ,comma
  word w;
  if (sv_match_char(sv, ',')) {
    if (!parse_expr(sv, &w, ps))
      return false;
    *I = w.bytes[4];
  }
  else
    *I = 0;
  return true;
}

bool parse_F(StrView *sv, byte *F, ParseState *ps) {
  // Don't skip spaces here; if there is whitespace before the opening
  // bracket, we take it to be a comment, e.g.
  // LDA     0       (test comment)
  if (sv_match_char(sv, '(')) {
    word w;
    if (!parse_expr(sv, &w, ps) || sv->s[0] != ')')
      return false;
    *F = w.bytes[4];
    assert(sv_advance(sv, 1));
    return true;
  }
  else
    return true;
}

bool parse_W(StrView *sv, word *W, ParseState *ps) {
  word w;
  byte F;
loop:
  F = 5;
  if (!parse_expr(sv, &w, ps))
    return false;
  if (sv->len > 0 && sv->s[0] == '(' && !parse_F(sv, &F, ps))
    return false;
  store_word(W, w, F);
  if (sv->len > 0 && sv_match_char(sv, ','))
    goto loop;
  return true;
}

bool parse_ALF(StrView *sv, char *alf, ParseState *ps) {
  for (int i = 0; i < 5; i++) {
    char c = sv->s[i];
    if (isalnum(c) || c == ' ' || c == '.' || c == ',' || c == '(' ||
        c == ')' || c == '+' || c == '-' || c == '*' || c == '/' ||
        c == '=' || c == '$' || c == '<' || c == '>' || c == '@' ||
        c == ';' || c == ':' || c == '\'')
      alf[i] = toupper(c);
    else
      return false;
  }
  assert(sv_advance(sv, 5));
  return true;
}

// Parse line, returning the status and updating the parse state and
// mix instance.
bool parse_line(StrView *sv, ParseState *ps, Mix *mix, LineInfo *line_info) {
  char *line_start = sv->s;
  line_info->is_mixal_line = false;
  line_info->is_con_or_alf = false;
  line_info->is_end = false;

  // If comment, advance to next newline.
  if (sv->s[0] == '*') {
    if (sv_find_char(sv, '\n'))
      sv_match_char(sv, '\n');
    else
      sv_advance(sv, sv->len);
    return true;
  }

  // Parse LOC field
  bool has_LOC = true;
  if (sv->s[0] == '\t')
    has_LOC = false;

  StrBuf sym_buf;

  if (has_LOC) {
    StrView sym_view;
    if (!parse_sym(sv, &sym_view))
      return false;
    sym_buf = sb_from_sv(sym_view, ps->arena);

    if (sv->s[0] != '\t')
      return false;
    assert(sv_match_char(sv, '\t'));

    // Is LOC a local symbol?
    // Internally, the kth instance of nH is mapped to the symbol
    // "nH#k" (k up to 99 is supported).  The presence of the
    // non-alphanumeric symbol '#' prevents it from colliding with a
    // user-defined symbol.
    if (sym_view.len == 2 && isdigit(sym_view.s[0]) && sym_view.s[1] == 'H') {
      int idx = sym_view.s[0] - '0';
      if (ps->local_sym_counts[idx] >= 99)
        return false;

      char s[3];
      s[0] = '#';
      s[1] = '0' + (ps->local_sym_counts[idx] / 10);
      s[2] = '0' + (ps->local_sym_counts[idx] % 10);
      sb_append_cstr_n(&sym_buf, s, 3, ps->arena);

      ps->local_sym_counts[idx]++;
    }
  }
  else
    assert(sv_advance(sv, 1));

  // Parse OP field

  if (sv_match_cstr(sv, "EQU\t")) {
    if (has_LOC) {
      word val;
      if (!parse_W(sv, &val, ps))
        return false;
      add_sym(sym_buf, val, ps);
    }
    else
      return false;
  }

  else if (sv_match_cstr(sv, "ORIG\t")) {
    if (has_LOC)
      add_sym(sym_buf, pos_word(ps->star), ps);

    word val;
    if (!parse_W(sv, &val, ps))
      return false;
    int addr = word_to_int(val);
    if (addr < 0 || addr >= 4000)
      return false;
    ps->star = addr;
  }

  else if (sv_match_cstr(sv, "CON\t")) {
    if (has_LOC)
      add_sym(sym_buf, pos_word(ps->star), ps);

    word val;
    if (!parse_W(sv, &val, ps))
      return false;
    mix->mem[ps->star++] = val;
    line_info->is_mixal_line = true;
    line_info->is_con_or_alf = true;
  }

  else if (sv_match_cstr(sv, "ALF\t")) {
    if (has_LOC)
      add_sym(sym_buf, pos_word(ps->star), ps);

    char alf[5];
    if (!parse_ALF(sv, alf, ps))
      return false;
    for (int i = 0; i < 5; i++)
      alf[i] = mix_ord(alf[i]);
    mix->mem[ps->star++] = build_word(true, alf[0], alf[1], alf[2], alf[3], alf[4]);
    line_info->is_mixal_line = true;
    line_info->is_con_or_alf = true;
  }

  else if (sv_match_cstr(sv, "END\t")) {
    // Handle future references
    for (int i = 0; i < ps->num_future_refs; i++) {
      FutureRef *fr = &ps->future_refs[i];
      word val;
      if (!fr->is_literal && lookup_sym(sv_from_sb(fr->sym), &val, ps)) {
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
          if (lookup_sym(sv_from_sb(fr->sym), &tmp, ps))
            addr = word_to_int(tmp);
          else
            add_sym(fr->sym, pos_word(ps->star), ps);
        }
        word instr = mix->mem[fr->addr];
        mix->mem[fr->addr] = build_instr((signmag) {true, addr}, get_I(instr), get_F(instr), get_C(instr));
        mix->mem[ps->star] = val;
        ps->star++;
        fr->is_resolved = true;
      }
    }

    if (has_LOC)
      add_sym(sym_buf, pos_word(ps->star), ps);

    word val;
    if (!parse_W(sv, &val, ps))
      return false;
    int addr = word_to_int(val);
    if (addr < 0 || addr >= 4000)
      return false;
    mix->PC = addr;
    mix->next_PC = addr;
    line_info->is_end = true;
  }

  // Treat the OP field as a MIX operator
  else {
    if (has_LOC)
      add_sym(sym_buf, pos_word(ps->star), ps);

    word A = build_word(false, 0, 0, 0, 0, 0);
    byte I = 0, C = 0, F = 0;
    if (!parse_operator(sv, &C, &F))
      return false;
    if (sv->s[0] == '\n')
      goto end_instr;
    assert(sv_match_char(sv, '\t'));
    if (!parse_A(sv, &A, ps))
      return false;
    if (sv->s[0] == '\n')
      goto end_instr;
    if (!parse_I(sv, &I, ps))
      return false;
    if (sv->s[0] == '\n')
      goto end_instr;
    if (!parse_F(sv, &F, ps))
      return false;

end_instr:
    if (I > 6)
      return false;
    if (F >= 64)
      return false;
    mix->mem[ps->star++] = build_instr(word_to_signmag(A), I, F, C);
    line_info->is_mixal_line = true;
  }

  if (sv_find_char(sv, '\n'))
    assert(sv_match_char(sv, '\n'));
  else
    sv_advance(sv, sv->len);

  if (line_info->is_mixal_line) {
    line_info->mixal_line.s = line_start;
    line_info->mixal_line.len = sv->s - line_start;
  }
  else
    line_info->mixal_line = sv_empty();

  return true;
}

#endif // ASSEMBLER_IMPLEMENTATION
#endif // ASSEMBLER_H