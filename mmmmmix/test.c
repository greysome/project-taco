#include <stdio.h>
#include "assembler.h"
#include "emulator.h"
#include "mmm.h"

void test_emulator() {
  Mix mix;
  word w;
  signmag sm;
  int64_t i;

  // TEST: conversions between word, signmag, int
  fprintf(stderr, "TESTING: conversion between word, signmag, int...\n");

  w = build_word(false, 1, 2, 3, 4, 5);
  assert(w.sign == false);
  assert(w.bytes[0] == 1);
  assert(w.bytes[1] == 2);
  assert(w.bytes[2] == 3);
  assert(w.bytes[3] == 4);
  assert(w.bytes[4] == 5);

  sm = word_to_signmag(w);
  assert(sm.sign == false);
  assert(sm.mag == 1ULL * MIX_BYTE_SIZE * MIX_BYTE_SIZE * MIX_BYTE_SIZE * MIX_BYTE_SIZE
	 + 2 * MIX_BYTE_SIZE * MIX_BYTE_SIZE * MIX_BYTE_SIZE
	 + 3 * MIX_BYTE_SIZE * MIX_BYTE_SIZE
	 + 4 * MIX_BYTE_SIZE
	 + 5);
  assert(word_eq(signmag_to_word(sm), w));

  i = word_to_int(w);
  assert(i == -1LL * MIX_BYTE_SIZE * MIX_BYTE_SIZE * MIX_BYTE_SIZE * MIX_BYTE_SIZE
	 - 2 * MIX_BYTE_SIZE * MIX_BYTE_SIZE * MIX_BYTE_SIZE
	 - 3 * MIX_BYTE_SIZE * MIX_BYTE_SIZE
	 - 4 * MIX_BYTE_SIZE
	 - 5);
  assert(word_eq(int_to_word(i), w));
  assert(signmag_to_int(sm) == i);

  w = pos_word(1ULL * MIX_BYTE_SIZE * MIX_BYTE_SIZE * MIX_BYTE_SIZE * MIX_BYTE_SIZE
	       + 2 * MIX_BYTE_SIZE * MIX_BYTE_SIZE * MIX_BYTE_SIZE
	       + 3 * MIX_BYTE_SIZE * MIX_BYTE_SIZE
	       + 4 * MIX_BYTE_SIZE
	       + 5);
  assert(w.sign == true);
  assert(w.bytes[0] == 1);
  assert(w.bytes[1] == 2);
  assert(w.bytes[2] == 3);
  assert(w.bytes[3] == 4);
  assert(w.bytes[4] == 5);

  w = neg_word(6ULL * MIX_BYTE_SIZE * MIX_BYTE_SIZE * MIX_BYTE_SIZE * MIX_BYTE_SIZE
	       + 7 * MIX_BYTE_SIZE * MIX_BYTE_SIZE * MIX_BYTE_SIZE
	       + 8 * MIX_BYTE_SIZE * MIX_BYTE_SIZE
	       + 9 * MIX_BYTE_SIZE
	       + 10);
  assert(w.sign == false);
  assert(w.bytes[0] == 6);
  assert(w.bytes[1] == 7);
  assert(w.bytes[2] == 8);
  assert(w.bytes[3] == 9);
  assert(w.bytes[4] == 10);
  w = set_sign(w, true);
  assert(w.sign == true);

  // TEST: MIX instruction fields
  fprintf(stderr, "TESTING: MIX instruction fields...\n");

  w = build_instr((signmag) {true, 2000}, 2, 3, 8); // LDA 2000,2(0:3)
  assert(signmag_eq(get_A(w), (signmag) {true, 2000}));
  assert(get_I(w) == 2);
  assert(get_F(w) == 3);
  assert(get_C(w) == 8);

  mix.I[1] = pos_word(5);
  assert(get_M(w, &mix) == 2005);
  mix.I[1] = neg_word(2005);
  assert(get_M(w, &mix) == -5);

  // TEST: extracting bytes
  fprintf(stderr, "TESTING: extracting bytes...\n");

  w = build_word(false, 1, 2, 3, 4, 5);
  assert(word_eq(extract_bytes(w, 3*8+5), build_word(true, 0, 0, 3, 4, 5)));
  assert(word_eq(extract_bytes(w, 0*8+3), build_word(false, 0, 0, 1, 2, 3)));
  assert(word_eq(extract_bytes(w, 4*8+4), build_word(true, 0, 0, 0, 0, 4)));
  assert(word_eq(extract_bytes(w, 0*8+0), build_word(false, 0, 0, 0, 0, 0)));
  assert(word_eq(extract_bytes(w, 1*8+1), build_word(true, 0, 0, 0, 0, 1)));

  // TEST: storing words
  fprintf(stderr, "TESTING: storing words...\n");

  mix.A         = build_word(true, 6, 7, 8, 9, 0);
  mix.mem[1000] = build_word(false, 1, 2, 3, 4, 5);
  store_word(&mix.mem[1000], mix.A, 0*8+5);
  assert(word_eq(mix.mem[1000], build_word(true, 6, 7, 8, 9, 0)));

  mix.mem[1000] = build_word(false, 1, 2, 3, 4, 5);
  store_word(&mix.mem[1000], mix.A, 1*8+5);
  assert(word_eq(mix.mem[1000], build_word(false, 6, 7, 8, 9, 0)));

  mix.mem[1000] = build_word(false, 1, 2, 3, 4, 5);
  store_word(&mix.mem[1000], mix.A, 5*8+5);
  assert(word_eq(mix.mem[1000], build_word(false, 1, 2, 3, 4, 0)));

  mix.mem[1000] = build_word(false, 1, 2, 3, 4, 5);
  store_word(&mix.mem[1000], mix.A, 2*8+2);
  assert(word_eq(mix.mem[1000], build_word(false, 1, 0, 3, 4, 5)));

  mix.mem[1000] = build_word(false, 1, 2, 3, 4, 5);
  store_word(&mix.mem[1000], mix.A, 2*8+3);
  assert(word_eq(mix.mem[1000], build_word(false, 1, 9, 0, 4, 5)));

  mix.mem[1000] = build_word(false, 1, 2, 3, 4, 5);
  store_word(&mix.mem[1000], mix.A, 0*8+1);
  assert(word_eq(mix.mem[1000], build_word(true, 0, 2, 3, 4, 5)));

  // TEST: adding words
  fprintf(stderr, "TESTING: adding words...\n");

#define BUILD_INT(sign, a, b, c, d, e)					\
  sign ((int64_t) (a) * MIX_BYTE_SIZE * MIX_BYTE_SIZE * MIX_BYTE_SIZE * MIX_BYTE_SIZE \
   + (b) * MIX_BYTE_SIZE * MIX_BYTE_SIZE * MIX_BYTE_SIZE		\
   + (c) * MIX_BYTE_SIZE * MIX_BYTE_SIZE				\
   + (d) * MIX_BYTE_SIZE						\
   + (e))

  // -- 1. Both positive, no overflow
  mix.A = build_word(true, 19, 18, 1, 2, 22);
  w     = build_word(true,  1, 36, 5, 0, 50);
  bool overflow = add_word(&mix.A, w);
  assert(!overflow);
  // Convert word to int for byte-size-agnostic testing
  assert(word_to_int(mix.A) == BUILD_INT(+, 20, 54, 6, 2, 72));

  // -- 2. Both positive, overflow
  mix.A = build_word(true, 59, 18, 1, 2, 22);
  w     = build_word(true, 50, 36, 5, 0, 50);
  overflow = add_word(&mix.A, w);
  assert(overflow);
  assert(word_to_int(mix.A) == BUILD_INT(+, 109, 54, 6, 2, 72) - MIX_MAX);

  // -- 3. Both negative, overflow
  mix.A = build_word(false, 59, 18, 1, 2, 22);
  w     = build_word(false, 50, 36, 5, 0, 50);
  overflow = add_word(&mix.A, w);
  assert(overflow);
  assert(word_to_int(mix.A) == BUILD_INT(-, 109, 54, 6, 2, 72) + MIX_MAX);

  // -- 4. X-Y, X>Y
  mix.A = build_word( true, 19, 18, 1, 2, 22);
  w     = build_word(false,  1, 36, 5, 0, 50);
  overflow = add_word(&mix.A, w);
  assert(!overflow);
  assert(word_to_int(mix.A) == BUILD_INT(+, 18, -18, -4, 2, -28));

  // -- 5. X-Y, X<Y
  mix.A = build_word( true, 19, 18, 1, 2, 22);
  w     = build_word(false, 50, 36, 5, 0, 50);
  overflow = add_word(&mix.A, w);
  assert(!overflow);
  assert(word_to_int(mix.A) == BUILD_INT(-, 31, 18, 4, -2, 28));

  // -- 6. -X+Y, X>Y
  mix.A = build_word(false, 19, 18, 1, 2, 22);
  w     = build_word( true,  1, 36, 5, 0, 50);
  overflow = add_word(&mix.A, w);
  assert(!overflow);
  assert(word_to_int(mix.A) == BUILD_INT(-, 18, -18, -4, 2, -28));

  // -- 7. -X+Y, X<Y
  mix.A = build_word(false, 19, 18, 1, 2, 22);
  w     = build_word( true, 50, 36, 5, 0, 50);
  overflow = add_word(&mix.A, w);
  assert(!overflow);
  assert(word_to_int(mix.A) == BUILD_INT(+, 31, 18, 4, -2, 28));

  // -- 8. Partial fields
  mix.A = build_word( true, 19, 18, 1, 2, 22);
  w     = build_word(false,  1, 36, 5, 0, 50);
  overflow = add_word(&mix.A, extract_bytes(w, 3*8+3));
  assert(!overflow);
  assert(word_to_int(mix.A) == BUILD_INT(+, 19, 18, 1, 2, 27));

  // -- 9. Result = 0, sign of A should be unchanged
  mix.A = build_word( true, 0, 0, 0, 0, 5);
  w     = build_word(false, 0, 0, 0, 0, 5);
  add_word(&mix.A, w);
  assert(word_to_int(mix.A) == BUILD_INT(+, 0, 0, 0, 0, 0));

  mix.A = build_word(false, 0, 0, 0, 0, 5);
  w     = build_word( true, 0, 0, 0, 0, 5);
  add_word(&mix.A, w);
  assert(word_to_int(mix.A) == BUILD_INT(+, 0, 0, 0, 0, 0));

  mix.A = build_word(true, MIX_BYTE_SIZE-1, MIX_BYTE_SIZE-1, MIX_BYTE_SIZE-1, MIX_BYTE_SIZE-1, MIX_BYTE_SIZE-1);
  w     = build_word(true, 0, 0, 0, 0, 1);
  add_word(&mix.A, w);
  assert(word_to_int(mix.A) == BUILD_INT(+, 0, 0, 0, 0, 0));

  mix.A = build_word(false, MIX_BYTE_SIZE-1, MIX_BYTE_SIZE-1, MIX_BYTE_SIZE-1, MIX_BYTE_SIZE-1, MIX_BYTE_SIZE-1);
  w     = build_word(false, 0, 0, 0, 0, 1);
  add_word(&mix.A, w);
  assert(word_to_int(mix.A) == BUILD_INT(-, 0, 0, 0, 0, 0));

  // TEST: negating words
  fprintf(stderr, "TESTING: negating words...\n");
  w = build_word(true, 1, 2, 3, 4, 5);
  assert(word_eq(negate_word(w), build_word(false, 1, 2, 3, 4, 5)));

  // TEST: subtracting words
  fprintf(stderr, "TESTING: subtracting words...\n");
  mix.A = build_word(false, 19, 18, 1, 2, 22);
  w     = build_word(false, 50, 36, 5, 0, 50);
  overflow = sub_word(&mix.A, w);
  assert(!overflow);
  assert(word_to_int(mix.A) == BUILD_INT(+, 31, 18, 4, -2, 28));

  // TEST: multiplication of words
  fprintf(stderr, "TESTING: multiplying words...\n");
  mix.A = build_word(false, 50, 0, 1, 48, 4);
  w     = build_word(false,  2, 0, 0,  0, 0);
  mul_word(&mix.A, &mix.X, w);
  assert(word_to_int(mix.A) == BUILD_INT(+, 0, 100, 0, 2, 96));
  assert(word_to_int(mix.X) == BUILD_INT(+, 8, 0, 0, 0, 0));

  // TODO: make this test byte-size-agnostic
  mix.A = build_word(false, 0, 0, 0, 1, 48);
  w     = build_word(false, 2, 9, 9, 9,  9);
  mul_word(&mix.A, &mix.X, extract_bytes(w, 9));
#if MIX_BYTE_SIZE == 64
  assert(word_to_int(mix.A) == BUILD_INT(-, 0, 0, 0, 0, 0));
  assert(word_to_int(mix.X) == BUILD_INT(-, 0, 0, 0, 3, 32));
#endif

#undef BUILD_INT

  // TEST: division of words
  fprintf(stderr, "TESTING: dividing words...\n");
  mix.A = neg_word(0);
  mix.X = pos_word(17);
  w = pos_word(3);
  overflow = div_word(&mix.A, &mix.X, w);
  assert(!overflow);
  assert(word_eq(mix.A, neg_word(5)));
  assert(word_eq(mix.X, neg_word(2)));

  mix.A = pos_word(1);
  mix.X = pos_word(2);
  w = pos_word(0);
  overflow = div_word(&mix.A, &mix.X, w);
  assert(overflow);

  mix.A = pos_word(5000);
  mix.X = pos_word(0);
  w = pos_word(1);
  overflow = div_word(&mix.A, &mix.X, w);
  assert(overflow);

  // TEST: comparing words
  fprintf(stderr, "TESTING: comparing words...\n");
  w      = build_word(false, 1, 2, 3, 4, 6);
  word v = build_word( true, 1, 2, 3, 4, 5);
  assert(cmp_word(v, w) == 1);
  assert(cmp_word(w, v) == -1);
  assert(cmp_word(extract_bytes(v, 1*8+5), extract_bytes(w, 1*8+5)) == -1);
  assert(cmp_word(extract_bytes(v, 1*8+4), extract_bytes(w, 1*8+4)) == 0);
  assert(cmp_word(extract_bytes(v, 0*8+4), extract_bytes(w, 0*8+4)) == 1);
  assert(cmp_word(extract_bytes(v, 0*8+0), extract_bytes(w, 0*8+0)) == 0);

  // TEST: word to num
  fprintf(stderr, "TESTING: word to num...\n");
  mix.A = build_word(false,  0,  0, 31, 32, 39);
  mix.X = build_word( true, 37, 57, 47, 30, 30);
  word_to_num(&mix.A, &mix.X);
  assert(word_eq(mix.A, neg_word(12977700)));
  assert(word_eq(mix.X, build_word(true, 37, 57, 47, 30, 30)));

  // TEST: num to char
  fprintf(stderr, "TESTING: num to char...\n");
  num_to_char(&mix.A, &mix.X);
  assert(word_eq(mix.A, build_word(false, 30, 30, 31, 32, 39)));
  assert(word_eq(mix.X, build_word( true, 37, 37, 37, 30, 30)));

#if MIX_BYTE_SIZE == 64
  // TEST: xor
  fprintf(stderr, "TESTING: xor...\n");
  mix.A = build_word(false, 0, 10, 20, 30, 40);
  w = build_word(true, 63, 32, 16, 8, 4);
  xor(&mix.A, w);
  assert(word_eq(mix.A, build_word(false, 63, 42, 4, 22, 44)));
#else
  fprintf(stderr, "SKIPPED: xor (byte size != 64)\n");
#endif

  // TEST: shifting words
  fprintf(stderr, "TESTING: shifting words...\n");
  mix.A = build_word(true, 1, 2, 3, 4, 5);
  shift_left_word(&mix.A, 0);
  assert(word_eq(mix.A, build_word(true, 1, 2, 3, 4, 5)));

  mix.A = build_word(true, 1, 2, 3, 4, 5);
  shift_left_word(&mix.A, 2);
  assert(word_eq(mix.A, build_word(true, 3, 4, 5, 0, 0)));

  mix.A = build_word(true, 1, 2, 3, 4, 5);
  shift_left_word(&mix.A, 6);
  assert(word_eq(mix.A, build_word(true, 0, 0, 0, 0, 0)));

  mix.A = build_word(true, 1, 2, 3, 4, 5);
  shift_right_word(&mix.A, 1);
  assert(word_eq(mix.A, build_word(true, 0, 1, 2, 3, 4)));

  mix.A = build_word(true, 1, 2, 3, 4, 5);
  shift_right_word(&mix.A, 3);
  assert(word_eq(mix.A, build_word(true, 0, 0, 0, 1, 2)));

  mix.A = build_word(true, 1, 2, 3, 4, 5);
  shift_right_word(&mix.A, 5);
  assert(word_eq(mix.A, build_word(true, 0, 0, 0, 0, 0)));

  mix.A = build_word( true, 1, 2, 3, 4, 5);
  mix.X = build_word(false, 6, 7, 8, 9, 10);
  shift_left_words(&mix.A, &mix.X, 1);
  assert(word_eq(mix.A, build_word(true, 2, 3, 4, 5, 6)));
  assert(word_eq(mix.X, build_word(false, 7, 8, 9, 10, 0)));

  mix.A = build_word( true, 1, 2, 3, 4, 5);
  mix.X = build_word(false, 6, 7, 8, 9, 10);
  shift_left_words(&mix.A, &mix.X, 5);
  assert(word_eq(mix.A, build_word(true, 6, 7, 8, 9, 10)));
  assert(word_eq(mix.X, build_word(false, 0, 0, 0, 0, 0)));

  mix.A = build_word( true, 1, 2, 3, 4, 5);
  mix.X = build_word(false, 6, 7, 8, 9, 10);
  shift_left_words(&mix.A, &mix.X, 11);
  assert(word_eq(mix.A, build_word(true, 0, 0, 0, 0, 0)));
  assert(word_eq(mix.X, build_word(false, 0, 0, 0, 0, 0)));

  mix.A = build_word( true, 1, 2, 3, 4, 5);
  mix.X = build_word(false, 6, 7, 8, 9, 10);
  shift_right_words(&mix.A, &mix.X, 2);
  assert(word_eq(mix.A, build_word(true, 0, 0, 1, 2, 3)));
  assert(word_eq(mix.X, build_word(false, 4, 5, 6, 7, 8)));

  mix.A = build_word( true, 1, 2, 3, 4, 5);
  mix.X = build_word(false, 6, 7, 8, 9, 10);
  shift_right_words(&mix.A, &mix.X, 8);
  assert(word_eq(mix.A, build_word(true, 0, 0, 0, 0, 0)));
  assert(word_eq(mix.X, build_word(false, 0, 0, 0, 1, 2)));

  mix.A = build_word(true, 1, 2, 3, 4, 5);
  mix.X = build_word(false, 6, 7, 8, 9, 10);
  shift_left_circ(&mix.A, &mix.X, 0);
  assert(word_eq(mix.A, build_word(true, 1, 2, 3, 4, 5)));
  assert(word_eq(mix.X, build_word(false, 6, 7, 8, 9, 10)));

  mix.A = build_word( true, 1, 2, 3, 4, 5);
  mix.X = build_word(false, 6, 7, 8, 9, 10);
  shift_left_circ(&mix.A, &mix.X, 1);
  assert(word_eq(mix.A, build_word(true, 2, 3, 4, 5, 6)));
  assert(word_eq(mix.X, build_word(false, 7, 8, 9, 10, 1)));

  mix.A = build_word( true, 1, 2, 3, 4, 5);
  mix.X = build_word(false, 6, 7, 8, 9, 10);
  shift_left_circ(&mix.A, &mix.X, 5);
  assert(word_eq(mix.A, build_word(true, 6, 7, 8, 9, 10)));
  assert(word_eq(mix.X, build_word(false, 1, 2, 3, 4, 5)));

  mix.A = build_word( true, 1, 2, 3, 4, 5);
  mix.X = build_word(false, 6, 7, 8, 9, 10);
  shift_left_circ(&mix.A, &mix.X, 33);
  assert(word_eq(mix.A, build_word(true, 4, 5, 6, 7, 8)));
  assert(word_eq(mix.X, build_word(false, 9, 10, 1, 2, 3)));

  mix.A = build_word( true, 1, 2, 3, 4, 5);
  mix.X = build_word(false, 6, 7, 8, 9, 10);
  shift_right_circ(&mix.A, &mix.X, 2);
  assert(word_eq(mix.A, build_word(true, 9, 10, 1, 2, 3)));
  assert(word_eq(mix.X, build_word(false, 4, 5, 6, 7, 8)));

  mix.A = build_word( true, 1, 2, 3, 4, 5);
  mix.X = build_word(false, 6, 7, 8, 9, 10);
  shift_right_circ(&mix.A, &mix.X, 6);
  assert(word_eq(mix.A, build_word(true, 5, 6, 7, 8, 9)));
  assert(word_eq(mix.X, build_word(false, 10, 1, 2, 3, 4)));

  mix.A = build_word( true, 1, 2, 3, 4, 5);
  mix.X = build_word(false, 6, 7, 8, 9, 10);
  shift_right_circ(&mix.A, &mix.X, 34);
  assert(word_eq(mix.A, build_word(true, 7, 8, 9, 10, 1)));
  assert(word_eq(mix.X, build_word(false, 2, 3, 4, 5, 6)));

  // TEST: tape IOC
  fprintf(stderr, "TESTING: tape IOC...\n");
  FILE *fp = fopen("test-files/in.tape", "r+");
  mix.tape_files[0] = fp;

  assert(ftell(fp) == 0L);
  // Wind forward
  assert(tape_ioc(&mix, 3, 0));
  assert(ftell(fp) == 3*601L);

  // Wind backward
  assert(tape_ioc(&mix, -1, 0));
  assert(ftell(fp) == 2*601L);

  // Wind backward past beginning
  assert(tape_ioc(&mix, -3, 0));
  assert(ftell(fp) == 0L);

  // Wind forward past end
  assert(!tape_ioc(&mix, 1001, 0));

  // TEST: tape IN
  mix.err = NULL;
  rewind(fp);
  assert(tape_read(&mix, 0, 0));
  assert(word_eq(mix.mem[0], pos_word(mix_ord('A'))));
  assert(word_eq(mix.mem[99], pos_word(mix_ord('Z'))));

  // TEST: tape OUT
  fclose(fp);
  fp = fopen("test-files/out.tape", "r+");
  mix.tape_files[1] = fp;

  // Write to very last block of tape
  assert(tape_ioc(&mix, 999, 1));
  assert(tape_write(&mix, 0, 1));
  assert(ftell(fp) == 1000*601L);

  fprintf(stderr, "All emulator tests passed!\n\n");
  fclose(fp);
}

