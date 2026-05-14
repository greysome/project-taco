#ifndef EMULATOR_H
#define EMULATOR_H

#include <stdbool.h>
#include <stdint.h>   // for int64_t, uint64_t, uint8_t
#include "mylib/io.h" // for read_all, write_all

// TAOCP 1.3.1: Byte size is at least 64, and at most 100.
// Technically the emulator should work fine if byte size is greater
// than 100, but in mmm.c bytes are hard-coded to be printed with only
// two digits.
#ifndef MIX_BYTE_SIZE
  #define MIX_BYTE_SIZE 64
#endif
#define MIX_MAX ((int64_t) MIX_BYTE_SIZE * MIX_BYTE_SIZE * MIX_BYTE_SIZE * MIX_BYTE_SIZE * MIX_BYTE_SIZE)

typedef uint8_t byte;

typedef struct {
  bool sign;
  byte bytes[5];
} word;

// An alternate representation of a word that is more amenable to
// arithmetic and indexing. The magnitude requires more than 32 bits
// for large byte sizes like 100.
typedef struct {
  bool sign;
  uint64_t mag;
} signmag;

typedef enum {
  NOP, ADD, SUB, MUL, DIV, SPEC, SHIFT, MOVE,
  LDA, LD1, LD2, LD3, LD4, LD5, LD6, LDX,
  LDAN, LD1N, LD2N, LD3N, LD4N, LD5N, LD6N, LDXN,
  STA, ST1, ST2, ST3, ST4, ST5, ST6, STX,
  STJ, STZ, JBUS, IOC, IN, OUT, JRED, JMP,
  JMPA, JMP1, JMP2, JMP3, JMP4, JMP5, JMP6, JMPX,
  INCA, INC1, INC2, INC3, INC4, INC5, INC6, INCX,
  CMPA, CMP1, CMP2, CMP3, CMP4, CMP5, CMP6, CMPX,
} MixOperation;

#define MIX_DEVICE_COUNT 21
typedef enum {
  TAPE_0, TAPE_1, TAPE_2, TAPE_3, TAPE_4, TAPE_5, TAPE_6, TAPE_7,
  DISK_0, DISK_1, DISK_2, DISK_3, DISK_4, DISK_5, DISK_6, DISK_7,
  CARD_READER, CARD_PUNCH, LINE_PRINTER, TERMINAL, PAPER_TAPE,
} IODevice;

typedef struct {
  bool is_active;
  int M;     // source/dest address
  byte F;    // device id
  byte C;    // task type (IN/OUT/IOC)
  int timer; // I/O task is only committed (blocking) when timer hits 0
} IOTask;

typedef struct {
  bool done;
  int exit_code;
  // Hardware exception; use one of the ERR_* strings below.
  char *err;

  int PC;
  // next_PC is the instruction that will be read in the next mix_step() call.
  int next_PC;
  bool overflow;
  int cmp;
  word A;
  word X;
  // Technically rIi and rJ only have 2 bytes, but it is convenient to reuse the word type.
  word I[6];
  word J;
  word mem[4000];

  int device_fds[MIX_DEVICE_COUNT];
  IOTask iotasks[MIX_DEVICE_COUNT];
  // The initial IOTask::timer to set when an I/O instruction is reached.
  int IN_cycles[MIX_DEVICE_COUNT];
  int OUT_cycles[MIX_DEVICE_COUNT];
  int IOC_cycles[MIX_DEVICE_COUNT];

  int trace_counts[4000];
  int trace_cycles[4000];

  // Useful for detecting instruction clobbering.
  bool is_overwritten[4000];
} Mix;

extern char *ERR_INVALID_INSTR;
extern char *ERR_INVALID_PC;
extern char *ERR_INVALID_RA;
extern char *ERR_INVALID_RX;
extern char *ERR_INVALID_RI1;
extern char *ERR_INVALID_RI2;
extern char *ERR_INVALID_RI3;
extern char *ERR_INVALID_RI4;
extern char *ERR_INVALID_RI5;
extern char *ERR_INVALID_RI6;
extern char *ERR_INVALID_RJ;
extern char *ERR_INVALID_ADDR;
extern char *ERR_DEVICE_UNATTACHED;
extern char *ERR_DEVICE_UNEXPECTED_CHAR;
extern char *ERR_DEVICE_MISSING_NEWLINE;
extern char *ERR_DEVICE_INVALID_SEEK;
extern char *ERR_DEVICE_READ_FAILED;
extern char *ERR_DEVICE_WRITE_FAILED;

bool word_eq(word w1, word w2);
bool signmag_eq(signmag sm1, signmag sm2);
signmag word_to_signmag(word w);
word signmag_to_word(signmag a);
int64_t word_to_int(word w);
word int_to_word(int64_t i);
int64_t signmag_to_int(signmag sm);
word pos_word(uint64_t mag);
word neg_word(uint64_t mag);
word set_sign(word w, bool sign);
word build_word(bool sign, byte b1, byte b2, byte b3, byte b4, byte b5);
word build_instr(signmag A, byte I, byte F, byte C);

signmag get_A(word instr);
byte get_I(word instr);
byte get_F(word instr);
byte get_C(word instr);
int get_M(word instr, Mix *mix);

word extract_bytes(word w, byte F);
void store_word(word *dest, word src, byte F);
word negate_word(word w);
bool add_word(word *dest, word src);
bool sub_word(word *dest, word src);
void mul_word(word *dest_A, word *dest_X, word src);
bool div_word(word *dest_A, word *dest_X, word src);
int cmp_word(word dest, word src);

void shift_left_word(word *dest, int shift_amt);
void shift_right_word(word *dest, int shift_amt);
void shift_left_words(word *dest_A, word *dest_X, int shift_amt);
void shift_right_words(word *dest_A, word *dest_X, int shift_amt);
void shift_left_circ(word *dest_A, word *dest_X, int shift_amt);
void shift_right_circ(word *dest_A, word *dest_X, int shift_amt);

void word_to_num(word *dest_A, word *dest_X);
void num_to_char(word *dest_A, word *dest_X);
void xor(word *dest_A, word w);

bool tape_ioc(Mix *, int, int);
bool tape_in(Mix *, int, int);
bool tape_out(Mix *, int, int);

