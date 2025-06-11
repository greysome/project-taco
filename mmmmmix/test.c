#include "emulator.h"
#include "assembler.h"

extern bool controltape(IOtask *, mix *, word, int);
extern bool readtape(IOtask *, mix *, word, int);
extern bool writetape(IOtask *, mix *, word, int);

void testemulator() {
  mix mix;

  // TEST: bitwise representation of MIX words
  assert(WORD(true, 1, 2, 3, 4, 5)       == 0b01000001000010000011000100000101);
  //                                           +     1     2     3     4     5
  assert(WORD(false, 63, 62, 61, 60, 59) == 0b00111111111110111101111100111011);
  //                                           -    63    62    61    60    59

  // TEST: bitwise representation of wordesses
  assert(ADDR(1)     == 0b0001000000000001);
  //                         +           1
  assert(ADDR(-4095) == 0b0000111111111111);
  //                         -        4096

  // TEST: bitwise representation of MIX instructions
  word w = INSTR(ADDR(2000), 2, 3, 8);
  //                  LDA 2000,2(0:3)
  assert(w == 0b01011111010000000010000011001000);
  //             +        2000     2     3     8

  // TEST: fields of a MIX instruction
  assert(getA(w) == 0b0001011111010000);
  //                     +        2000
  assert(getI(w) == 0b000010);
  //                       2
  assert(getF(w) == 0b000011);
  //                    0  3
  assert(getC(w) == 0b001000);
  //                       8
  mix.Is[1] = POS(5);
  assert(getM(w,&mix) == POS(2005));
  mix.Is[1] = NEG(2005);
  assert(getM(w,&mix) == NEG(5));

  // TEST: loading words
  // Examples taken from TAOCP vol 1, p129
  mix.mem[1000] = WORD(false, 1, 2, 3, 4, 5);

  loadword(&mix.A, mix.mem[1000]);
  assert(mix.A == WORD(false, 1, 2, 3, 4, 5));
  loadword(&mix.A, applyfield(mix.mem[1000], 13));
  assert(mix.A == WORD(true, 1, 2, 3, 4, 5));
  loadword(&mix.A, applyfield(mix.mem[1000], 29));
  assert(mix.A == WORD(true, 0, 0, 3, 4, 5));
  loadword(&mix.A, applyfield(mix.mem[1000], 3));
  assert(mix.A == WORD(false, 0, 0, 1, 2, 3));
  loadword(&mix.Is[0], applyfield(mix.mem[1000], 36));
  assert(mix.Is[0] == WORD(true, 0, 0, 0, 0, 4));
  loadword(&mix.Is[0], applyfield(mix.mem[1000], 0));
  assert(mix.Is[0] == WORD(false, 0, 0, 0, 0, 0));
  loadword(&mix.Is[0], applyfield(mix.mem[1000], 9));
  assert(mix.Is[0] == WORD(true, 0, 0, 0, 0, 1));

  // TEST: storing words
  // Examples taken from TAOCP vol 1, p130
  mix.A         = WORD(true, 6, 7, 8, 9, 0);
  mix.mem[1000] = WORD(false, 1, 2, 3, 4, 5);
  storeword(&mix.mem[1000], mix.A, 5);
  assert(mix.mem[1000] == WORD(true, 6, 7, 8, 9, 0));

  mix.mem[1000] = WORD(false, 1, 2, 3, 4, 5);
  storeword(&mix.mem[1000], mix.A, 13);
  assert(mix.mem[1000] == WORD(false, 6, 7, 8, 9, 0));

  mix.mem[1000] = WORD(false, 1, 2, 3, 4, 5);
  storeword(&mix.mem[1000], mix.A, 45);
  assert(mix.mem[1000] == WORD(false, 1, 2, 3, 4, 0));

  mix.mem[1000] = WORD(false, 1, 2, 3, 4, 5);
  storeword(&mix.mem[1000], mix.A, 18);
  assert(mix.mem[1000] == WORD(false, 1, 0, 3, 4, 5));

  mix.mem[1000] = WORD(false, 1, 2, 3, 4, 5);
  storeword(&mix.mem[1000], mix.A, 19);
  assert(mix.mem[1000] == WORD(false, 1, 9, 0, 4, 5));

  mix.mem[1000] = WORD(false, 1, 2, 3, 4, 5);
  storeword(&mix.mem[1000], mix.A, 1);
  assert(mix.mem[1000] == WORD(true, 0, 2, 3, 4, 5));

  // TEST: addition of words
  // 1. Both positive, no overflow
  mix.A = WORD(true, 19, 18, 1, 2, 22);
  w     = WORD(true,  1, 36, 5, 0, 50);
  bool overflow = addword(&mix.A, w);
  assert(!overflow);
  assert(mix.A == WORD(true, 20, 54, 6, 3, 8));

  // 2. Both positive, overflow
  mix.A = WORD(true, 19, 18, 1, 2, 22);
  w     = WORD(true, 50, 36, 5, 0, 50);
  overflow = addword(&mix.A, w);
  assert(overflow);
  assert(mix.A == WORD(true, 5, 54, 6, 3, 8));

  // 4. Both negative, overflow
  mix.A = WORD(false, 19, 18, 1, 2, 22);
  w     = WORD(false, 50, 36, 5, 0, 50);
  overflow = addword(&mix.A, w);
  assert(overflow);
  assert(mix.A == WORD(false, 5, 54, 6, 3, 8));

  // 5. X-Y, X>Y
  mix.A = WORD( true, 19, 18, 1, 2, 22);
  w     = WORD(false,  1, 36, 5, 0, 50);
  overflow = addword(&mix.A, w);
  assert(!overflow);
  assert(mix.A == WORD(true, 17, 45, 60, 1, 36));

  // 6. X-Y, X<Y
  mix.A = WORD( true, 19, 18, 1, 2, 22);
  w     = WORD(false, 50, 36, 5, 0, 50);
  overflow = addword(&mix.A, w);
  assert(!overflow);
  assert(mix.A == WORD(false, 31, 18, 3, 62, 28));

  // 7. -X+Y, X>Y
  mix.A = WORD(false, 19, 18, 1, 2, 22);
  w     = WORD( true,  1, 36, 5, 0, 50);
  overflow = addword(&mix.A, w);
  assert(!overflow);
  assert(mix.A == WORD(false, 17, 45, 60, 1, 36));

  // 8. -X+Y, X<Y
  mix.A = WORD(false, 19, 18, 1, 2, 22);
  w     = WORD( true, 50, 36, 5, 0, 50);
  overflow = addword(&mix.A, w);
  assert(!overflow);
  assert(mix.A == WORD(true, 31, 18, 3, 62, 28));

  // 9. Partial fields
  mix.A = WORD( true, 19, 18, 1, 2, 22);
  w     = WORD(false,  1, 36, 5, 0, 50);
  overflow = addword(&mix.A, applyfield(w, 27));
  assert(!overflow);
  assert(mix.A == WORD(true, 19, 18, 1, 2, 27));

  // 10. Result = 0, sign of A should be unchanged
  mix.A = WORD( true, 0, 0, 0, 0, 5);
  w     = WORD(false, 0, 0, 0, 0, 5);
  addword(&mix.A, w);
  assert(mix.A == WORD(true, 0, 0, 0, 0, 0));

  mix.A = WORD(false, 0, 0, 0, 0, 5);
  w     = WORD( true, 0, 0, 0, 0, 5);
  addword(&mix.A, w);
  assert(mix.A == WORD(false, 0, 0, 0, 0, 0));

  mix.A = WORD(true, 63, 63, 63, 63, 63);
  w     = WORD(true,  0,  0,  0,  0, 1);
  addword(&mix.A, w);
  assert(mix.A == WORD(true, 0, 0, 0, 0, 0));

  mix.A = WORD(false, 63, 63, 63, 63, 63);
  w     = WORD(false,  0,  0,  0,  0,  1);
  addword(&mix.A, w);
  assert(mix.A == WORD(false, 0, 0, 0, 0, 0));

  // TEST: negating words
  w = WORD(true, 1, 2, 3, 4, 5);
  assert(negword(w) == WORD(false, 1, 2, 3, 4, 5));

  // TEST: subtracting words
  mix.A = WORD(false, 19, 18, 1, 2, 22);
  w     = WORD(false, 50, 36, 5, 0, 50);
  overflow = subword(&mix.A, w);
  assert(!overflow);
  assert(mix.A == WORD(true, 31, 18, 3, 62, 28));

  // TEST: multiplication of words
  mix.A = WORD(false, 50, 0, 1, 48, 4);
  w     = WORD(false,  2, 0, 0,  0, 0);
  mulword(&mix.A, &mix.X, w);
  assert(mix.A == WORD(true, 1, 36, 0, 3, 32));
  assert(mix.X == WORD(true, 8, 0, 0, 0, 0));

  mix.A = WORD(false, 0, 0, 0, 1, 48);
  w     = WORD(false, 2, 9, 9, 9,  9);
  mulword(&mix.A, &mix.X, applyfield(w, 9));
  assert(mix.A == WORD(false, 0, 0, 0, 0, 0));
  assert(mix.X == WORD(false, 0, 0, 0, 3, 32));

  // TEST: division of words
  mix.A = NEG(0);
  mix.X = POS(17);
  w = POS(3);
  overflow = divword(&mix.A, &mix.X, w);
  assert(!overflow);
  assert(mix.A == NEG(5));
  assert(mix.X == NEG(2));

  mix.A = POS(1);
  mix.X = POS(2);
  w = POS(0);
  overflow = divword(&mix.A, &mix.X, w);
  assert(overflow);

  mix.A = POS(5000);
  mix.X = POS(0);
  w = POS(1);
  overflow = divword(&mix.A, &mix.X, w);
  assert(overflow);

  // TEST: comparing words
  w      = WORD(false, 1, 2, 3, 4, 6);
  word v = WORD( true, 1, 2, 3, 4, 5);
  assert(compareword(v, w) == 1);
  assert(compareword(w, v) == -1);
  assert(compareword(applyfield(v, 13), applyfield(w, 13)) == -1);
  assert(compareword(applyfield(v, 12), applyfield(w, 12)) == 0);
  assert(compareword(applyfield(v, 4), applyfield(w, 4)) == 1);
  assert(compareword(applyfield(v, 0), applyfield(w, 0)) == 0);

  // TEST: word to num
  mix.A = WORD(false,  0,  0, 31, 32, 39);
  mix.X = WORD( true, 37, 57, 47, 30, 30);
  wordtonum(&mix.A, &mix.X);
  assert(mix.A == NEG(12977700));
  assert(mix.X = WORD(true, 37, 57, 47, 30, 30));

  // TEST: num to char
  numtochar(&mix.A, &mix.X);
  assert(mix.A == WORD(false, 30, 30, 31, 32, 39));
  assert(mix.X == WORD( true, 37, 37, 37, 30, 30));

  // TEST: xor
  mix.A = WORD(false, 0, 10, 20, 30, 40);
  w = WORD(true, 63, 32, 16, 8, 4);
  xor(&mix.A, w);
  assert(mix.A = WORD(false, 63, 42, 4, 22, 44));

  // TEST: shifting words
  mix.A = WORD(true, 1, 2, 3, 4, 5);
  shiftleftword(&mix.A, 0);
  assert(mix.A == WORD(true, 1, 2, 3, 4, 5));

  mix.A = WORD(true, 1, 2, 3, 4, 5);
  shiftleftword(&mix.A, 2);
  assert(mix.A == WORD(true, 3, 4, 5, 0, 0));

  mix.A = WORD(true, 1, 2, 3, 4, 5);
  shiftleftword(&mix.A, 6);
  assert(mix.A == WORD(true, 0, 0, 0, 0, 0));

  mix.A = WORD(true, 1, 2, 3, 4, 5);
  shiftrightword(&mix.A, 1);
  assert(mix.A == WORD(true, 0, 1, 2, 3, 4));

  mix.A = WORD(true, 1, 2, 3, 4, 5);
  shiftrightword(&mix.A, 3);
  assert(mix.A == WORD(true, 0, 0, 0, 1, 2));

  mix.A = WORD(true, 1, 2, 3, 4, 5);
  shiftrightword(&mix.A, 5);
  assert(mix.A == WORD(true, 0, 0, 0, 0, 0));

  mix.A = WORD( true, 1, 2, 3, 4, 5);
  mix.X = WORD(false, 6, 7, 8, 9, 10);
  shiftleftwords(&mix.A, &mix.X, 1);
  assert(mix.A == WORD(true, 2, 3, 4, 5, 6));
  assert(mix.X == WORD(false, 7, 8, 9, 10, 0));

  mix.A = WORD( true, 1, 2, 3, 4, 5);
  mix.X = WORD(false, 6, 7, 8, 9, 10);
  shiftleftwords(&mix.A, &mix.X, 5);
  assert(mix.A == WORD(true, 6, 7, 8, 9, 10));
  assert(mix.X == WORD(false, 0, 0, 0, 0, 0));

  mix.A = WORD( true, 1, 2, 3, 4, 5);
  mix.X = WORD(false, 6, 7, 8, 9, 10);
  shiftleftwords(&mix.A, &mix.X, 11);
  assert(mix.A == WORD(true, 0, 0, 0, 0, 0));
  assert(mix.X == WORD(false, 0, 0, 0, 0, 0));

  mix.A = WORD( true, 1, 2, 3, 4, 5);
  mix.X = WORD(false, 6, 7, 8, 9, 10);
  shiftrightwords(&mix.A, &mix.X, 2);
  assert(mix.A == WORD(true, 0, 0, 1, 2, 3));
  assert(mix.X == WORD(false, 4, 5, 6, 7, 8));

  mix.A = WORD( true, 1, 2, 3, 4, 5);
  mix.X = WORD(false, 6, 7, 8, 9, 10);
  shiftrightwords(&mix.A, &mix.X, 8);
  assert(mix.A == WORD(true, 0, 0, 0, 0, 0));
  assert(mix.X == WORD(false, 0, 0, 0, 1, 2));

  mix.A = WORD(true, 1, 2, 3, 4, 5);
  mix.X = WORD(false, 6, 7, 8, 9, 10);
  shiftleftcirc(&mix.A, &mix.X, 0);
  assert(mix.A == WORD(true, 1, 2, 3, 4, 5));
  assert(mix.X == WORD(false, 6, 7, 8, 9, 10));

  mix.A = WORD( true, 1, 2, 3, 4, 5);
  mix.X = WORD(false, 6, 7, 8, 9, 10);
  shiftleftcirc(&mix.A, &mix.X, 1);
  assert(mix.A == WORD(true, 2, 3, 4, 5, 6));
  assert(mix.X == WORD(false, 7, 8, 9, 10, 1));

  mix.A = WORD( true, 1, 2, 3, 4, 5);
  mix.X = WORD(false, 6, 7, 8, 9, 10);
  shiftleftcirc(&mix.A, &mix.X, 5);
  assert(mix.A == WORD(true, 6, 7, 8, 9, 10));
  assert(mix.X == WORD(false, 1, 2, 3, 4, 5));

  mix.A = WORD( true, 1, 2, 3, 4, 5);
  mix.X = WORD(false, 6, 7, 8, 9, 10);
  shiftleftcirc(&mix.A, &mix.X, 33);
  assert(mix.A == WORD(true, 4, 5, 6, 7, 8));
  assert(mix.X == WORD(false, 9, 10, 1, 2, 3));

  mix.A = WORD( true, 1, 2, 3, 4, 5);
  mix.X = WORD(false, 6, 7, 8, 9, 10);
  shiftrightcirc(&mix.A, &mix.X, 2);
  assert(mix.A == WORD(true, 9, 10, 1, 2, 3));
  assert(mix.X == WORD(false, 4, 5, 6, 7, 8));

  mix.A = WORD( true, 1, 2, 3, 4, 5);
  mix.X = WORD(false, 6, 7, 8, 9, 10);
  shiftrightcirc(&mix.A, &mix.X, 6);
  assert(mix.A == WORD(true, 5, 6, 7, 8, 9));
  assert(mix.X == WORD(false, 10, 1, 2, 3, 4));

  mix.A = WORD( true, 1, 2, 3, 4, 5);
  mix.X = WORD(false, 6, 7, 8, 9, 10);
  shiftrightcirc(&mix.A, &mix.X, 34);
  assert(mix.A == WORD(true, 7, 8, 9, 10, 1));
  assert(mix.X == WORD(false, 2, 3, 4, 5, 6));

  // TEST: tape IOC
  FILE *fp = fopen("test-files/in.tape", "r+");
  mix.tapefiles[0] = fp;

  IOtask iotask;
  iotask.M = 0;

  assert(ftell(fp) == 0L);
  // Wind forward
  assert(controltape(&iotask, &mix, POS(3), 0));
  assert(ftell(fp) == 3*601L);

  // Wind backward
  assert(controltape(&iotask, &mix, NEG(1), 0));
  assert(ftell(fp) == 2*601L);

  // Wind backward past beginning
  assert(controltape(&iotask, &mix, NEG(3), 0));
  assert(ftell(fp) == 0L);

  // Wind forward past end
  assert(!controltape(&iotask, &mix, POS(1001), 0));

  // TEST: tape IN
  assert(readtape(&iotask, &mix, 0, 0));
  assert(mix.mem[0] == POS(mixord('A')));
  assert(mix.mem[99] == POS(mixord('Z')));

  // TEST: tape OUT
  fclose(fp);
  fp = fopen("test-files/out.tape", "r+");
  mix.tapefiles[1] = fp;

  // Write to very last block of tape
  assert(controltape(&iotask, &mix, POS(999), 1));
  assert(writetape(&iotask, &mix, 0, 1));
  assert(ftell(fp) == 1000*601L);

  fclose(fp);
}

