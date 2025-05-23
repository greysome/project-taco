#include "emulator.h"

// Cycle count of each instruction, from TAOCP Section 1.3.1, Table 1.
// Note that the cycle count of MOVE is variable.
// (So are IOC/IN/OUT, but these instructions will be specially handled.)
static int instrtimes[64] = { 1, 2, 2, 10, 12, 10, 2, -1,
		              2, 2, 2,  2,  2,  2, 2,  2,
		              2, 2, 2,  2,  2,  2, 2,  2,
		              2, 2, 2,  2,  2,  2, 2,  2,
		              2, 2, 1,  1,  1,  1, 1,  1,
		              1, 1, 1,  1,  1,  1, 1,  1,
		              1, 1, 1,  1,  1,  1, 1,  1,
		              2, 2, 2,  2,  2,  2, 2,  2};


// Let's get this out of the way
int max(int a, int b) { return a >= b ? a : b; }

#define INVALIDADDR(addr) ((addr)<0 || (addr)>=4000)
#define ISTAPE(F) (TAPE0 <= (F) && (F) <= TAPE7)

// Construct the +-AA field of a word (1 sign bit and 12 address bits).
word ADDR(int addr) {
  int abs = addr < 0 ? -addr : addr;
  assert(abs < 1<<12);
  return abs | ((addr >= 0) << 12);
}

// Construct a (raw) word from the sign and its five bytes.
word WORD(bool sign, byte b1, byte b2, byte b3, byte b4, byte b5) {
  assert(b1 < 64);
  assert(b2 < 64);
  assert(b3 < 64);
  assert(b4 < 64);
  assert(b5 < 64);
  word w = b5;
  w |= b4 << 6;
  w |= b3 << 12;
  w |= b2 << 18;
  w |= b1 << 24;
  w |= sign << 30;
  return w;
}

// Construct an instruction word based on its fields.
word INSTR(word A, byte I, byte F, byte C) {
  assert(C < 1<<6);
  assert(F < 64);
  assert(I <= 6);
  assert(A <= 1<<13);
  word w = C;
  w |= F << 6;
  w |= I << 12;
  w |= A << 18;
  return w;
}

// Extract the individual fields from an instruction word.
word getA(word instr) { return (instr >> 18) & ONES(13); }
byte getI(word instr) { return (instr >> 12) & ONES(6); }
byte getF(word instr) { return (instr >> 6) & ONES(6); }
byte getC(word instr) { return instr & ONES(6); }
word getM(word instr, mix *mix);

int getinstrtime(int C, int F) {
  if (C == 7) return 1 + 2*F;
  return instrtimes[C];
}

// Check that the F-specification has the form A:B where 0<=A<=B<=5.
bool checkfieldspec(byte F) {
  int start = F/8, end = F%8;
  return 0 <= start && start <= end && end <= 5;
}

// Extract a portion of a word according to an F-specification.
word applyfield(word w, byte F) {
  assert(checkfieldspec(F));
  int start = F/8, end = F%8;
  bool sign = (w >> 30) & 1;
  // Last byte of v = end byte of w
  word v = w >> 6*(5-end);
  // Keep the desired bytes based on F
  v &= ONES(6 * (end - max(start,1) + 1));
  if (start == 0)
    return WITHSIGN(v, sign);
  else
    return POS(v);
}

// Carry out a "load word" operation.
void loadword(word *dest, word src) {
  *dest = src;
}

// Carry out a "store word" operation.
void storeword(word *dest, word src, byte F) {
  assert(checkfieldspec(F));
  int start = F/8, end = F%8;
  word mask = ONES(6 * (end - max(start,1) + 1));
  word touched_bit_positions = mask << 6*(5-end);
  word new_bits = (src & mask) << 6*(5-end);
  if (start == 0) {
    touched_bit_positions |= 1<<30;
    new_bits |= src & (1<<30);
  }
  word untouched_bit_positions = ONES(31) ^ touched_bit_positions;
  *dest &= untouched_bit_positions;  // Clear out the parts of *dest we are about to store
  *dest |= new_bits;
}

// Negate a word.
word negword(word w) {
  return WITHSIGN(w, !SIGN(w));
}