char mix_chr(byte b, char *extra);
byte mix_ord(char c);

int get_exact_cycle_count(int C, int F);
void mix_init(Mix *mix);
void mix_step(Mix *mix);

#ifdef EMULATOR_IMPLEMENTATION

char *ERR_INVALID_INSTR = "invalid instruction";
char *ERR_INVALID_PC = "invalid PC";
char *ERR_INVALID_RA = "invalid rA";
char *ERR_INVALID_RX = "invalid rX";
char *ERR_INVALID_RI1 = "invalid rI1";
char *ERR_INVALID_RI2 = "invalid rI2";
char *ERR_INVALID_RI3 = "invalid rI3";
char *ERR_INVALID_RI4 = "invalid rI4";
char *ERR_INVALID_RI5 = "invalid rI5";
char *ERR_INVALID_RI6 = "invalid rI6";
char *ERR_INVALID_RJ = "invalid rJ";
char *ERR_INVALID_ADDR = "invalid address";
char *ERR_DEVICE_UNATTACHED = "unattached device";
char *ERR_DEVICE_UNEXPECTED_CHAR = "unexpected char in device file";
char *ERR_DEVICE_MISSING_NEWLINE = "missing newline in device file";
char *ERR_DEVICE_INVALID_SEEK = "invalid device seek";
char *ERR_DEVICE_READ_FAILED = "device read failed";
char *ERR_DEVICE_WRITE_FAILED = "device write failed";

static bool word_is_valid(word w) {
  return w.bytes[0] < MIX_BYTE_SIZE &&
    w.bytes[1] < MIX_BYTE_SIZE &&
    w.bytes[2] < MIX_BYTE_SIZE &&
    w.bytes[3] < MIX_BYTE_SIZE &&
    w.bytes[4] < MIX_BYTE_SIZE;
}

// For the rIi and rJ registers.
static bool word_is_valid_short(word w) {
  return w.bytes[0] == 0 &&
    w.bytes[1] == 0 &&
    w.bytes[2] == 0 &&
    w.bytes[3] < MIX_BYTE_SIZE &&
    w.bytes[4] < MIX_BYTE_SIZE;
}

bool word_eq(word w1, word w2) {
  return w1.sign == w2.sign &&
    w1.bytes[0] == w2.bytes[0] &&
    w1.bytes[1] == w2.bytes[1] &&
    w1.bytes[2] == w2.bytes[2] &&
    w1.bytes[3] == w2.bytes[3] &&
    w1.bytes[4] == w2.bytes[4];
}

bool signmag_eq(signmag sm1, signmag sm2) {
  return sm1.sign == sm2.sign && sm1.mag == sm2.mag;
}

signmag word_to_signmag(word w) {
  uint64_t mag = w.bytes[0];
  mag *= MIX_BYTE_SIZE; mag += w.bytes[1];
  mag *= MIX_BYTE_SIZE; mag += w.bytes[2];
  mag *= MIX_BYTE_SIZE; mag += w.bytes[3];
  mag *= MIX_BYTE_SIZE; mag += w.bytes[4];
  return (signmag) { w.sign, mag };
}

int64_t signmag_to_int(signmag sm) {
  return sm.sign ? sm.mag : -sm.mag;
}

int64_t word_to_int(word w) {
  return signmag_to_int(word_to_signmag(w));
}

word signmag_to_word(signmag sm) {
  uint64_t mag = sm.mag;
  uint64_t pow = MIX_MAX;
  word w;

  w.sign = sm.sign;
  pow /= MIX_BYTE_SIZE; w.bytes[0] = mag / pow; mag %= pow;
  pow /= MIX_BYTE_SIZE; w.bytes[1] = mag / pow; mag %= pow;
  pow /= MIX_BYTE_SIZE; w.bytes[2] = mag / pow; mag %= pow;
  pow /= MIX_BYTE_SIZE; w.bytes[3] = mag / pow; mag %= pow;
  pow /= MIX_BYTE_SIZE; w.bytes[4] = mag / pow;
  return w;
}

word int_to_word(int64_t i) {
  int64_t abs = i > 0 ? i : -i;
  return signmag_to_word((signmag) {i > 0, abs});
}

word pos_word(uint64_t mag) {
  return signmag_to_word((signmag) {true, mag});
}

word neg_word(uint64_t mag) {
  return signmag_to_word((signmag) {false, mag});
}

word set_sign(word w, bool sign) {
  w.sign = sign;
  return w;
}

word build_word(bool sign, byte b1, byte b2, byte b3, byte b4, byte b5) {
  assert(b1 < MIX_BYTE_SIZE);
  assert(b2 < MIX_BYTE_SIZE);
  assert(b3 < MIX_BYTE_SIZE);
  assert(b4 < MIX_BYTE_SIZE);
  assert(b5 < MIX_BYTE_SIZE);
  return (word) { sign, {b1,b2,b3,b4,b5} };
}

word build_instr(signmag A, byte I, byte F, byte C) {
  assert(A.mag < MIX_BYTE_SIZE * MIX_BYTE_SIZE);
  assert(I <= 6);
  assert(F < MIX_BYTE_SIZE);
  assert(C < 64);
  byte b1 = A.mag / MIX_BYTE_SIZE;
  byte b2 = A.mag % MIX_BYTE_SIZE;
  return build_word(A.sign, b1, b2, I, F, C);
}

signmag get_A(word instr) {
  int mag = instr.bytes[0] * MIX_BYTE_SIZE + instr.bytes[1];
  return (signmag) { instr.sign, mag };
}

byte get_I(word instr) {
  return instr.bytes[2];
}

byte get_F(word instr) {
  return instr.bytes[3];
}

byte get_C(word instr) {
  return instr.bytes[4];
}

// Return a pointer to
// mix->A,      if i=0
// mix->I(i-1), if 1<=i<=6
// mix->X,      if i=7
// mix->J,      if i=8.
// Used to conveniently implement the LOAD/STORE/JMP/INC/CMP families
// of instructions (essentially any instructions ending with A, X, J
// or a digit 1-6).
static word *get_register_ptr(int i, Mix *mix) {
  assert(0 <= i && i <= 8);
  return i == 0 ? &mix->A
       : i == 7 ? &mix->X
       : i == 8 ? &mix->J
       : &mix->I[i-1];
}