void test_assembler() {
  ParseState ps;
  Mix mix;
  parse_state_init(&ps);
  mix_init(&mix);

  // TEST: parse_sym
  fprintf(stderr, "TESTING: parse_sym...\n");
  char sym[11];
  char *line = "SYMBOL \n", *cur = line;
  assert(parse_sym(&cur, sym));
  assert(cur-line == 6);
  assert(strcmp(sym, "SYMBOL") == 0);

  line = "123456\n"; cur = line;
  assert(!parse_sym(&cur, sym));
  assert(cur == line);

  line = "VERYLONGSYMBOL\n";
  assert(!parse_sym(&line, sym));

  // TEST: parse_operator
  fprintf(stderr, "TESTING: parse_operator...\n");
  int op_idx;
  byte C, F;
  line = "HLT\t";
  assert(parse_operator(&line, &C, &F));
  assert(C==5 && F==2);

  line = "XXX\t";
  assert(!parse_operator(&line, &C, &F));

  line = "TOOLONG\t";
  assert(!parse_operator(&line, &C, &F));

  // TEST: parse_num
  fprintf(stderr, "TESTING: parse_num...\n");
  int num;
  line = "00052\n";
  assert(parse_num(&line, &num));
  assert(num == 52);

  line = "1234123412341234\n";
  assert(!parse_num(&line, &num));

  line = "20BY20\n";
  assert(!parse_num(&line, &num));

  word w; byte b;
  // TEST: parse_atomic
  fprintf(stderr, "TESTING: parse_atomic...\n");
  line = "UNDEFINED\n";
  assert(!parse_atomic(&line, &w, &ps));

  ps.num_syms = 1;
  strcpy(ps.syms[0], "20BY20");
  ps.sym_vals[0] = pos_word(1234);
  line = "20BY20\n";
  assert(parse_atomic(&line, &w, &ps));
  assert(word_eq(w, pos_word(1234)));

  // TEST: parse_expr
  fprintf(stderr, "TESTING: parse_expr...\n");
  line = "5\n";
  assert(parse_expr(&line, &w, &ps));
  assert(word_eq(w, pos_word(5)));

  line = "+5\n";
  assert(parse_expr(&line, &w, &ps));
  assert(word_eq(w, pos_word(5)));

  line = "-5\n";
  assert(parse_expr(&line, &w, &ps));
  assert(word_eq(w, neg_word(5)));

  line = "-1+5\n";
  assert(parse_expr(&line, &w, &ps));
  assert(word_eq(w, pos_word(4)));

  line = "-1+5*20/6\n";
  assert(parse_expr(&line, &w, &ps));
  assert(word_eq(w, pos_word(13)));

  line = "1//3\n";
  assert(parse_expr(&line, &w, &ps));
  assert(word_eq(w, pos_word(MIX_MAX / 3)));

  line = "1:3\n";
  assert(parse_expr(&line, &w, &ps));
  assert(word_eq(w, pos_word(11)));

  line = "*+3\n";
  assert(parse_expr(&line, &w, &ps));
  assert(word_eq(w, pos_word(ps.star+3)));

  ps.star = 1;
  line = "***\n";
  assert(parse_expr(&line, &w, &ps));
  assert(word_eq(w, pos_word(ps.star*ps.star)));

  // TEST: parse_A
  fprintf(stderr, "TESTING: parse_A...\n");
  line = "5678\n";
  assert(parse_A(&line, &w, &ps));
  assert(word_eq(w, pos_word(5678)));

  ps.star = 3000;
  line = "* \n";
  assert(parse_A(&line, &w, &ps));
  assert(word_eq(w, pos_word(3000)));

  line = "###\n";
  assert(!parse_A(&line, &w, &ps));

  // TEST: parse_I
  fprintf(stderr, "TESTING: parse_I...\n");
  line = ",6\n";
  assert(parse_I(&line, &b, &ps));
  assert(b == 6);

  line = ",###\n";
  assert(!parse_I(&line, &b, &ps));

  line = "###\n";
  assert(parse_I(&line, &b, &ps));
  assert(b == 0);

  // TEST: parse_F
  fprintf(stderr, "TESTING: parse_F...\n");
  line = "(12)\n";
  assert(parse_F(&line, &b, &ps));
  assert(b == 12);

  line = "(12\n";
  assert(!parse_F(&line, &b, &ps));

  line = "(1:5)\n";
  assert(parse_F(&line, &b, &ps));
  assert(b == 13);

  // TEST: parse_W
  fprintf(stderr, "TESTING: parse_W...\n");
  line = "1\n";
  assert(parse_W(&line, &w, &ps));
  assert(word_eq(w, pos_word(1)));

  line = "1,-1000(0:2)\n";
  assert(parse_W(&line, &w, &ps));
  assert(word_eq(w, build_instr((signmag) {false, 1000}, 0, 0, 1)));

  line = "-1000(0:2),1\n";
  assert(parse_W(&line, &w, &ps));
  assert(word_eq(w, pos_word(1)));

  // TEST: parse_line
  fprintf(stderr, "TESTING: parse_line...\n");
  ExtraParseInfo extraparseinfo;
  parse_state_init(&ps);
  line = "START\tNOP\n";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  assert(ps.num_syms == 1);
  assert(!strcmp(ps.syms[0], "START"));
  assert(word_eq(ps.sym_vals[0], pos_word(ps.star-1)));

  line = "TEN\tEQU\t10\n";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  assert(ps.num_syms == 2);
  assert(!strcmp(ps.syms[1], "TEN"));
  assert(word_eq(ps.sym_vals[1], pos_word(10)));

  line = "\tCON\t1337\n";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  assert(ps.num_syms == 2);
  assert(word_eq(mix.mem[ps.star-1], pos_word(1337)));

  line = "\tALF\tA2J5S\n";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  assert(word_eq(mix.mem[ps.star-1], build_word(true, 1, 32, 11, 35, 22)));

  line = "\tORIG\t2000\n";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  assert(ps.star == 2000);

  line = "\tORIG\t9999\n";
  assert(!parse_line(line, &ps, &mix, &extraparseinfo));

  line = "==INVALID LINE==";
  assert(!parse_line(line, &ps, &mix, &extraparseinfo));

  line = "* COMMENT";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));

  ps.star = 3000;
  line = "\tSTA\t2000(1:5)\n";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  assert(word_eq(mix.mem[3000], build_instr((signmag) {true, 2000}, 0, 13, 24)));

  // TEST: future references
  fprintf(stderr, "TESTING: future references...\n");
  parse_state_init(&ps);
  line = "\tJMP\tFUTURE\n";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  assert(ps.num_future_refs == 1);
  assert(!ps.future_refs[0].is_resolved);
  assert(ps.future_refs[0].addr == 0);
  assert(!ps.future_refs[0].is_literal);
  assert(!strcmp(ps.future_refs[0].sym, "FUTURE"));
  line = "FUTURE\tNOP\n";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  line = "\tJMP\tUNDEFINED\n";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  assert(ps.num_future_refs == 2);
  line = "\tJMP\t=2000=\n";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  assert(ps.num_future_refs == 3);
  assert(ps.future_refs[2].is_literal);
  assert(word_eq(ps.future_refs[2].literal, pos_word(2000)));
  line = "FOO\tEND\t1000\n";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  assert(ps.future_refs[0].is_resolved);
  assert(ps.future_refs[1].is_resolved);
  assert(signmag_eq(get_A(mix.mem[0]), (signmag) {true, 1}));
  assert(signmag_eq(get_A(mix.mem[2]), (signmag) {true, 4}));
  assert(signmag_eq(get_A(mix.mem[3]), (signmag) {true, 5}));
  assert(word_eq(mix.mem[4], pos_word(0)));
  assert(word_eq(mix.mem[5], pos_word(2000)));
  assert(lookup_sym("FOO", &w, &ps));
  assert(word_eq(w, pos_word(6)));
  assert(mix.PC == 1000);

  // TEST: local symbols
  fprintf(stderr, "TESTING: local symbols...\n");
  parse_state_init(&ps);
  line = "1H\tNOP\n";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  assert(ps.local_sym_counts[1] == 1);
  assert(lookup_sym("1H#00", &w, &ps));
  assert(word_eq(w, pos_word(0)));
  line = "\tJMP\t1F\n";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  line = "\tJMP\t1B\n";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  line = "2H\tNOP\n";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  assert(ps.local_sym_counts[2] == 1);
  assert(lookup_sym("2H#00", &w, &ps));
  assert(word_eq(w, pos_word(3)));
  line = "1H\tNOP\n";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  assert(ps.local_sym_counts[1] == 2);
  assert(lookup_sym("1H#01", &w, &ps));
  assert(word_eq(w, pos_word(4)));
  line = "\tEND\t1000\n";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  assert(signmag_eq(get_A(mix.mem[1]), (signmag) {true, 4}));
  assert(signmag_eq(get_A(mix.mem[2]), (signmag) {true, 0}));

  // TEST: multiple instances of the same undefined symbol
  fprintf(stderr, "TESTING: multiple instances of undefined symbol...\n");
  parse_state_init(&ps);
  line = "\tJMP\tUNDEFINED\n";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  line = "\tJMP\tUNDEFINED\n";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  line = "\tEND\t1000\n";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  assert(signmag_eq(get_A(mix.mem[0]), (signmag) {true, 2}));
  assert(signmag_eq(get_A(mix.mem[1]), (signmag) {true, 2}));

  // TEST: comment, but no A/I/F field
  fprintf(stderr, "TESTING: comment, but no A/I/F field...\n");
  parse_state_init(&ps);
  line = "\tHLT\t\thalts the program";
  assert(parse_line(line, &ps, &mix, &extraparseinfo));
  assert(signmag_eq(get_A(mix.mem[0]), (signmag) {true, 0}));
  assert(get_I(mix.mem[0]) == 0);
  assert(get_F(mix.mem[0]) == 2);
  assert(get_C(mix.mem[0]) == 5);

  fprintf(stderr, "All assembler tests passed!\n\n");
}