// Add two words and set the first operand to the result.
// Return true if there is overflow, false otherwise.
// The first operand is usually a register value, with which we want
// to carry out an ADD/INC/DEC instruction.
bool addword(word *dest, word src) {
  bool sign1 = SIGN(*dest);
  bool sign2 = SIGN(src);
  word w1 = MAG(*dest);
  word w2 = MAG(src);
  // Because the signed words are not stored using two's complement
  // notation, the addition has to be split into cases by sign.
  bool overflow;
  if (sign1 && sign2) {
    overflow = (w1+w2) >= 1<<30;
    *dest = POS(w1+w2);
  }
  else if (sign1 && !sign2) {
    overflow = false;
    if (w1 < w2)
      *dest = NEG(w2-w1);
    else
      *dest = POS(w1-w2);
  }
  else if (!sign1 && sign2) {
    overflow = false;
    if (w2 <= w1)
      *dest = NEG(w1-w2);
    else
      *dest = POS(w2-w1);
  }
  else if (!sign1 && !sign2) {
    overflow = (w1+w2) >= 1<<30;
    *dest = NEG(w1+w2);
  }
  return overflow;
}

// Return a pointer to:
// mix->A,              if i=0
// mix->I(i-1),         if 1<=i<=6
// mix->X,              if i=7
// mix->J,              if i=8.
//
// Used to conveniently implement the LOAD/STORE/JMP/INC/CMP families
// of instructions (essentially any instructions ending with A, X, J
// or a digit 1-6).
static word *regpointer(int i, mix *mix) {
  assert(0 <= i && i <= 8);
  return i == 0 ? &mix->A
       : i == 7 ? &mix->X
       : i == 8 ? &mix->J
       : &mix->Is[i-1];
}

// Return M = A-field + rIi, where I-field=i.
word getM(word instr, mix *mix) {
  // Manually cast the 13-bit A-field into a full word.
  word A = getA(instr);
  bool sign = (A >> 12) & 1;
  A = WITHSIGN(A & ONES(12), sign);
  // Add the contents of rIi if i>0
  byte i = getI(instr);
  assert(0 <= i && i <= 6);
  if (i > 0)
    addword(&A, *regpointer(i, mix));
  assert(MAG(A) < 1<<12);
  return A;
}

// Subtract two words and set the first operand to the result.
// Return true if there is overflow, false otherwise.
bool subword(word *dest, word src) {
  return addword(dest, negword(src));
}

// Mutiply *destA by src and set the combined 10-byte word
// (*destA, *destX) to the result.
// Used mainly for the MUL instruction.
void mulword(word *destA, word *destX, word src) {
  uint64_t prod = (uint64_t)MAG(*destA) * MAG(src);
  bool prodsign = SIGN(*destA) == SIGN(src);
  *destA = WITHSIGN((prod >> 30) & ONES(30), prodsign);
  *destX = WITHSIGN(prod & ONES(30),         prodsign);
}

// Divide the combined 10-byte word (*destA, *destX) by src; then set
// *destA to the quotient and *destX to the remainder.
// Return true if there is overflow, false otherwise.
// Used mainly for the DIV instruction.
bool divword(word *destA, word *destX, word src) {
  if (MAG(src) == 0)
    return true;
  uint64_t w = COMBINE(*destA, *destX);
  word quot = w / MAG(src);
  word rem  = w % MAG(src);
  if ((uint64_t)quot * MAG(src) + rem < w)
    return true;
  bool quotsign = SIGN(*destA) == SIGN(src);
  *destA = WITHSIGN(quot, quotsign);
  *destX = WITHSIGN(rem,  quotsign);
  return false;
}

// Return
//  1 if dest > src,
// -1 if dest < src,
//  0 if dest = src.
// Note that +0 = -0.
int compareword(word dest, word src) {
  bool sign1 = SIGN(dest);
  bool sign2 = SIGN(src);
  dest = MAG(dest);
  src = MAG(src);
  if (dest == 0 && src == 0) return 0;
  if (sign1 == sign2) {
    if (dest == src) return 0;
    if (dest > src) return sign1 ? 1 : -1;
    if (dest < src) return sign1 ? -1 : 1;
  }
  if (sign1 && !sign2) return 1;
  if (!sign1 && sign2) return -1;
}

// Shift *dest left by a specified number of bytes.
void shiftleftword(word *dest, int amt) {
  // We cap the shift amount, because shifting a value by more than
  // its width is undefined behaviour.
  if (amt > 5) amt = 5;
  word w = MAG(*dest);
  w = (w << amt*6) & ONES(30);
  *dest = WITHSIGN(w, SIGN(*dest));
}

// Similar to shiftleftword.
void shiftrightword(word *dest, int amt) {
  if (amt > 5) amt = 5;
  word w = MAG(*dest);
  w = (w >> amt*6) & ONES(30);
  *dest = WITHSIGN(w, SIGN(*dest));
}