bool add_word(word *dest, word src);

// Return M = A-field + rIi, where i = I-field.
int get_M(word instr, Mix *mix) {
  signmag A = get_A(instr);
  byte I = get_I(instr);
  word A_word = signmag_to_word(A);
  if (I > 0)
    add_word(&A_word, mix->I[I-1]);
  return word_to_int(A_word);
}

// Check if F can be read as a spec of the form (A:B).
static bool is_valid_fieldspec(byte F) {
  int start = F / 8;
  int end = F % 8;
  return 0 <= start && start <= end && end <= 5;
}

// Return the portion of w specified by the field F.
word extract_bytes(word w, byte F) {
  assert(is_valid_fieldspec(F));
  int start = F / 8;
  int end = F % 8;

  if (start > 0)
    w.sign = true;
  else
    start = 1;

  for (int i = 0; i <= 4; i++) {
    if (i <= end-start)
      w.bytes[4-i] = w.bytes[end-1-i];
    else
      w.bytes[4-i] = 0;
  }
  return w;
}

void store_word(word *dest, word src, byte F) {
  assert(is_valid_fieldspec(F));
  int start = F / 8;
  int end = F % 8;

  if (start == 0) {
    dest->sign = src.sign;
    start = 1;
  }
  for (int i = 0; i < end-start+1; i++)
    dest->bytes[end-1-i] = src.bytes[4-i];
}

word negate_word(word w) {
  w.sign = !w.sign;
  return w;
}

bool add_word(word *dest, word src) {
  bool overflow = false;
  signmag sm1 = word_to_signmag(*dest);
  signmag sm2 = word_to_signmag(src);

  if (sm1.sign == sm2.sign) {
    uint64_t mag = sm1.mag + sm2.mag;
    if (mag >= MIX_MAX) {
      overflow = true;
      mag -= MIX_MAX;
    }
    *dest = signmag_to_word((signmag) {sm1.sign, mag});
  }

  else {
    if (sm1.mag < sm2.mag)
      *dest = signmag_to_word((signmag) {sm2.sign, sm2.mag - sm1.mag});
    else
      *dest = signmag_to_word((signmag) {sm1.sign, sm1.mag - sm2.mag});
  }

  return overflow;
}

bool sub_word(word *dest, word src) {
  return add_word(dest, negate_word(src));
}

void mul_word(word *dest_A, word *dest_X, word src) {
  signmag sm_A = word_to_signmag(*dest_A);
  signmag sm_src = word_to_signmag(src);

  __uint128_t prod = (__uint128_t) sm_A.mag * sm_src.mag;
  uint64_t mag_A = prod / MIX_MAX;
  uint64_t mag_X = prod % MIX_MAX;
  bool sign = sm_A.sign == sm_src.sign;
  *dest_A = signmag_to_word((signmag) {sign, mag_A});
  *dest_X = signmag_to_word((signmag) {sign, mag_X});
}

bool div_word(word *dest_A, word *dest_X, word src) {
  signmag sm_A = word_to_signmag(*dest_A);
  signmag sm_X = word_to_signmag(*dest_X);
  signmag sm_src = word_to_signmag(src);
  if (sm_src.mag == 0)
    return true;
  __uint128_t combined = (__uint128_t) sm_A.mag * MIX_MAX + sm_X.mag;
  uint64_t quot = combined / sm_src.mag;
  uint64_t rem = combined % sm_src.mag;
  if (quot >= MIX_MAX)
    return true;
  bool sign = sm_A.sign == sm_src.sign;
  *dest_A = signmag_to_word((signmag) {sign, quot});
  *dest_X = signmag_to_word((signmag) {sign, rem});
  return false;
}

int cmp_word(word dest, word src) {
  int64_t int_dest = word_to_int(dest);
  int64_t int_src = word_to_int(src);
  return int_dest == int_src ? 0 : int_dest > int_src ? 1 : -1;
}

void shift_left_word(word *dest, int shift_amt) {
  assert(shift_amt >= 0);
  if (shift_amt > 5)
    shift_amt = 5;
  for (int i = 0; i <= 4 - shift_amt; i++)
    dest->bytes[i] = dest->bytes[i + shift_amt];
  for (int i = 4 - shift_amt + 1; i <= 4; i++)
    dest->bytes[i] = 0;
}

void shift_right_word(word *dest, int shift_amt) {
  assert(shift_amt >= 0);
  if (shift_amt > 5)
    shift_amt = 5;
  for (int i = 4; i >= shift_amt; i--)
    dest->bytes[i] = dest->bytes[i - shift_amt];
  for (int i = shift_amt - 1; i >= 0; i--)
    dest->bytes[i] = 0;
}

void shift_left_words(word *dest_A, word *dest_X, int shift_amt) {
  assert(shift_amt >= 0);
  byte combined_bytes[10];

  for (int i = 0; i <= 4; i++) {
    combined_bytes[i] = dest_A->bytes[i];
    combined_bytes[i+5] = dest_X->bytes[i];
  }

  if (shift_amt > 10)
    shift_amt = 10;
  for (int i = 0; i <= 9 - shift_amt; i++)
    combined_bytes[i] = combined_bytes[i + shift_amt];
  for (int i = 9 - shift_amt + 1; i <= 9; i++)
    combined_bytes[i] = 0;

  for (int i = 0; i <= 4; i++) {
    dest_A->bytes[i] = combined_bytes[i];
    dest_X->bytes[i] = combined_bytes[i+5];
  }
}

void shift_right_words(word *dest_A, word *dest_X, int shift_amt) {
  assert(shift_amt >= 0);
  byte combined_bytes[10];
  int i;

  for (i = 0; i <= 4; i++) {
    combined_bytes[i] = dest_A->bytes[i];
    combined_bytes[i+5] = dest_X->bytes[i];
  }

  if (shift_amt > 10)
    shift_amt = 10;
  for (i = 9; i >= shift_amt; i--)
    combined_bytes[i] = combined_bytes[i - shift_amt];
  for (i = shift_amt - 1; i >= 0; i--)
    combined_bytes[i] = 0;

  for (i = 0; i <= 4; i++) {
    dest_A->bytes[i] = combined_bytes[i];
    dest_X->bytes[i] = combined_bytes[i+5];
  }
}

