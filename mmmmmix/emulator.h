#ifndef _EMULATOR_H
#define _EMULATOR_H
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef enum { false, true } bool;

typedef enum {
  NOP, ADD, SUB, MUL, DIV, SPEC, SHIFT, MOVE,
  LDA, LD1, LD2, LD3, LD4, LD5, LD6, LDX,
  LDAN, LD1N, LD2N, LD3N, LD4N, LD5N, LD6N, LDXN,
  STA, ST1, ST2, ST3, ST4, ST5, ST6, STX,
  STJ, STZ, JBUS, IOC, IN, OUT, JRED, JMP,
  JMPA, JMP1, JMP2, JMP3, JMP4, JMP5, JMP6, JMPX,
  INCA, INC1, INC2, INC3, INC4, INC5, INC6, INCX,
  CMPA, CMP1, CMP2, CMP3, CMP4, CMP5, CMP6, CMPX
} instruction;

typedef enum {
  TAPE0, TAPE1, TAPE2, TAPE3, TAPE4, TAPE5, TAPE6, TAPE7,
  DISK8, DISK9, DISK10, DISK11, DISK12, DISK13, DISK14, DISK15,
  CARDREADER, CARDPUNCH, LINEPRINTER, TERMINAL, PAPERTAPE
} IOdevice;

// A byte in MIX holds 64 values, hence is actually 6-bit.
// The first 2 bits are unused.
typedef uint8_t byte;
// A word consists of 5 bytes and a sign, hence 31-bit.
// The first bit is unused.
// This type is also used for index and jump registers out of
// convenience, even though they are supposed to have 2 bytes.
typedef uint32_t word;

// Useful macros for manipulating words
#define ONES(n)       (((uint64_t)1<<(n)) - 1)
// Extract information from word
#define MAG(w)        ((w) & ONES(30))  // Unset the sign bit
#define SIGN(w)       (((w)>>30) & 1)   // Get the sign bit
// Get integral value of word
#define INT(w)        (SIGN(w) ? MAG(w) : -MAG(w))
// Construct word from an integer:
#define POS(w)        ((w) | (1<<30))
#define NEG(w)        MAG(w)
#define WITHSIGN(w,s) ((s ? POS(w) : NEG(w)))
// For operations involving rA and rX
#define COMBINE(w,v)  ((MAG(w) << 30) | MAG(v))
// Macros for 13-bit addresses
#define SIGNA(A)      (((A)>>12) & 1)
#define MAGA(A)       ((A) & ONES(12))
#define INTA(A)       (SIGNA(A) ? MAGA(A) : -MAGA(A))

// Data relevant to the operation of each IO device
typedef struct {
  word M;
  word F;
  word C;
  int totaltime;
  int timer;
} IOtask;

typedef struct {
  bool done;
  int exitcode;
  char *err;

  int PC;  // Program counter
  // Keep track of the execution counts and times of each memory cell.
  int execcounts[4000];
  int exectimes[4000];
  bool modified[4000];

  bool overflow;
  int cmp;
  word A;
  word X;
  word Is[6];
  word J;
  // Technically the I and J registers only have 2 bytes, but it is
  // convenient to reuse the word type.
  word mem[4000];

  FILE *cardreaderfile;
  FILE *cardpunchfile;
  FILE *tapefiles[8];
  IOtask iotasks[21];

  int INtimes[21];
  int OUTtimes[21];
  int IOCtimes[21];
} mix;

// Construct a 13-bit value consisting of a sign and 2 bytes.
// The 2 bytes store the magnitude of x, i.e. not using two's
// complement.
// Used for the A field of an instruction.
word ADDR(int x);
// Construct a 31-bit word from the given sign and bytes.
word WORD(bool sign, byte b1, byte b2, byte b3, byte b4, byte b5);
// Construct a 31-bit instruction word from the given fields.
// A should be a 13-bit signed value created by ADDR().
word INSTR(word A, byte I, byte F, byte C);

word getA(word instr);
byte getI(word instr);
byte getF(word instr);
byte getC(word instr);
word getM(word instr, mix *mix);
int getinstrtime(int C, int F);

// Return the portion of w specified by the field F.
word applyfield(word w, byte F);
void loadword(word *dest, word src);
void storeword(word *dest, word src, byte F);
word negword(word w);
bool addword(word *dest, word src);
bool subword(word *dest, word src);
void mulword(word *destA, word *destX, word src);
bool divword(word *destA, word *destX, word src);
int compareword(word dest, word src);

void shiftleftword(word *dest, int M);
void shiftrightword(word *dest, int M);
void shiftleftwords(word *destA, word *destX, int M);
void shiftrightwords(word *destA, word *destX, int M);
void shiftleftcirc(word *destA, word *destX, int M);
void shiftrightcirc(word *destA, word *destX, int M);

void wordtonum(word *destA, word *destX);
void numtochar(word *destA, word *destX);
void xor(word *destA, word w);

unsigned char mixchr(byte b, unsigned char *extra);
byte mixord(char c);
void initmix(mix *mix);
void onestep(mix *mix);
#endif