// Shift the combined word (*destA, *destX) left by a specified number of bytes.
void shiftleftwords(word *destA, word *destX, int amt) {
  uint64_t w = COMBINE(*destA, *destX);
  if (amt > 10) amt = 10;
  w = (w << amt*6) & ONES(60);
  *destA = WITHSIGN((w >> 30) & ONES(30), SIGN(*destA));
  *destX = WITHSIGN(w & ONES(30),         SIGN(*destX));
}

// Similar to shiftleftwords.
void shiftrightwords(word *destA, word *destX, int amt) {
  uint64_t w = ((*destA & ONES(30)) << 30) | (*destX & ONES(30));
  if (amt > 10) amt = 10;
  w = (w >> amt*6) & ONES(60);
  *destA = WITHSIGN((w >> 30) & ONES(30), SIGN(*destA));
  *destX = WITHSIGN(w & ONES(30),         SIGN(*destX));
}

// Shift the combined word (*destA, *destX) circularly left by a
// specified number of bytes.
void shiftleftcirc(word *destA, word *destX, int amt) {
  uint64_t w = (MAG(*destA) << 30) | MAG(*destX);
  amt %= 10;
  w = (w << 6*amt) | ((w >> 60-6*amt) & ONES(60));
  w &= ONES(60);
  *destA = WITHSIGN(MAG(w >> 30), SIGN(*destA));
  *destX = WITHSIGN(MAG(w),       SIGN(*destX));
}

// Similar to shiftleftcirc.
void shiftrightcirc(word *destA, word *destX, int amt) {
  uint64_t w = (MAG(*destA) << 30) | MAG(*destX);
  amt %= 10;
  w = (w >> 6*amt) | ((w & ONES(6*amt)) << (60-6*amt));
  w &= ONES(60);
  *destA = WITHSIGN(MAG(w >> 30), SIGN(*destA));
  *destX = WITHSIGN(MAG(w),       SIGN(*destX));
}

// Apply the NUM instruction on the combined word (*destA, *destX).
void wordtonum(word *destA, word *destX) {
  word w = MAG(*destA);
  int d1  = ((w >> 24) & ONES(6)) % 10;
  int d2  = ((w >> 18) & ONES(6)) % 10;
  int d3  = ((w >> 12) & ONES(6)) % 10;
  int d4  = ((w >>  6) & ONES(6)) % 10;
  int d5  = ( w        & ONES(6)) % 10;
  w = MAG(*destX);
  int d6  = ((w >> 24) & ONES(6)) % 10;
  int d7  = ((w >> 18) & ONES(6)) % 10;
  int d8  = ((w >> 12) & ONES(6)) % 10;
  int d9  = ((w >>  6) & ONES(6)) % 10;
  int d10 = ( w        & ONES(6)) % 10;
  uint64_t num = d10 + d9*10 + d8*100 + d7*1000 + d6*10000 +
    d5*100000 + d4*1000000 + d3*10000000 + d2*100000000 + d1*1000000000;
  num &= ONES(30);
  *destA = WITHSIGN((word)num, SIGN(*destA));
}

// Apply the CHAR instruction, storing the character representation of
// the number *destA to the combined word (*destA, *destX).
void numtochar(word *destA, word *destX) {
  int x = MAG(*destA);
  byte b1  = 30 + x / 1000000000; x %= 1000000000;
  byte b2  = 30 + x /  100000000; x %=  100000000;
  byte b3  = 30 + x /   10000000; x %=   10000000;
  byte b4  = 30 + x /    1000000; x %=    1000000;
  byte b5  = 30 + x /     100000; x %=     100000;
  byte b6  = 30 + x /      10000; x %=      10000;
  byte b7  = 30 + x /       1000; x %=       1000;
  byte b8  = 30 + x /        100; x %=        100;
  byte b9  = 30 + x /         10; x %=         10;
  byte b10 = 30 + x;
  *destA = WORD(SIGN(*destA), b1, b2, b3, b4, b5);
  *destX = WORD(SIGN(*destX), b6, b7, b8, b9, b10);
}