void shift_left_circ(word *dest_A, word *dest_X, int shift_amt) {
  assert(shift_amt >= 0);
  byte combined_bytes[10];
  byte new_bytes[10];

  for (int i = 0; i <= 4; i++) {
    combined_bytes[i] = dest_A->bytes[i];
    combined_bytes[i+5] = dest_X->bytes[i];
  }

  for (int i = 0; i <= 9; i++)
    new_bytes[i] = combined_bytes[(i + shift_amt) % 10];

  for (int i = 0; i <= 4; i++) {
    dest_A->bytes[i] = new_bytes[i];
    dest_X->bytes[i] = new_bytes[i+5];
  }
}

void shift_right_circ(word *dest_A, word *dest_X, int shift_amt) {
  assert(shift_amt >= 0);
  byte combined_bytes[10];
  byte new_bytes[10];

  for (int i = 0; i <= 4; i++) {
    combined_bytes[i] = dest_A->bytes[i];
    combined_bytes[i+5] = dest_X->bytes[i];
  }

  shift_amt %= 10;
  for (int i = 0; i <= 9; i++)
    new_bytes[i] = combined_bytes[(i + 10 - shift_amt) % 10];

  for (int i = 0; i <= 4; i++) {
    dest_A->bytes[i] = new_bytes[i];
    dest_X->bytes[i] = new_bytes[i+5];
  }
}

void word_to_num(word *dest_A, word *dest_X) {
  uint64_t mag = dest_A->bytes[0] % 10;
  mag *= 10; mag += dest_A->bytes[1] % 10;
  mag *= 10; mag += dest_A->bytes[2] % 10;
  mag *= 10; mag += dest_A->bytes[3] % 10;
  mag *= 10; mag += dest_A->bytes[4] % 10;
  mag *= 10; mag += dest_X->bytes[0] % 10;
  mag *= 10; mag += dest_X->bytes[1] % 10;
  mag *= 10; mag += dest_X->bytes[2] % 10;
  mag *= 10; mag += dest_X->bytes[3] % 10;
  mag *= 10; mag += dest_X->bytes[4] % 10;
  *dest_A = signmag_to_word((signmag) {dest_A->sign, mag});
}

void num_to_char(word *dest_A, word *dest_X) {
  uint64_t mag = word_to_signmag(*dest_A).mag;
  dest_A->bytes[0] = 30 + mag / 1000000000; mag %= 1000000000;
  dest_A->bytes[1] = 30 + mag / 100000000;  mag %= 100000000;
  dest_A->bytes[2] = 30 + mag / 10000000;   mag %= 10000000;
  dest_A->bytes[3] = 30 + mag / 1000000;    mag %= 1000000;
  dest_A->bytes[4] = 30 + mag / 100000;     mag %= 100000;
  dest_X->bytes[0] = 30 + mag / 10000;      mag %= 10000;
  dest_X->bytes[1] = 30 + mag / 1000;       mag %= 1000;
  dest_X->bytes[2] = 30 + mag / 100;        mag %= 100;
  dest_X->bytes[3] = 30 + mag / 10;         mag %= 10;
  dest_X->bytes[4] = 30 + mag;
}

void xor(word *dest_A, word w) {
  dest_A->bytes[0] ^= w.bytes[0];
  dest_A->bytes[1] ^= w.bytes[1];
  dest_A->bytes[2] ^= w.bytes[2];
  dest_A->bytes[3] ^= w.bytes[3];
  dest_A->bytes[4] ^= w.bytes[4];
}

// Encode charset.
// Some character codes are non-ASCII, and in that case the remaining
// Unicode codepoint data is set in extra.
char mix_chr(byte b, char *extra) {
  if (extra)
    *extra = '\0';

  switch (b) {
  case 0:
    return ' ';

  case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9:
    return 'A' + b - 1;

  case 10:
    if (extra)
      *extra = (char) 0x94;  // \u0394, Delta
    return (char) 0xce;

  case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19:
    return 'J' + b - 11;

  case 20:
    if (extra)
      *extra = (char) 0xa3; // \u03a3, Sigma
    return (char) 0xce;

  case 21:
    if (extra)
      *extra = (char) 0xa0; // \u03a0, Pi
    return (char) 0xce;

  case 22: case 23: case 24: case 25: case 26: case 27: case 28: case 29:
    return 'S' + b - 22;

  case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37: case 38: case 39:
    return '0' + b - 30;

  case 40: return '.';
  case 41: return ',';
  case 42: return '(';
  case 43: return ')';
  case 44: return '+';
  case 45: return '-';
  case 46: return '*';
  case 47: return '/';
  case 48: return '=';
  case 49: return '$';
  case 50: return '<';
  case 51: return '>';
  case 52: return '@';
  case 53: return ';';
  case 54: return ':';
  case 55: return '\'';
  default: assert(false && "unreachable");
  }
}

// Decode charset.
// Note that !, [, ] are used in place of their Unicode counterparts
// Delta, Sigma, Pi.
byte mix_ord(char c) {
  switch (c) {
  case ' ': return 0;
  case '!': return 10;
  case '[': return 20;
  case ']': return 21;
  case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I':
    return 1 + (c - 'A');
  case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
    return 11 + (c - 'J');
  case 'S': case 'T': case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
    return 22 + (c - 'S');
  case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
    return 30 + (c - '0');
  case '.': return 40;
  case ',': return 41;
  case '(': return 42;
  case ')': return 43;
  case '+': return 44;
  case '-': return 45;
  case '*': return 46;
  case '/': return 47;
  case '=': return 48;
  case '$': return 49;
  case '<': return 50;
  case '>': return 51;
  case '@': return 52;
  case ';': return 53;
  case ':': return 54;
  case '\'': return 55;
  default: return 63;
  }
}

bool is_valid_addr(int M) {
  return M >= 0 && M < 4000;
}