static bool load_mixal_file(char *filename, Mix *mix) {
  ParseState ps;
  FILE *fp;
  int line_num = 0;
  char line[MMM_LINE_LEN];
  ExtraParseInfo extra;

  mix_init(mix);
  parse_state_init(&ps);

  if ((fp = fopen(filename, "r")) == NULL) return false;

  while (++line_num && fgets(line, MMM_LINE_LEN, fp) != NULL) {
    if (!parse_line(line, &ps, mix, &extra)) return false;
    if (extra.is_end) break;
  }
  return true;
}

static bool run_mix(Mix *mix) {
  while (!mix->done) mix_step(mix);
  return mix->err == NULL;
}

void test_integration() {
  Mix mix;
  // TEST: max
  fprintf(stderr, "TESTING: max...\n");
  assert(load_mixal_file("../taocp/1.3.2-max.mixal", &mix));
  assert(run_mix(&mix) || (puts(mix.err), false));
  assert(word_eq(mix.A, pos_word(9)));
  assert(word_eq(mix.I[1], pos_word(5)));

  // TEST: primes
  fprintf(stderr, "TESTING: primes...\n");
  assert(load_mixal_file("../taocp/1.3.2-primes.mixal", &mix));
  assert(run_mix(&mix) || (puts(mix.err), false));

  // TEST: instr
  fprintf(stderr, "TESTING: instr...\n");
  assert(load_mixal_file("../taocp/1.3.2-instr.mixal", &mix));
  assert(run_mix(&mix) || (puts(mix.err), false));                    // A = 2000, I = 0, F = 10, C = 63
  assert(word_eq(mix.A, pos_word(1337)));

  assert(load_mixal_file("../taocp/1.3.2-instr.mixal", &mix));
  // Inject different inputs directly into memory
  mix.mem[3000] = build_instr((signmag) { false, 2000 }, 0, 0, INCA); // A = -2000
  assert(run_mix(&mix) || (puts(mix.err), false));
  assert(word_eq(mix.A, pos_word(666)));

  assert(load_mixal_file("../taocp/1.3.2-instr.mixal", &mix));
  mix.mem[3000] = build_instr((signmag) { true, 2000 }, 0, 0, INCA);  // A = +2000
  mix.mem[3004] = build_instr((signmag) { true, 24 }, 0, 0, INCA);    // F = (3:0)
  assert(run_mix(&mix) || (puts(mix.err), false));
  assert(word_eq(mix.A, pos_word(666)));

  // TEST: saddle
  fprintf(stderr, "TESTING: saddle...\n");
  assert(load_mixal_file("../taocp/1.3.2-saddle.mixal", &mix));
  assert(run_mix(&mix) || (puts(mix.err), false));
  assert(word_eq(mix.I[0], pos_word(42)));

  // TODO: port more programs over

  fprintf(stderr, "All integration tests passed!\n");
}

int main() {
  test_emulator();
  test_assembler();
  test_integration();
}