// Convert a MIX character code to the actual character.
// Some characters are Unicode, and in that case the remaining
// codepoint data is set in extra.
unsigned char mixchr(byte b, unsigned char *extra) {
  *extra = '\0';
  switch (b) {
  case 0: return ' '; break;
  case 1: case 2: case 3: case 4: case 5:
  case 6: case 7: case 8: case 9:
    return 'A'+(b-1);
  case 10: *extra = 0x94; return 0xce;  // \u0394, Delta
  case 11: case 12: case 13: case 14: case 15:
  case 16: case 17: case 18: case 19:
    return 'J'+(b-11);
  case 20: *extra = 0xa3; return 0xce;  // \u03a3, Sigma
  case 21: *extra = 0xa0; return 0xce;  // \u03a0, Pi
  case 22: case 23: case 24: case 25: case 26:
  case 27: case 28: case 29:
    return 'S'+(b-22);
  case 30: case 31: case 32: case 33: case 34:
  case 35: case 36: case 37: case 38: case 39:
    return '0'+(b-30);
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
  case 56: return 'a';
  case 57: return 'b';
  case 58: return 'c';
  case 59: return 'd';
  case 60: return 'e';
  case 61: return 'f';
  case 62: return 'g';
  case 63: return 'h';
  default: assert(false);
  }
}

// Get a MIX character's code.
// Note that !, [, ] are used in place of their Unicode counterparts
// Delta, Sigma, Pi.
// I also added character codes 56-63 for a-h.
byte mixord(char c) {
  switch (c) {
  case ' ': return 0;
  // My custom non-unicode substitutes for Delta/Sigma/Pi
  case '!': return 10;
  case '[': return 20;
  case ']': return 21;
  case 'A': case 'B': case 'C': case 'D': case 'E':
  case 'F': case 'G': case 'H': case 'I':
    return 1+c-'A';
  case 'J': case 'K': case 'L': case 'M': case 'N':
  case 'O': case 'P': case 'Q': case 'R':
    return 11+c-'J';
  case 'S': case 'T': case 'U': case 'V': case 'W':
  case 'X': case 'Y': case 'Z':
    return 22+c-'S';
  case '0': case '1': case '2': case '3': case '4':
  case '5': case '6': case '7': case '8': case '9':
    return 30+c-'0';
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
  case 'a': return 56;
  case 'b': return 57;
  case 'c': return 58;
  case 'd': return 59;
  case 'e': return 60;
  case 'f': return 61;
  case 'g': return 62;
  case 'h': return 63;
  default: assert(false);
  }
}

// Initialise the MIX machine.
void initmix(mix *mix) {
  mix->done = false;
  mix->exitcode = 0;
  mix->err = "";

  mix->PC = 0;

  for (int i = 0; i < 4000; i++) {
    mix->exectimes[i] = 0;
    mix->execcounts[i] = 0;
    mix->mem[i] = POS(0);
    mix->modified[i] = false;
  }

  mix->overflow = false;
  mix->cmp = 0;
  mix->A = POS(0);
  mix->X = POS(0);
  for (int i = 0; i < 6; i++)
    mix->Is[i] = POS(0);
  mix->J = POS(0);

  mix->cardfile = NULL;
  for (int i = 0; i < 8; i++)
    mix->tapefiles[i] = NULL;

  for (int i = 0; i < 21; i++) {
    mix->iotasks[i].totaltime = 0;
    mix->iotasks[i].timer = 0;
    mix->iotasks[i].err = "";
  }
}

static bool readcard(IOtask *iotask, mix *mix, word M) {
  if (mix->cardfile == NULL) {
    iotask->err = "unspecified card file";
    return false;
  }

  for (int i = 0; i < 16*5; i++) {
    char c;
    if ((c = fgetc(mix->cardfile)) == EOF)  // If we encountered EOF halfway through the card,
      c = ' ';                              // fill in the remaining words with 0s.
    if (c == '\n') {  // Ignore newlines
      i--;
      continue;
    }

    int addr = INT(M)+i/5;
    if (INVALIDADDR(addr)) {
      iotask->err = "illegal address while reading card";
      return false;
    }

    int pos = i%5 + 1;
    // Store the character in the appropriate byte of the address.
    storeword(&mix->mem[addr], mixord(c), pos*8 + pos);
  }
  return true;
}