static bool cardreader_in(Mix *mix, int M) {
  int fd = mix->device_fds[CARD_READER];
  if (fd == -1) {
    mix->err = ERR_DEVICE_UNATTACHED;
    return false;
  }

  char buf[81];
  if (!read_all(fd, buf, 81)) {
    mix->err = ERR_DEVICE_READ_FAILED;
    return false;
  }
  if (buf[80] != '\n') {
    mix->err = ERR_DEVICE_MISSING_NEWLINE;
    return false;
  }

  for (int i = 0; i < 80; i++) {
    char c = buf[i];
    if (mix_ord(c) == 63) {
      mix->err = ERR_DEVICE_UNEXPECTED_CHAR;
      return false;
    }

    int addr = M + i / 5;
    if (!is_valid_addr(addr)) {
      mix->err = ERR_INVALID_ADDR;
      return false;
    }

    int byte_pos = i % 5;
    // All words in cards are positive.
    if (byte_pos == 0)
      mix->mem[addr].sign = true;
    // Store the character in the appropriate byte of the address.
    mix->mem[addr].bytes[byte_pos] = mix_ord(c);
  }
  return true;
}

static bool cardpunch_out(Mix *mix, int M) {
  // TODO: card punch out
  //if (mix->card_punch_file == NULL) {
  //  mix->err = ERR_DEVICE_UNATTACHED;
  //  return false;
  //}

  //for (int addr = M; addr < M+16; addr++) {
  //  if (!is_valid_addr(addr)) {
  //    mix->err = ERR_INVALID_ADDR;
  //    return false;
  //  }

  //  word w = mix->mem[addr];
  //  for (int i = 0; i <= 4; i++) {
  //    byte b = w.bytes[i];
  //    fputc(mix_chr(b, NULL), mix->card_punch_file);
  //  }
  //}
  //fputc('\n', mix->card_punch_file);
  //return true;
  return true;
}

static bool lineprinter_out(Mix *mix, int M) {
  // Each line has 120 characters (plus the trailing \n\0), and a
  // character may need 2 bytes to encode (for the codepoints
  // 10=Delta, 20=Sigma, 21=Pi).
  char line[120*2+2];
  int c = 0;

  for (int i = 0; i < 24; i++) {
    int addr = M + i;
    if (!is_valid_addr(addr)) {
      mix->err = ERR_INVALID_ADDR;
      return false;
    }

    // Add the characters in the word to the line buffer.
    word w = mix->mem[addr];
    for (int j = 0; j <= 4; j++) {
      char extra;
      line[c++] = mix_chr(w.bytes[j], &extra);
      if (extra)
        line[c++] = extra;
    }
  }
  line[c++] = '\n';
  line[c++] = '\0';
  my_print(line);
  return true;
}

bool tape_ioc(Mix *mix, int M, int unit) {
  int fd = mix->device_fds[TAPE_0 + unit];
  if (fd == -1) {
    mix->err = ERR_DEVICE_UNATTACHED;
    return false;
  }

  // We should be at the start of a tape block.
  // (A tape block is represented by 601 characters -- 6 for each word
  // and then a final \n.)
  long cur_pos = lseek(fd, 0, SEEK_CUR);
  assert(cur_pos % 601L == 0);

  if (M == 0) // Rewind to start of tape
    lseek(fd, 0L, SEEK_SET);
  else {      // Move tape forward or backward
    long new_pos = cur_pos + 601L * M;
    if (new_pos < 0)
      lseek(fd, 0L, SEEK_SET);
    else if (new_pos >= 601L * 1000) {
      mix->err = ERR_DEVICE_INVALID_SEEK;
      return false;
    }
    else
      lseek(fd, new_pos, SEEK_SET);
  }

  assert(lseek(fd, 0, SEEK_CUR) % 601L == 0);
  return true;
}

bool tape_in(Mix *mix, int M, int unit) {
  int fd = mix->device_fds[TAPE_0 + unit];
  if (fd == -1) {
    mix->err = ERR_DEVICE_UNATTACHED;
    return false;
  }

  // We should be at the start of a tape block.
  assert(lseek(fd, 0, SEEK_CUR) % 601L == 0);

  char buf[601];
  if (!read_all(fd, buf, 601)) {
    mix->err = ERR_DEVICE_READ_FAILED;
    return false;
  }
  if (buf[600] != '\n') {
    mix->err = ERR_DEVICE_MISSING_NEWLINE;
    return false;
  }

  for (int i = 0; i < 600; i++) {
    char c = buf[i];

    int addr = M + i / 6;
    if (!is_valid_addr(addr)) {
      mix->err = ERR_INVALID_ADDR;
      return false;
    }

    int x = i % 6;
    // Load the sign
    if (x == 0) {
      if (c == '#')
        mix->mem[addr].sign = true;
      else if (c == '~')
        mix->mem[addr].sign = false;
      else {
	mix->err = ERR_DEVICE_UNEXPECTED_CHAR;
        return false;
      }
    }
    // Load a byte
    else {
      if (mix_ord(c) == 63 && c != 'h') {
        mix->err = ERR_DEVICE_UNEXPECTED_CHAR;
        return false;
      }
      mix->mem[addr].bytes[x-1] = mix_ord(c);
    }
  }

  assert(lseek(fd, 0, SEEK_CUR) % 601L == 0);
  return true;
}

bool tape_out(Mix *mix, int M, int unit) {
  int fd = mix->device_fds[TAPE_0 + unit];
  if (fd == -1) {
    mix->err = ERR_DEVICE_UNATTACHED;
    return false;
  }

  // We should be at the start of a tape block.
  assert(lseek(fd, 0, SEEK_CUR) % 601 == 0);

  char buf[601];
  int buf_ptr = 0;

  for (int i = 0; i < 100; i++) {
    int addr = M + i;
    if (!is_valid_addr(addr)) {
      mix->err = ERR_INVALID_ADDR;
      return false;
    }

    // Write the word to the tape file.
    word w = mix->mem[addr];
    buf[buf_ptr++] = w.sign ? '#' : '~';
    for (int j = 0; j <= 4; j++) {
      byte b = w.bytes[j];
      if (b == 10)
        buf[buf_ptr++] = '!';
      else if (b == 20)
        buf[buf_ptr++] = '[';
      else if (b == 21)
        buf[buf_ptr++] = ']';
      else
        buf[buf_ptr++] = mix_chr(w.bytes[j], NULL);
    }
  }
  buf[buf_ptr++] = '\n';

  if (!write_all(fd, buf, 601)) {
    mix->err = ERR_DEVICE_WRITE_FAILED;
    return false;
  }

  assert(lseek(fd, 0, SEEK_CUR) % 601 == 0);
  return true;
}