void testassembler() {
  parsestate ps;
  mix mix;
  initparsestate(&ps);
  initmix(&mix);

  // TEST: parsesym
  char sym[11];
  char *line = "SYMBOL \n", *cur = line;
  assert(parsesym(&cur, sym));
  assert(cur-line == 6);
  assert(strcmp(sym, "SYMBOL") == 0);

  line = "123456\n"; cur = line;
  assert(!parsesym(&cur, sym));
  assert(cur == line);

  line = "VERYLONGSYMBOL\n";
  assert(!parsesym(&line, sym));

  // TEST: parseoperator
  int opidx;
  byte C, F;
  line = "HLT\t";
  assert(parseoperator(&line, &C, &F));
  assert(C==5 && F==2);

  line = "XXX\t";
  assert(!parseoperator(&line, &C, &F));

  line = "TOOLONG\t";
  assert(!parseoperator(&line, &C, &F));

  // TEST: parsenum
  int num;
  line = "00052\n";
  assert(parsenum(&line, &num));
  assert(num == 52);

  line = "1234123412341234\n";
  assert(!parsenum(&line, &num));

  line = "20BY20\n";
  assert(!parsenum(&line, &num));

  word w; byte b;
  // TEST: parseatomic
  line = "UNDEFINED\n";
  assert(!parseatomic(&line, &w, &ps));

  ps.numsyms = 1;
  strcpy(ps.syms[0], "20BY20");
  ps.symvals[0] = POS(1234);
  line = "20BY20\n";
  assert(parseatomic(&line, &w, &ps));
  assert(w == POS(1234));

  // TEST: parseexpr
  line = "5\n";
  assert(parseexpr(&line, &w, &ps));
  assert(w == POS(5));

  line = "+5\n";
  assert(parseexpr(&line, &w, &ps));
  assert(w == POS(5));

  line = "-5\n";
  assert(parseexpr(&line, &w, &ps));
  assert(w == NEG(5));

  line = "-1+5\n";
  assert(parseexpr(&line, &w, &ps));
  assert(w == POS(4));

  line = "-1+5*20/6\n";
  assert(parseexpr(&line, &w, &ps));
  assert(w == POS(13));

  line = "1//3\n";
  assert(parseexpr(&line, &w, &ps));
  assert(w == POS(357913941));

  line = "1:3\n";
  assert(parseexpr(&line, &w, &ps));
  assert(w == POS(11));

  line = "*+3\n";
  assert(parseexpr(&line, &w, &ps));
  assert(w == POS(ps.star+3));

  ps.star = 1;
  line = "***\n";
  assert(parseexpr(&line, &w, &ps));
  assert(w == POS(ps.star*ps.star));

  // TEST: parseA
  line = "5678\n";
  assert(parseA(&line, &w, &ps));
  assert(w == POS(5678));

  ps.star = 3000;
  line = "* \n";
  assert(parseA(&line, &w, &ps));
  assert(w == POS(3000));

  line = "###\n";
  assert(!parseA(&line, &w, &ps));

  // TEST: parseI
  line = ",6\n";
  assert(parseI(&line, &b, &ps));
  assert(b == 6);

  line = ",###\n";
  assert(!parseI(&line, &b, &ps));

  line = "###\n";
  assert(parseI(&line, &b, &ps));
  assert(b == 0);

  // TEST: parseF
  line = "(12)\n";
  assert(parseF(&line, &b, &ps));
  assert(b == 12);

  line = "(12\n";
  assert(!parseF(&line, &b, &ps));

  line = "(1:5)\n";
  assert(parseF(&line, &b, &ps));
  assert(b == 13);

  // TEST: parseW
  line = "1\n";
  assert(parseW(&line, &w, &ps));
  assert(w == POS(1));

  line = "1,-1000(0:2)\n";
  assert(parseW(&line, &w, &ps));
  assert(w == INSTR(ADDR(-1000), 0, 0, 1));

  line = "-1000(0:2),1\n";
  assert(parseW(&line, &w, &ps));
  assert(w == POS(1));

  extraparseinfo extraparseinfo;
  // TEST: parseline
  initparsestate(&ps);
  line = "START\tNOP\n";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  assert(ps.numsyms == 1);
  assert(!strcmp(ps.syms[0], "START"));
  assert(ps.symvals[0] == POS(ps.star-1));

  line = "TEN\tEQU\t10\n";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  assert(ps.numsyms == 2);
  assert(!strcmp(ps.syms[1], "TEN"));
  assert(ps.symvals[1] == POS(10));

  line = "\tCON\t1337\n";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  assert(ps.numsyms == 2);
  assert(mix.mem[ps.star-1] == POS(1337));

  line = "\tALF\tA2J5S\n";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  assert(mix.mem[ps.star-1] == WORD(true,1,32,11,35,22));

  line = "\tORIG\t2000\n";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  assert(ps.star == 2000);

  line = "\tORIG\t9999\n";
  assert(!parseline(line, &ps, &mix, &extraparseinfo));

  line = "==INVALID LINE==";
  assert(!parseline(line, &ps, &mix, &extraparseinfo));

  line = "* COMMENT";
  assert(parseline(line, &ps, &mix, &extraparseinfo));

  ps.star = 3000;
  line = "\tSTA\t2000(1:5)\n";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  assert(mix.mem[3000] == INSTR(ADDR(2000), 0, 13, 24));

  // TEST: future references
  initparsestate(&ps);
  line = "\tJMP\tFUTURE\n";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  assert(ps.numfuturerefs == 1);
  assert(!ps.futurerefs[0].resolved);
  assert(ps.futurerefs[0].addr == 0);
  assert(!ps.futurerefs[0].which);
  assert(!strcmp(ps.futurerefs[0].sym, "FUTURE"));
  line = "FUTURE\tNOP\n";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  line = "\tJMP\tUNDEFINED\n";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  assert(ps.numfuturerefs == 2);
  line = "\tJMP\t=2000=\n";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  assert(ps.numfuturerefs == 3);
  assert(ps.futurerefs[2].which);
  assert(ps.futurerefs[2].literal == POS(2000));
  line = "FOO\tEND\t1000\n";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  assert(ps.futurerefs[0].resolved);
  assert(ps.futurerefs[1].resolved);
  assert(getA(mix.mem[0]) == (1|(1<<12)));
  assert(getA(mix.mem[2]) == (4|(1<<12)));
  assert(getA(mix.mem[3]) == (5|(1<<12)));
  assert(mix.mem[4] == POS(0));
  assert(mix.mem[5] == POS(2000));
  assert(lookupsym("FOO", &w, &ps));
  assert(w == POS(6));
  assert(mix.PC == 1000);

  // TEST: local symbols
  initparsestate(&ps);
  line = "1H\tNOP\n";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  assert(ps.localsymcounts[1] == 1);
  assert(lookupsym("1H#00", &w, &ps));
  assert(w == POS(0));
  line = "\tJMP\t1F\n";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  line = "\tJMP\t1B\n";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  line = "2H\tNOP\n";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  assert(ps.localsymcounts[2] == 1);
  assert(lookupsym("2H#00", &w, &ps));
  assert(w == POS(3));
  line = "1H\tNOP\n";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  assert(ps.localsymcounts[1] == 2);
  assert(lookupsym("1H#01", &w, &ps));
  assert(w == POS(4));
  line = "\tEND\t1000\n";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  assert(getA(mix.mem[1]) == (4|(1<<12)));
  assert(getA(mix.mem[2]) == (0|(1<<12)));

  // TEST: multiple of the same undefined symbol
  initparsestate(&ps);
  line = "\tJMP\tUNDEFINED\n";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  line = "\tJMP\tUNDEFINED\n";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  line = "\tEND\t1000\n";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  assert(getA(mix.mem[0]) == (2|(1<<12)));
  assert(getA(mix.mem[1]) == (2|(1<<12)));

  // TEST: no A/I/F field, but with comment
  initparsestate(&ps);
  line = "\tHLT\t\thalts the program";
  assert(parseline(line, &ps, &mix, &extraparseinfo));
  assert(INTA(getA(mix.mem[0])) == 0);
  assert(getI(mix.mem[0]) == 0);
  assert(getF(mix.mem[0]) == 2);
  assert(getC(mix.mem[0]) == 5);
}

int main() {
  testemulator();
  testassembler();
}