static bool printline(IOtask *iotask, mix *mix, word M) {
  // Each line has 120 characters (plus the trailing \n\0), a
  // character may need 2 bytes to encode (for the codepoints
  // 10=Delta, 20=Sigma, 21=Pi).
  unsigned char line[120*2+2];
  int c = 0;
  unsigned char extra;

  for (int i = 0; i < 24; i++) {
    int addr = INT(M)+i;
    if (INVALIDADDR(addr)) {
      iotask->err = "illegal address while printing line";
      return false;
    }

    // Add the characters in the word to the line buffer.
    word w = mix->mem[addr];
#define _ADDCHAR(b) line[c++] = mixchr((b), &extra);    \
                    if (extra) line[c++] = extra;
    byte b1 = (w >> 24) & ONES(6); _ADDCHAR(b1)
    byte b2 = (w >> 18) & ONES(6); _ADDCHAR(b2)
    byte b3 = (w >> 12) & ONES(6); _ADDCHAR(b3)
    byte b4 = (w >>  6) & ONES(6); _ADDCHAR(b4)
    byte b5 =  w        & ONES(6); _ADDCHAR(b5)
#undef _ADDCHAR
  }
  line[c++] = '\n'; line[c++] = '\0';
  printf(line);
  return true;
}

bool controltape(IOtask *iotask, mix *mix, word M, int unit) {
  FILE *fp = mix->tapefiles[unit];
  if (fp == NULL) {
    iotask->err = "unspecified tape file";
    return false;
  }

  // We should be at the start of a tape block.
  // (A tape block is represented by 601 characters -- 6 for each word
  // and then a final \n.)
  long curpos = ftell(fp);
  assert(curpos % 601L == 0);

  if (INT(M) == 0) // Rewind to start of tape
    fseek(fp, 0L, SEEK_SET);
  else {           // Move tape forward or backward
    long newpos = curpos + 601L*INT(M);
    if (newpos < 0)
      fseek(fp, 0L, SEEK_SET);
    else if (newpos >= 601L*1000) {
      iotask->err = "end of tape reached";
      return false;
    }
    else
      fseek(fp, newpos, SEEK_SET);
  }

  assert(ftell(fp) % 601L == 0);
  return true;
}

bool readtape(IOtask *iotask, mix *mix, word M, int unit) {
  FILE *fp = mix->tapefiles[unit];
  if (fp == NULL) {
    iotask->err = "unspecified tape file";
    return false;
  }

  // We should be at the start of a tape block.
  assert(ftell(fp) % 601L == 0);

  for (int i = 0; i < 600; i++) {
    char c;
    if ((c = fgetc(fp)) == EOF) {
      iotask->err = "unexpected EOF in middle of tape";
      return false;
    }

    int addr = INT(M)+i/6;
    if (INVALIDADDR(addr)) {
      iotask->err = "illegal address while reading from tape";
      return false;
    }

    if (i%6 == 0) {  // Do we load a sign?
      if (c == '#')
	mix->mem[addr] = POS(0);
      else if (c == '~')
	mix->mem[addr] = NEG(0);
      else {
	iotask->err = "invalid sign in tape, should be # or ~";
	return false;
      }
    }
    else {           // or load a byte?
      int pos = i%6;
      // Store the character in the appropriate byte of the address.
      storeword(&mix->mem[addr], mixord(c), pos*8 + pos);
    }
  }
  assert(fgetc(fp) == '\n');  // Last character of block is a newline
  assert(ftell(fp) % 601L == 0);
  return true;
}

// A small wrapper around fputc, taking into account the Unicode
// characters in the MIX character set.
static void _writechar(unsigned char c, unsigned char extra, FILE *fp) {
  if (extra) {
    if (extra == 0x94) fputc('!', fp);
    else if (extra == 0xa3) fputc('[', fp);
    else if (extra == 0xa0) fputc(']', fp);
    else assert(false);
  }
  else
    fputc(c, fp);
}

bool writetape(IOtask *iotask, mix *mix, word M, int unit) {
  FILE *fp = mix->tapefiles[unit];
  if (fp == NULL) {
    iotask->err = "unspecified tape file";
    return false;
  }

  // We should be at the start of a tape block.
  assert(ftell(fp) % 601 == 0);

  unsigned char c, extra;
  for (int i = 0; i < 100; i++) {
    int addr = INT(M)+i;
    if (INVALIDADDR(addr)) {
      iotask->err = "illegal address while writing to tape";
      return false;
    }

    // Write the word to the tape file.
    word w = mix->mem[addr];
    _writechar(SIGN(w) ? '#' : '~', '\0', fp);
#define _WRITECHAR(b) c = mixchr((b), &extra);	\
                     _writechar(c, extra, fp);
    byte b1 = (w >> 24) & ONES(6); _WRITECHAR(b1)
    byte b2 = (w >> 18) & ONES(6); _WRITECHAR(b2)
    byte b3 = (w >> 12) & ONES(6); _WRITECHAR(b3)
    byte b4 = (w >>  6) & ONES(6); _WRITECHAR(b4)
    byte b5 =  w        & ONES(6); _WRITECHAR(b5)
#undef _WRITECHAR
  }
  _writechar('\n', '\0', fp);
  assert(ftell(fp) % 601 == 0);
  return true;
}