static bool is_tape(byte F) {
  return F >= TAPE_0 && F <= TAPE_7;
}

// Actually perform the I/O (called when iotask->timer hits 0).
static bool do_io_transmission(IOTask *iotask, Mix *mix) {
  assert(iotask->is_active);
  int M = iotask->M;
  byte F = iotask->F;
  byte C = iotask->C;
  bool status;

  if (F == CARD_READER && C == IN)
    status = cardreader_in(mix, M);
  else if (F == CARD_PUNCH && C == OUT)
    status = cardpunch_out(mix, M);
  else if (F == LINE_PRINTER && C == OUT)
    status = lineprinter_out(mix, M);
  // Not implemented yet.
  else if (F == LINE_PRINTER && C == IOC)
    status = true;
  else if (is_tape(F)) {
    if (C == IOC)
      status = tape_ioc(mix, M, F);
    else if (C == IN)
      status = tape_in(mix, M, F);
    else if (C == OUT)
      status = tape_out(mix, M, F);
  }
  // Other devices have not been implemented.
  else
    assert(false && "invalid iotask->F");
  return status;
}

// V = value at memory address M, according to the field spec F
word get_V(int M, byte F, Mix *mix) {
  assert(is_valid_addr(M));
  return extract_bytes(mix->mem[M], F);
}

// Implementation of MIX instructions
// -----------------------------------------------------------------
// Return value of false means that A,F,I or M fields of the given
// instruction C is invalid.

bool instr_nop(signmag A, byte C, byte F, byte I, int M, Mix *mix) {
  return true;
}

bool instr_add(signmag A, byte C, byte F, byte I, int M, Mix *mix) {
  if (!is_valid_fieldspec(F) || !is_valid_addr(M))
    return false;
  mix->overflow = add_word(&mix->A, get_V(M, F, mix));
  return true;
}

bool instr_sub(signmag A, byte C, byte F, byte I, int M, Mix *mix) {
  if (!is_valid_fieldspec(F) || !is_valid_addr(M))
    return false;
  mix->overflow = sub_word(&mix->A, get_V(M, F, mix));
  return true;
}

bool instr_mul(signmag A, byte C, byte F, byte I, int M, Mix *mix) {
  if (!is_valid_fieldspec(F) || !is_valid_addr(M))
    return false;
  mul_word(&mix->A, &mix->X, get_V(M, F, mix));
  return true;
}

bool instr_div(signmag A, byte C, byte F, byte I, int M, Mix *mix) {
  if (!is_valid_fieldspec(F) || !is_valid_addr(M))
    return false;
  mix->overflow = div_word(&mix->A, &mix->X, get_V(M, F, mix));
  return true;
}

bool instr_spec(signmag A, byte C, byte F, byte I, int M, Mix *mix) {
  if (F == 0)
    word_to_num(&mix->A, &mix->X);
  else if (F == 1)
    num_to_char(&mix->A, &mix->X);
  else if (F == 2) {
    mix->done = true;
    mix->exit_code = M;
  }
  else if (F == 5) {
    if (!is_valid_addr(M))
      return false;
    xor(&mix->A, get_V(M, F, mix));
  }
  return true;
}

bool instr_shift(signmag A, byte C, byte F, byte I, int M, Mix *mix) {
  if (F == 0)
    shift_left_word(&mix->A, M);
  else if (F == 1)
    shift_right_word(&mix->A, M);
  else if (F == 2)
    shift_left_words(&mix->A, &mix->X, M);
  else if (F == 3)
    shift_right_words(&mix->A, &mix->X, M);
  else if (F == 4)
    shift_left_circ(&mix->A, &mix->X, M);
  else if (F == 5)
    shift_right_circ(&mix->A, &mix->X, M);
  else
    return false;
  return true;
}

bool instr_move(signmag A, byte C, byte F, byte I, int M, Mix *mix) {
  int int_rI = word_to_int(mix->I[0]);
  for (int i = 0; i < F; i++) {
    if (!is_valid_addr(M+i) || !is_valid_addr(int_rI))
      return false;
    mix->mem[int_rI] = mix->mem[M+i];
    int_rI++;
  }
  mix->I[0] = int_to_word(int_rI);
  return true;
}

bool instr_load(signmag A, byte C, byte F, byte I, int M, Mix *mix) {
  if (!is_valid_fieldspec(F) || !is_valid_addr(M))
    return false;
  int i = C - LDA;
  word *reg = get_register_ptr(i, mix);
  *reg = get_V(M, F, mix);
  return true;
}

bool instr_load_neg(signmag A, byte C, byte F, byte I, int M, Mix *mix) {
  if (!is_valid_fieldspec(F) || !is_valid_addr(M))
    return false;
  int i = C - LDAN;
  word *reg = get_register_ptr(i, mix);
  *reg = get_V(M, F, mix);
  if (F % 8 >= 1)
    reg->sign = false;
  else *reg = negate_word(*reg);
  return true;
}

bool instr_store(signmag A, byte C, byte F, byte I, int M, Mix *mix) {
  if (!is_valid_fieldspec(F) || !is_valid_addr(M))
    return false;
  int i = C - STA;
  mix->is_overwritten[M] = true;
  if (C == STZ)
    store_word(&mix->mem[M], pos_word(0), F);
  else {
    word *reg = get_register_ptr(i, mix);
    store_word(&mix->mem[M], *reg, F);
  }
  return true;
}

bool instr_jmp_busy(signmag A, byte C, byte F, byte I, int M, Mix *mix) {
  if (!is_tape(F) && F != CARD_READER && F != CARD_PUNCH && F != LINE_PRINTER)
    return false;
  if (mix->iotasks[F].timer > 0) {
    mix->J = pos_word(mix->PC + 1);
    mix->next_PC = M;
  }
  return true;
}

bool instr_io(signmag A, byte C, byte F, byte I, int M, Mix *mix) {
  // Does the nature of the device (input or output) match the
  // instruction given?
  if (F >= MIX_DEVICE_COUNT ||
      (C == IOC && !(is_tape(F) || F == LINE_PRINTER)) ||
      (C == IN  && !(is_tape(F) || F == CARD_READER)) ||
      (C == OUT && !(is_tape(F) || F == LINE_PRINTER || F == CARD_PUNCH))) {
    return false;
  }

  IOTask *iotask = mix->iotasks + F;
  // If I/O device is currently busy, we will fast forward to when it
  // is ready (and perform the I/O if it hasn't been done yet).
  if (iotask->is_active && iotask->timer > 0)
    if (!do_io_transmission(iotask, mix))
      return false;
  // To simulate the fast forwarding effect, we simply update the other devices' timers...
  for (int devid = 0; devid < MIX_DEVICE_COUNT; devid++)
    mix->iotasks[devid].timer -= iotask->timer;
  // ... and add to execution time.
  mix->trace_cycles[mix->PC] += iotask->timer;

  // Register an I/O task.
  iotask->is_active = true;
  iotask->M = M;
  iotask->F = F;
  iotask->C = C;
  if (C == IOC)
    iotask->timer = mix->IOC_cycles[F];
  else if (C == IN)
    iotask->timer = mix->IN_cycles[F];
  else if (C == OUT)
    iotask->timer = mix->OUT_cycles[F];
  return true;
}

bool instr_jmp_ready(signmag A, byte C, byte F, byte I, int M, Mix *mix) {
  if (!is_tape(F) && F != CARD_READER && F != CARD_PUNCH && F != LINE_PRINTER)
    return false;
  if (mix->iotasks[F].timer == 0) {
    mix->J = pos_word(mix->PC + 1);
    mix->next_PC = M;
  }
  return true;
}

bool instr_jmp_test(signmag A, byte C, byte F, byte I, int M, Mix *mix) {
  if (F >= 10)
    return false;
  if (F == 0 || F == 1 ||
      (F == 2 && mix->overflow)  ||
      (F == 3 && !mix->overflow) ||
      (F == 4 && mix->cmp < 0)   ||
      (F == 5 && mix->cmp == 0)  ||
      (F == 6 && mix->cmp > 0)   ||
      (F == 7 && mix->cmp >= 0)  ||
      (F == 8 && mix->cmp != 0)  ||
      (F == 9 && mix->cmp <= 0)) {
    if (!is_valid_addr(M))
      return false;
    if (F != 1)
      mix->J = pos_word(mix->PC + 1);
    mix->next_PC = M;
  }
  return true;
}

bool instr_jmp_register(signmag A, byte C, byte F, byte I, int M, Mix *mix) {
  if (F >= 6)
    return false;
  word w = *get_register_ptr(C - JMPA, mix);
  int i = word_to_int(w);
  if ((F == 0 && i < 0)  ||
      (F == 1 && i == 0) ||
      (F == 2 && i > 0)  ||
      (F == 3 && i >= 0) ||
      (F == 4 && i != 0) ||
      (F == 5 && i <= 0)) {
    mix->J = pos_word(mix->PC + 1);
    mix->next_PC = M;
  }
  return true;
}

bool instr_inc_register(signmag A, byte C, byte F, byte I, int M, Mix *mix) {
  if (F >= 4)
    return false;
  word w = int_to_word(M);
  int i = C - INCA;
  word *reg = get_register_ptr(i, mix);

  if (F == 0)
    mix->overflow = add_word(reg, w);
  else if (F == 1)
    mix->overflow = sub_word(reg, w);
  else if (F == 2)
    *reg = w;
  else if (F == 3)
    *reg = negate_word(w);
  if (M == 0) reg->sign = A.sign;
  return true;
}

bool instr_cmp_register(signmag A, byte C, byte F, byte I, int M, Mix *mix) {
  if (!is_valid_fieldspec(F) || !is_valid_addr(M))
    return false;
  int i = C - CMPA;
  word *reg = get_register_ptr(i, mix);
  mix->cmp = cmp_word(extract_bytes(*reg, F), get_V(M, F, mix));
  return true;
}

bool (*instr_dispatch_table[64])(signmag, byte, byte, byte, int, Mix *) = {
  [NOP] = instr_nop,
  [ADD] = instr_add,
  [SUB] = instr_sub,
  [MUL] = instr_mul,
  [DIV] = instr_div,
  [SPEC] = instr_spec,
  [SHIFT] = instr_shift,
  [MOVE] = instr_move,
  [LDA] = instr_load,
  [LD1] = instr_load,
  [LD2] = instr_load,
  [LD3] = instr_load,
  [LD4] = instr_load,
  [LD5] = instr_load,
  [LD6] = instr_load,
  [LDX] = instr_load,
  [LDAN] = instr_load_neg,
  [LD1N] = instr_load_neg,
  [LD2N] = instr_load_neg,
  [LD3N] = instr_load_neg,
  [LD4N] = instr_load_neg,
  [LD5N] = instr_load_neg,
  [LD6N] = instr_load_neg,
  [LDXN] = instr_load_neg,
  [STA] = instr_store,
  [ST1] = instr_store,
  [ST2] = instr_store,
  [ST3] = instr_store,
  [ST4] = instr_store,
  [ST5] = instr_store,
  [ST6] = instr_store,
  [STX] = instr_store,
  [STJ] = instr_store,
  [STZ] = instr_store,
  [JBUS] = instr_jmp_busy,
  [IOC] = instr_io,
  [IN] = instr_io,
  [OUT] = instr_io,
  [JRED] = instr_jmp_ready,
  [JMP] = instr_jmp_test,
  [JMPA] = instr_jmp_register,
  [JMP1] = instr_jmp_register,
  [JMP2] = instr_jmp_register,
  [JMP3] = instr_jmp_register,
  [JMP4] = instr_jmp_register,
  [JMP5] = instr_jmp_register,
  [JMP6] = instr_jmp_register,
  [JMPX] = instr_jmp_register,
  [INCA] = instr_inc_register,
  [INC1] = instr_inc_register,
  [INC2] = instr_inc_register,
  [INC3] = instr_inc_register,
  [INC4] = instr_inc_register,
  [INC5] = instr_inc_register,
  [INC6] = instr_inc_register,
  [INCX] = instr_inc_register,
  [CMPA] = instr_cmp_register,
  [CMP1] = instr_cmp_register,
  [CMP2] = instr_cmp_register,
  [CMP3] = instr_cmp_register,
  [CMP4] = instr_cmp_register,
  [CMP5] = instr_cmp_register,
  [CMP6] = instr_cmp_register,
  [CMPX] = instr_cmp_register,
};