// Do the actual IO transmission (the corresponding IO instruction
// would have been dispatched many cycles ago).
// Return false if there was an error carrying out the IO operation.
static bool do_io_transmission(IOtask *iotask, mix *mix) {
  word M = iotask->M;
  word F = iotask->F;
  word C = iotask->C;

  if (F == CARDREADER)
    return readcard(iotask, mix, M);
  else if (F == LINEPRINTER && C == OUT)
    return printline(iotask, mix, M);
  else if (F == LINEPRINTER && C == IOC)  // So far, IOC on a line printer does nothing
    return true;
  else if (ISTAPE(F) && C == IOC)
    return controltape(iotask, mix, M, F);
  else if (ISTAPE(F) && C == IN)
    return readtape(iotask, mix, M, F);
  else if (ISTAPE(F) && C == OUT)
    return writetape(iotask, mix, M, F);
  assert(false);
}

void onestep(mix *mix) {
  if (mix->done) return;

#define _STOP(error) {             			\
  mix->done = true;                                     \
  mix->err = error;                                     \
  return;                                               \
}
#define _CHECKADDR(i)                                   \
if ((i)<0 || (i)>=4000) {                               \
  _STOP("illegal address");                             \
}
#define _FIELDSPEC(C)                                   \
if (!checkfieldspec(F)) {				\
  _STOP("invalid field for " #C);                       \
}

  _CHECKADDR(mix->PC)
  word instr = mix->mem[mix->PC];
  byte C = getC(instr);
  byte F = getF(instr);
  byte I = getI(instr);
  word M = getM(instr, mix);
  // We don't want to evaluate V straight away, because INT(M) may not
  // be a valid address for instructions like ENTA
#define V() applyfield(mix->mem[INT(M)], F)

  if (C < 0 || C >= 64)
    _STOP("invalid instruction")
  int instrtime = instrtimes[C];
  bool advancePC = true;

  if (C == NOP) {}

  else if (C == ADD) {
    _FIELDSPEC("ADD")
    _CHECKADDR(INT(M))
    mix->overflow = addword(&mix->A, V());
  }

  else if (C == SUB) {
    _FIELDSPEC("SUB")
    _CHECKADDR(INT(M))
    mix->overflow = subword(&mix->A, V());
  }

  else if (C == MUL) {
    _FIELDSPEC("MUL")
    _CHECKADDR(INT(M))
    mulword(&mix->A, &mix->X, V());
  }

  else if (C == DIV) {
    _FIELDSPEC("DIV")
    _CHECKADDR(INT(M))
    mix->overflow = divword(&mix->A, &mix->X, V());
  }

  else if (C == SPEC) {
    if (F == 0)                                 // NUM
      wordtonum(&mix->A, &mix->X);

    else if (F == 1)                            // CHAR
      numtochar(&mix->A, &mix->X);

    else if (F == 2) {                          // HLT
      // Complete remaining IO tasks before returning
      for (int i = 0; i < 21; i++) {
	IOtask *iotask = &mix->iotasks[i];
	if (iotask->timer > iotask->totaltime/2)  // IO transmission not done yet?
	  if (!do_io_transmission(iotask, mix))
	    _STOP(iotask->err)
	iotask->timer = 0;
      }
      mix->exitcode = INT(M);
      _STOP("")
    }

    else
      _STOP("invalid field for SPECIAL")
  }

  else if (C == SHIFT) {
    if (F == 0)
      shiftleftword(&mix->A, INT(M));             // SLA
    else if (F == 1)
      shiftrightword(&mix->A, INT(M));            // SRA
    else if (F == 2)
      shiftleftwords(&mix->A, &mix->X, INT(M));   // SLAX
    else if (F == 3)
      shiftrightwords(&mix->A, &mix->X, INT(M));  // SRAX
    else if (F == 4)
      shiftleftcirc(&mix->A, &mix->X, INT(M));    // SLC
    else if (F == 5)
      shiftrightcirc(&mix->A, &mix->X, INT(M));   // SRC
    else
      _STOP("invalid field for SHIFT")
  }

  else if (C == MOVE) {
    instrtime = 1 + 2*F;
    for (int i = 0; i < F; i++) {
      _CHECKADDR(INT(M)+i)
      _CHECKADDR(INT(mix->Is[0]))
      mix->mem[INT(mix->Is[0])] = mix->mem[INT(M)+i];
      mix->Is[0]++;
    }
  }

  else if (LDA <= C && C <= LDX) {
    _FIELDSPEC("LOAD")
    _CHECKADDR(INT(M))
    loadword(regpointer(C-LDA, mix), V());
  }

  else if (LDAN <= C && C <= LDXN) {
    _FIELDSPEC("LOADNEG")
    _CHECKADDR(INT(M))
    loadword(regpointer(C-LDAN, mix), negword(V()));
    // If sign is not part of F, make the word negative.
    if ((F>>3) & ONES(3) >= 1) {
      word *reg = regpointer(C-LDAN, mix);
      *reg = NEG(*reg);
    }
  }

  else if (STA <= C && C <= STJ) {
    _FIELDSPEC("STORE")
    _CHECKADDR(INT(M))
    mix->modified[INT(M)] = true;
    storeword(&mix->mem[INT(M)], *regpointer(C-STA, mix), F);
  }

  else if (C == STZ) {
    _FIELDSPEC("STZ")
    _CHECKADDR(INT(M))
    storeword(&mix->mem[INT(M)], POS(0), F);
  }

  else if (C == JBUS) {
    if (ISTAPE(F) || F == CARDREADER || F == LINEPRINTER) {
      if (mix->iotasks[F].timer > 0) {
	mix->J = POS(mix->PC+1);
	mix->execcounts[mix->PC]++;
	mix->exectimes[mix->PC] += instrtime;
	mix->PC = INT(M);
	advancePC = false;
      }
    }
    else
      _STOP("invalid device number for JBUS")
  }

  else if (C == IOC || C == IN || C == OUT) {
    // Check that device number is valid
    if (F < 0 || F >= 21 ||
	(C == IOC && !(ISTAPE(F) || F == LINEPRINTER)) ||
	(C == IN  && !(ISTAPE(F) || F == CARDREADER)) ||
	(C == OUT && !(ISTAPE(F) || F == LINEPRINTER))) {
      if (C == IOC)
	_STOP("invalid device number for IOC")
      else if (C == IN)
	_STOP("invalid device number for IN")
      else if (C == OUT)
	_STOP("invalid device number for OUT")
    }

    else {
      /*
      if (C == IOC)
	printf("INFO: Device %d: IOC %d\n", F, INT(M));
      else if (C == IN)
	printf("INFO: Device %d: IN to %d\n", F, INT(M));
      else if (C == OUT)
	printf("INFO: Device %d: OUT from %d\n", F, INT(M));
      */

      IOtask prev_iotask = mix->iotasks[F];
      // If IO device is currently busy, we will fast forward to when
      // it is ready (and do the transmission if it hasn't been done
      // yet).
      if (prev_iotask.timer > prev_iotask.totaltime/2)
	do_io_transmission(&prev_iotask, mix);
      // To simulate the fast forwarding effect, we simply update the
      // other devices' timers...
      for (int i = 0; i < 21; i++)
	mix->iotasks[i].timer -= prev_iotask.timer;
      // ... and add to execution times.
      mix->exectimes[mix->PC] += prev_iotask.timer;

      // Set the arguments for current IO control operation.
      mix->iotasks[F].M = M;
      mix->iotasks[F].F = F;
      mix->iotasks[F].C = C;
      mix->iotasks[F].err = "";
      if (C == IOC) {
	mix->iotasks[F].totaltime = mix->IOCtimes[F];
	mix->iotasks[F].timer = mix->IOCtimes[F];
      }
      else if (C == IN) {
	mix->iotasks[F].totaltime = mix->INtimes[F];
	mix->iotasks[F].timer = mix->INtimes[F];
      }
      else if (C == OUT) {
	mix->iotasks[F].totaltime = mix->OUTtimes[F];
	mix->iotasks[F].timer = mix->OUTtimes[F];
      }
    }
  }

  else if (C == JRED) {
    if (ISTAPE(F) || F == CARDREADER || F == LINEPRINTER) {
      if (mix->iotasks[F].timer == 0) {
	mix->J = POS(mix->PC+1);
	mix->execcounts[mix->PC]++;
	mix->exectimes[mix->PC] += instrtime;
	mix->PC = INT(M);
	advancePC = false;
      }
    }
    else
      _STOP("invalid device number for JRED")
  }

  else if (C == JMP) {
    if (F == 0 || F == 1 ||                     // JMP/JSJ
	(F == 2 && mix->overflow)  ||           // JOV
	(F == 3 && !mix->overflow) ||           // JNOV
	(F == 4 && mix->cmp < 0)   ||           // JL
	(F == 5 && mix->cmp == 0)  ||           // JE
	(F == 6 && mix->cmp > 0)   ||           // JG
	(F == 7 && mix->cmp >= 0)  ||           // JGE
	(F == 8 && mix->cmp != 0)  ||           // JNE
	(F == 9 && mix->cmp <= 0)) {            // JLE
      if (F != 1)
	mix->J = POS(mix->PC+1);
      _CHECKADDR(INT(M))
      mix->execcounts[mix->PC]++;
      mix->exectimes[mix->PC] += instrtime;
      mix->PC = INT(M);
      advancePC = false;
    }
    else if (F >= 10)
      _STOP("invalid field for JUMP")
  }

  else if (JMPA <= C && C <= JMPX) {
    word w = *regpointer(C-JMPA, mix);
    if ((F == 0 && !SIGN(w) && MAG(w) > 0)   ||   // JxN
	(F == 1 && MAG(w) == 0)              ||   // JxZ
	(F == 2 && SIGN(w) && MAG(w) > 0)    ||   // JxP
	(F == 3 && (SIGN(w) || MAG(w) == 0)) ||   // JxNN
	(F == 4 && MAG(w) != 0)              ||   // JxNZ
	(F == 5 && (!SIGN(w) || MAG(w) == 0))) {  // JxNP
      mix->execcounts[mix->PC]++;
      mix->exectimes[mix->PC] += instrtime;
      mix->PC = INT(M);
      mix->J = POS(mix->PC+1);
      advancePC = false;
    }
    else if (F >= 6)
      _STOP("invalid field for REGJUMP")
  }

  else if (INCA <= C && C <= INCX) {
    if (F == 0)                                 // INCx
      mix->overflow = addword(regpointer(C-INCA, mix), M);
    else if (F == 1)                            // DECx
      mix->overflow = subword(regpointer(C-INCA, mix), M);
    else if (F == 2)                            // ENTx
      *regpointer(C-INCA, mix) = M;
    else if (F == 3)                            // ENNx
      *regpointer(C-INCA, mix) = negword(M);
    else
      _STOP("invalid field for ADDROP")
  }

  else if (CMPA <= C && C <= CMPX) {
    _FIELDSPEC("CMP")
    _CHECKADDR(INT(M))
    mix->cmp = compareword(applyfield(*regpointer(C-56, mix), F), V());
  }

#define MORE_THAN_TWO_BYTES(w) (MAG(w)>>12 != 0)
  for (int i = 0; i < 6; i++) {
    if (MORE_THAN_TWO_BYTES(mix->Is[i])) {
      mix->done = true;
      if (i == 0)
	mix->err = "rI1 contains more than two bytes";
      else if (i == 1)
	mix->err = "rI2 contains more than two bytes";
      else if (i == 2)
	mix->err = "rI3 contains more than two bytes";
      else if (i == 3)
	mix->err = "rI4 contains more than two bytes";
      else if (i == 4)
	mix->err = "rI5 contains more than two bytes";
      else if (i == 5)
	mix->err = "rI6 contains more than two bytes";
    }
  }
  if (MORE_THAN_TWO_BYTES(mix->J))
    _STOP("rJ contains more than two bytes")

  // Check up on each IO task to see if we need to do transmission
  for (int i = 0; i < 21; i++) {
    IOtask *iotask = &mix->iotasks[i];
    if (iotask->timer > iotask->totaltime/2 &&
	iotask->timer - instrtime <= iotask->totaltime/2) {
      if (!do_io_transmission(iotask, mix))
	_STOP(iotask->err)
    }
    iotask->timer -= instrtime;
    if (iotask->timer < 0)
      iotask->timer = 0;
  }
#undef _STOP
#undef _CHECKADDR
#undef _FIELDSPEC

  if (mix->done) {
    // Flush the tape files
    for (int i = 0; i < 7; i++) {
      if (mix->tapefiles[i] != NULL)
	fflush(mix->tapefiles[i]);
    }
  }

  if (advancePC) {
    mix->execcounts[mix->PC]++;
    mix->exectimes[mix->PC] += instrtime;
    mix->PC++;
  }
}