// Cycle count of each instruction, from TAOCP 1.3.1, Table 1. Note
// that the cycle count of MOVE and SPEC depends on the F field. (So
// are IOC/IN/OUT, but these instructions will be specially handled.)
static int cycle_counts[64] = {
  [NOP] = 1, [ADD] = 2, [SUB] = 2, [MUL] = 10, [DIV] = 12, [SPEC] = -1, [SHIFT] = 2, [MOVE] = -1,
  [LDA] = 2, [LD1] = 2, [LD2] = 2, [LD3] = 2, [LD4] = 2, [LD5] = 2, [LD6] = 2, [LDX] = 2,
  [LDAN] = 2, [LD1N] = 2, [LD2N] = 2, [LD3N] = 2, [LD4N] = 2, [LD5N] = 2, [LD6N] = 2, [LDXN] = 2,
  [STA] = 2, [ST1] = 2, [ST2] = 2, [ST3] = 2, [ST4] = 2, [ST5] = 2, [ST6] = 2, [STX] = 2,
  [STJ] = 2, [STZ] = 2, [JBUS] = 1, [IOC] = 1, [IN] = 1, [OUT] = 1, [JRED] = 1, [JMP] = 1,
  [JMPA] = 1, [JMP1] = 1, [JMP2] = 1, [JMP3] = 1, [JMP4] = 1, [JMP5] = 1, [JMP6] = 1, [JMPX] = 1,
  [INCA] = 1, [INC1] = 1, [INC2] = 1, [INC3] = 1, [INC4] = 1, [INC5] = 1, [INC6] = 1, [INCX] = 1,
  [CMPA] = 2, [CMP1] = 2, [CMP2] = 2, [CMP3] = 2, [CMP4] = 2, [CMP5] = 2, [CMP6] = 2, [CMPX] = 2,
};

int get_exact_cycle_count(int C, int F) {
  if (C == 5) {
    if (F == 0 || F == 1)
      return 10;
    else if (F == 2)
      return 0;
    else if (F == 5)
      return 2;
  }
  else if (C == 7)
    return 1 + 2*F;
  return cycle_counts[C];
}

static bool validate_registers(Mix *mix) {
  if (!word_is_valid(mix->A)) { mix->err = ERR_INVALID_RA; return false; }
  else if (!word_is_valid(mix->X)) { mix->err = ERR_INVALID_RX; return false; }
  else if (!word_is_valid_short(mix->I[0])) { mix->err = ERR_INVALID_RI1; return false; }
  else if (!word_is_valid_short(mix->I[1])) { mix->err = ERR_INVALID_RI2; return false; }
  else if (!word_is_valid_short(mix->I[2])) { mix->err = ERR_INVALID_RI3; return false; }
  else if (!word_is_valid_short(mix->I[3])) { mix->err = ERR_INVALID_RI4; return false; }
  else if (!word_is_valid_short(mix->I[4])) { mix->err = ERR_INVALID_RI5; return false; }
  else if (!word_is_valid_short(mix->I[5])) { mix->err = ERR_INVALID_RI6; return false; }
  else if (!word_is_valid_short(mix->J)) { mix->err = ERR_INVALID_RJ; return false; }
  return true;
}

static bool update_io(Mix *mix, int cycles_to_advance) {
  IOTask *iotask;
  for (int devid = 0; devid < MIX_DEVICE_COUNT; devid++) {
    iotask = mix->iotasks + devid;
    if (iotask->is_active && iotask->timer < 0) {
      if (!do_io_transmission(iotask, mix))
        return false;
      iotask->is_active = false;
    }
    iotask->timer -= cycles_to_advance;
  }
  return true;
}

void mix_init(Mix *mix) {
  mix->done = false;
  mix->exit_code = 0;
  mix->err = NULL;
  mix->PC = 0;

  for (int addr = 0; addr < 4000; addr++) {
    mix->trace_cycles[addr] = 0;
    mix->trace_counts[addr] = 0;
    mix->mem[addr] = pos_word(0);
    mix->is_overwritten[addr] = false;
  }

  mix->overflow = false;
  mix->cmp = 0;
  mix->A = pos_word(0);
  mix->X = pos_word(0);
  mix->I[0] = pos_word(0);
  mix->I[1] = pos_word(0);
  mix->I[2] = pos_word(0);
  mix->I[3] = pos_word(0);
  mix->I[4] = pos_word(0);
  mix->I[5] = pos_word(0);
  mix->J = pos_word(0);

  for (int devid = 0; devid < MIX_DEVICE_COUNT; devid++) {
    mix->device_fds[devid] = -1;
    mix->iotasks[devid].is_active = false;
  }
}

void mix_step(Mix *mix) {
  if (mix->done)
    return;

  mix->PC = mix->next_PC;
  mix->next_PC = mix->PC + 1;
  if (!is_valid_addr(mix->PC)) {
    mix->err = ERR_INVALID_PC;
    mix->done = true;
  }

  word instr = mix->mem[mix->PC];
  signmag A = get_A(instr);
  byte C = get_C(instr);
  if (C >= 64) {
    mix->err = ERR_INVALID_INSTR;
    mix->done = true;
  }
  byte F = get_F(instr);
  byte I = get_I(instr);
  int M = get_M(instr, mix);

  mix->next_PC = mix->PC + 1;
  if (!instr_dispatch_table[C](A, C, F, I, M, mix)) {
    mix->err = ERR_INVALID_INSTR;
    mix->done = true;
  }
  if (!validate_registers(mix))
    mix->done = true;
  int cycles = get_exact_cycle_count(C, F);
  if (!update_io(mix, cycles))
    mix->done = true;

  mix->trace_counts[mix->PC]++;
  mix->trace_cycles[mix->PC] += cycles;

  if (mix->done) {
    for (int devid = 0; devid < MIX_DEVICE_COUNT; devid++) {
      IOTask *iotask = mix->iotasks + devid;
      if (iotask->is_active)
	do_io_transmission(iotask, mix);
    }
  }
}

#endif // EMULATOR_IMPLEMENTATION
#endif // EMULATOR_H