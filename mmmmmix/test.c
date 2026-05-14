#include <assert.h>
#include <fcntl.h>    // for open
#include <sys/stat.h> // for lstat
#include <unistd.h>   // for lseek
#define UTIL_IMPLEMENTATION
#define ARENA_IMPLEMENTATION
#define ASSEMBLER_IMPLEMENTATION
#define EMULATOR_IMPLEMENTATION
#define IO_IMPLEMENTATION
#define STR_IMPLEMENTATION
#include "mylib/arena.h"
#include "mylib/io.h"
#include "mylib/str.h"
#include "assembler.h"
#include "emulator.h"

void test_emulator(Arena arena) {
  Mix mix;
  word w;
  signmag sm;
  int64_t i;

  // TEST: conversions between word, signmag, int
  my_eprint("TESTING: conversion between word, signmag, int...\n");

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
  my_eprint("TESTING: MIX instruction fields...\n");

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
  my_eprint("TESTING: extracting bytes...\n");

  w = build_word(false, 1, 2, 3, 4, 5);
  assert(word_eq(extract_bytes(w, 3*8+5), build_word(true, 0, 0, 3, 4, 5)));
  assert(word_eq(extract_bytes(w, 0*8+3), build_word(false, 0, 0, 1, 2, 3)));
  assert(word_eq(extract_bytes(w, 4*8+4), build_word(true, 0, 0, 0, 0, 4)));
  assert(word_eq(extract_bytes(w, 0*8+0), build_word(false, 0, 0, 0, 0, 0)));
  assert(word_eq(extract_bytes(w, 1*8+1), build_word(true, 0, 0, 0, 0, 1)));

  // TEST: storing words
  my_eprint("TESTING: storing words...\n");

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
  my_eprint("TESTING: adding words...\n");

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
  my_eprint("TESTING: negating words...\n");
  w = build_word(true, 1, 2, 3, 4, 5);
  assert(word_eq(negate_word(w), build_word(false, 1, 2, 3, 4, 5)));

  // TEST: subtracting words
  my_eprint("TESTING: subtracting words...\n");
  mix.A = build_word(false, 19, 18, 1, 2, 22);
  w     = build_word(false, 50, 36, 5, 0, 50);
  overflow = sub_word(&mix.A, w);
  assert(!overflow);
  assert(word_to_int(mix.A) == BUILD_INT(+, 31, 18, 4, -2, 28));

  // TEST: multiplication of words
  my_eprint("TESTING: multiplying words...\n");
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
  my_eprint("TESTING: dividing words...\n");
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
  my_eprint("TESTING: comparing words...\n");
  w      = build_word(false, 1, 2, 3, 4, 6);
  word v = build_word( true, 1, 2, 3, 4, 5);
  assert(cmp_word(v, w) == 1);
  assert(cmp_word(w, v) == -1);
  assert(cmp_word(extract_bytes(v, 1*8+5), extract_bytes(w, 1*8+5)) == -1);
  assert(cmp_word(extract_bytes(v, 1*8+4), extract_bytes(w, 1*8+4)) == 0);
  assert(cmp_word(extract_bytes(v, 0*8+4), extract_bytes(w, 0*8+4)) == 1);
  assert(cmp_word(extract_bytes(v, 0*8+0), extract_bytes(w, 0*8+0)) == 0);

  // TEST: word to num
  my_eprint("TESTING: word to num...\n");
  mix.A = build_word(false,  0,  0, 31, 32, 39);
  mix.X = build_word( true, 37, 57, 47, 30, 30);
  word_to_num(&mix.A, &mix.X);
  assert(word_eq(mix.A, neg_word(12977700)));
  assert(word_eq(mix.X, build_word(true, 37, 57, 47, 30, 30)));

  // TEST: num to char
  my_eprint("TESTING: num to char...\n");
  num_to_char(&mix.A, &mix.X);
  assert(word_eq(mix.A, build_word(false, 30, 30, 31, 32, 39)));
  assert(word_eq(mix.X, build_word( true, 37, 37, 37, 30, 30)));

#if MIX_BYTE_SIZE == 64
  // TEST: xor
  my_eprint("TESTING: xor...\n");
  mix.A = build_word(false, 0, 10, 20, 30, 40);
  w = build_word(true, 63, 32, 16, 8, 4);
  xor(&mix.A, w);
  assert(word_eq(mix.A, build_word(false, 63, 42, 4, 22, 44)));
#else
  my_eprint("SKIPPED: xor (byte size != 64)\n");
#endif

  // TEST: shifting words
  my_eprint("TESTING: shifting words...\n");
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
  my_eprint("TESTING: tape IOC...\n");
  int fd = open("test-files/in.tape", O_RDWR);
  assert(fd != -1);
  mix.device_fds[TAPE_0] = fd;

  assert(lseek(fd, 0, SEEK_CUR) == 0L);
  // Wind forward
  assert(tape_ioc(&mix, 3, 0));
  assert(lseek(fd, 0, SEEK_CUR) == 3*601L);

  // Wind backward
  assert(tape_ioc(&mix, -1, 0));
  assert(lseek(fd, 0, SEEK_CUR) == 2*601L);

  // Wind backward past beginning
  assert(tape_ioc(&mix, -3, 0));
  assert(lseek(fd, 0, SEEK_CUR) == 0L);

  // Wind forward past end
  assert(!tape_ioc(&mix, 1001, 0));

  // TEST: tape IN
  my_eprint("TESTING: tape IN...\n");
  lseek(fd, 0, SEEK_SET);
  assert(tape_in(&mix, 0, 0));
  assert(word_eq(mix.mem[0], pos_word(mix_ord('A'))));
  assert(word_eq(mix.mem[99], pos_word(mix_ord('Z'))));

  // TEST: tape OUT
  my_eprint("TESTING: tape OUT...\n");
  fd = open("test-files/out.tape", O_RDWR);
  assert(fd != -1);
  mix.device_fds[TAPE_1] = fd;

  // Write to very last block of tape
  assert(tape_ioc(&mix, 999, 1));
  assert(tape_out(&mix, 0, 1));
  assert(lseek(fd, 0, SEEK_CUR) == 1000*601L);

  my_eprint("All emulator tests passed!\n\n");
}

void test_assembler(Arena arena) {
  Mix mix;
  ParseState ps;
  mix_init(&mix);
  parse_state_init(&ps, &arena);

  // TEST: parse_sym
  my_eprint("TESTING: parse_sym...\n");
  char *line;
  StrView line_view;
  StrView sym;

  line = "SYMBOL\t\n";
  line_view = sv_from_cstr(line);
  assert(parse_sym(&line_view, &sym));
  assert(line_view.s - line == 6);
  assert(sv_eq_cstr(sym, "SYMBOL"));

  line = "123456\n";
  line_view = sv_from_cstr(line);
  assert(!parse_sym(&line_view, &sym));
  assert(line_view.s == line);

  line_view = sv_from_cstr("VERYLONGSYMBOL\n");
  assert(!parse_sym(&line_view, &sym));

  // TEST: parse_operator
  my_eprint("TESTING: parse_operator...\n");
  int op_idx;
  byte C, F;

  line_view = sv_from_cstr("HLT\t");
  assert(parse_operator(&line_view, &C, &F));
  assert(C == 5);
  assert(F == 2);

  line_view = sv_from_cstr("XXX\t");
  assert(!parse_operator(&line_view, &C, &F));

  line_view = sv_from_cstr("TOOLONG\t");
  assert(!parse_operator(&line_view, &C, &F));

  // TEST: parse_num
  my_eprint("TESTING: parse_num...\n");
  int num;

  line_view = sv_from_cstr("00052\n");
  assert(parse_num(&line_view, &num));
  assert(num == 52);

  line_view = sv_from_cstr("1234123412341234\n");
  assert(!parse_num(&line_view, &num));

  line_view = sv_from_cstr("20BY20\n");
  assert(!parse_num(&line_view, &num));

  // TEST: parse_atomic
  my_eprint("TESTING: parse_atomic...\n");
  word w;
  byte b;

  line_view = sv_from_cstr("UNDEFINED\n");
  assert(!parse_atomic(&line_view, &w, &ps));

  ps.num_syms = 1;
  ps.syms[0] = sb_from_cstr("20BY20", &arena);
  ps.sym_vals[0] = pos_word(1234);
  line_view = sv_from_cstr("20BY20\n");
  assert(parse_atomic(&line_view, &w, &ps));
  assert(word_eq(w, pos_word(1234)));

  // TEST: parse_expr
  my_eprint("TESTING: parse_expr...\n");
  line_view = sv_from_cstr("5\n");
  assert(parse_expr(&line_view, &w, &ps));
  assert(word_eq(w, pos_word(5)));

  line_view = sv_from_cstr("+5\n");
  assert(parse_expr(&line_view, &w, &ps));
  assert(word_eq(w, pos_word(5)));

  line_view = sv_from_cstr("-5\n");
  assert(parse_expr(&line_view, &w, &ps));
  assert(word_eq(w, neg_word(5)));

  line_view = sv_from_cstr("-1+5\n");
  assert(parse_expr(&line_view, &w, &ps));
  assert(word_eq(w, pos_word(4)));

  line_view = sv_from_cstr("-1+5*20/6\n");
  assert(parse_expr(&line_view, &w, &ps));
  assert(word_eq(w, pos_word(13)));

  line_view = sv_from_cstr("1//3\n");
  assert(parse_expr(&line_view, &w, &ps));
  assert(word_eq(w, pos_word(MIX_MAX / 3)));

  line_view = sv_from_cstr("1:3\n");
  assert(parse_expr(&line_view, &w, &ps));
  assert(word_eq(w, pos_word(11)));

  line_view = sv_from_cstr("*+3\n");
  assert(parse_expr(&line_view, &w, &ps));
  assert(word_eq(w, pos_word(ps.star+3)));

  ps.star = 1;
  line_view = sv_from_cstr("***\n");
  assert(parse_expr(&line_view, &w, &ps));
  assert(word_eq(w, pos_word(ps.star*ps.star)));

  // TEST: parse_A
  my_eprint("TESTING: parse_A...\n");
  line_view = sv_from_cstr("5678\n");
  assert(parse_A(&line_view, &w, &ps));
  assert(word_eq(w, pos_word(5678)));

  ps.star = 3000;
  line_view = sv_from_cstr("* \n");
  assert(parse_A(&line_view, &w, &ps));
  assert(word_eq(w, pos_word(3000)));

  line_view = sv_from_cstr("###\n");
  assert(!parse_A(&line_view, &w, &ps));

  // TEST: parse_I
  my_eprint("TESTING: parse_I...\n");
  line_view = sv_from_cstr(",6\n");
  assert(parse_I(&line_view, &b, &ps));
  assert(b == 6);

  line_view = sv_from_cstr(",###\n");
  assert(!parse_I(&line_view, &b, &ps));

  line_view = sv_from_cstr("###\n");
  assert(parse_I(&line_view, &b, &ps));
  assert(b == 0);

  // TEST: parse_F
  my_eprint("TESTING: parse_F...\n");
  line_view = sv_from_cstr("(12)\n");
  assert(parse_F(&line_view, &b, &ps));
  assert(b == 12);

  line_view = sv_from_cstr("(12\n");
  assert(!parse_F(&line_view, &b, &ps));

  line_view = sv_from_cstr("(1:5)\n");
  assert(parse_F(&line_view, &b, &ps));
  assert(b == 13);

  // TEST: parse_W
  my_eprint("TESTING: parse_W...\n");
  line_view = sv_from_cstr("1\n");
  assert(parse_W(&line_view, &w, &ps));
  assert(word_eq(w, pos_word(1)));

  line_view = sv_from_cstr("1,-1000(0:2)\n");
  assert(parse_W(&line_view, &w, &ps));
  assert(word_eq(w, build_instr((signmag) {false, 1000}, 0, 0, 1)));

  line_view = sv_from_cstr("-1000(0:2),1\n");
  assert(parse_W(&line_view, &w, &ps));
  assert(word_eq(w, pos_word(1)));

  // TEST: parse_line
  my_eprint("TESTING: parse_line...\n");
  LineInfo line_info;
  parse_state_init(&ps, &arena);
  line_view = sv_from_cstr("START\tNOP\n");
  assert(parse_line(&line_view, &ps, &mix, &line_info));
  assert(ps.num_syms == 1);
  assert(sb_eq_cstr(ps.syms[0], "START"));
  assert(word_eq(ps.sym_vals[0], pos_word(ps.star-1)));

  line_view = sv_from_cstr("TEN\tEQU\t10\n");
  assert(parse_line(&line_view, &ps, &mix, &line_info));
  assert(ps.num_syms == 2);
  assert(sb_eq_cstr(ps.syms[1], "TEN"));
  assert(word_eq(ps.sym_vals[1], pos_word(10)));

  line_view = sv_from_cstr("\tCON\t1337\n");
  assert(parse_line(&line_view, &ps, &mix, &line_info));
  assert(ps.num_syms == 2);
  assert(word_eq(mix.mem[ps.star-1], pos_word(1337)));

  line_view = sv_from_cstr("\tALF\tA2J5S\n");
  assert(parse_line(&line_view, &ps, &mix, &line_info));
  assert(word_eq(mix.mem[ps.star-1], build_word(true, 1, 32, 11, 35, 22)));

  line_view = sv_from_cstr("\tORIG\t2000\n");
  assert(parse_line(&line_view, &ps, &mix, &line_info));
  assert(ps.star == 2000);

  line_view = sv_from_cstr("\tORIG\t9999\n");
  assert(!parse_line(&line_view, &ps, &mix, &line_info));

  line_view = sv_from_cstr("==INVALID LINE_VIEW==");
  assert(!parse_line(&line_view, &ps, &mix, &line_info));

  line_view = sv_from_cstr("* COMMENT");
  assert(parse_line(&line_view, &ps, &mix, &line_info));

  ps.star = 3000;
  line_view = sv_from_cstr("\tSTA\t2000(1:5)\n");
  assert(parse_line(&line_view, &ps, &mix, &line_info));
  assert(word_eq(mix.mem[3000], build_instr((signmag) {true, 2000}, 0, 13, 24)));

  line_view = sv_from_cstr("INVALIDSYM!\t");
  assert(!parse_line(&line_view, &ps, &mix, &line_info));

  line_view = sv_from_cstr("\tNOP!\t");
  assert(!parse_line(&line_view, &ps, &mix, &line_info));

  line_view = sv_from_cstr("\tJMP INVALID!\t");
  assert(!parse_line(&line_view, &ps, &mix, &line_info));

  line_view = sv_from_cstr("\tJMP INVALID,2!\t");
  assert(!parse_line(&line_view, &ps, &mix, &line_info));

  // TEST: future references
  my_eprint("TESTING: future references...\n");
  parse_state_init(&ps, &arena);

  line_view = sv_from_cstr("\tJMP\tFUTURE\n");
  assert(parse_line(&line_view, &ps, &mix, &line_info));
  assert(ps.num_future_refs == 1);
  assert(!ps.future_refs[0].is_resolved);
  assert(ps.future_refs[0].addr == 0);
  assert(!ps.future_refs[0].is_literal);
  assert(sb_eq_cstr(ps.future_refs[0].sym, "FUTURE"));

  line_view = sv_from_cstr("FUTURE\tNOP\n");
  assert(parse_line(&line_view, &ps, &mix, &line_info));

  line_view = sv_from_cstr("\tJMP\tUNDEFINED\n");
  assert(parse_line(&line_view, &ps, &mix, &line_info));
  assert(ps.num_future_refs == 2);

  line_view = sv_from_cstr("\tJMP\t=2000=\n");
  assert(parse_line(&line_view, &ps, &mix, &line_info));
  assert(ps.num_future_refs == 3);
  assert(ps.future_refs[2].is_literal);
  assert(word_eq(ps.future_refs[2].literal, pos_word(2000)));

  line_view = sv_from_cstr("FOO\tEND\t1000\n");
  assert(parse_line(&line_view, &ps, &mix, &line_info));
  assert(ps.future_refs[0].is_resolved);
  assert(ps.future_refs[1].is_resolved);
  assert(signmag_eq(get_A(mix.mem[0]), (signmag) {true, 1}));
  assert(signmag_eq(get_A(mix.mem[2]), (signmag) {true, 4}));
  assert(signmag_eq(get_A(mix.mem[3]), (signmag) {true, 5}));
  assert(word_eq(mix.mem[4], pos_word(0)));
  assert(word_eq(mix.mem[5], pos_word(2000)));
  assert(lookup_sym(sv_from_cstr("FOO"), &w, &ps));
  assert(word_eq(w, pos_word(6)));
  assert(mix.PC == 1000);

  // TEST: local symbols
  my_eprint("TESTING: local symbols...\n");
  parse_state_init(&ps, &arena);

  line_view = sv_from_cstr("1H\tNOP\n");
  assert(parse_line(&line_view, &ps, &mix, &line_info));
  assert(ps.local_sym_counts[1] == 1);
  assert(lookup_sym(sv_from_cstr("1H#00"), &w, &ps));
  assert(word_eq(w, pos_word(0)));

  line_view = sv_from_cstr("\tJMP\t1F\n");
  assert(parse_line(&line_view, &ps, &mix, &line_info));

  line_view = sv_from_cstr("\tJMP\t1B\n");
  assert(parse_line(&line_view, &ps, &mix, &line_info));

  line_view = sv_from_cstr("2H\tNOP\n");
  assert(parse_line(&line_view, &ps, &mix, &line_info));
  assert(ps.local_sym_counts[2] == 1);
  assert(lookup_sym(sv_from_cstr("2H#00"), &w, &ps));
  assert(word_eq(w, pos_word(3)));

  line_view = sv_from_cstr("1H\tNOP\n");
  assert(parse_line(&line_view, &ps, &mix, &line_info));
  assert(ps.local_sym_counts[1] == 2);
  assert(lookup_sym(sv_from_cstr("1H#01"), &w, &ps));
  assert(word_eq(w, pos_word(4)));

  line_view = sv_from_cstr("\tEND\t1000\n");
  assert(parse_line(&line_view, &ps, &mix, &line_info));
  assert(signmag_eq(get_A(mix.mem[1]), (signmag) {true, 4}));
  assert(signmag_eq(get_A(mix.mem[2]), (signmag) {true, 0}));

  // TEST: multiple instances of the same undefined symbol
  my_eprint("TESTING: multiple instances of undefined symbol...\n");
  parse_state_init(&ps, &arena);

  line_view = sv_from_cstr("\tJMP\tUNDEFINED\n");
  assert(parse_line(&line_view, &ps, &mix, &line_info));

  line_view = sv_from_cstr("\tJMP\tUNDEFINED\n");
  assert(parse_line(&line_view, &ps, &mix, &line_info));

  line_view = sv_from_cstr("\tEND\t1000\n");
  assert(parse_line(&line_view, &ps, &mix, &line_info));
  assert(signmag_eq(get_A(mix.mem[0]), (signmag) {true, 2}));
  assert(signmag_eq(get_A(mix.mem[1]), (signmag) {true, 2}));

  // TEST: comment, but no A/I/F field
  my_eprint("TESTING: comment, but no A/I/F field...\n");
  parse_state_init(&ps, &arena);

  line_view = sv_from_cstr("\tHLT\t\thalts the program");
  assert(parse_line(&line_view, &ps, &mix, &line_info));
  assert(signmag_eq(get_A(mix.mem[0]), (signmag) {true, 0}));
  assert(get_I(mix.mem[0]) == 0);
  assert(get_F(mix.mem[0]) == 2);
  assert(get_C(mix.mem[0]) == 5);

  my_eprint("All assembler tests passed!\n\n");
}

static bool load_mixal(char *filename, Mix *mix, ParseState *ps, Arena *arena) {
  mix_init(mix);
  parse_state_init(ps, arena);

  int source_fd = open(filename, O_RDONLY);
  assert(source_fd != -1);
  struct stat statbuf;
  assert(fstat(source_fd, &statbuf) != -1);
  assert(statbuf.st_size >= 0);

  StrBuf source_buf = sb_alloc(statbuf.st_size, arena);
  // Disk I/O, so most likely one read() necessary
  assert(read(source_fd, source_buf.s, statbuf.st_size) == statbuf.st_size);
  source_buf.len = statbuf.st_size;

  int line_num = 0;
  LineInfo line_info;
  StrView source_view = sv_from_sb(source_buf);

  while (1) {
    line_num++;
    if (!parse_line(&source_view, ps, mix, &line_info))
      return false;
    if (line_info.is_end || sv_is_empty(source_view))
      break;
  }
  return true;
}

static bool run_mix(Mix *mix) {
  while (!mix->done) mix_step(mix);
  return mix->err == NULL;
}

void test_integration(Arena arena) {
  Mix mix;
  ParseState ps;

  // TEST: max
  my_eprint("TESTING: max...\n");
  assert(load_mixal("../taocp/1.3.2-max.mixal", &mix, &ps, &arena));
  assert(run_mix(&mix) || (my_print(mix.err), false));
  assert(word_eq(mix.A, pos_word(9)));
  assert(word_eq(mix.I[1], pos_word(5)));

  // TEST: primes
  my_eprint("TESTING: primes...\n");
  assert(load_mixal("../taocp/1.3.2-primes.mixal", &mix, &ps, &arena));
  assert(run_mix(&mix) || (my_print(mix.err), false));

  // TEST: instr
  my_eprint("TESTING: instr...\n");
  assert(load_mixal("../taocp/1.3.2-instr.mixal", &mix, &ps, &arena));
  assert(run_mix(&mix) || (my_print(mix.err), false));                // A = 2000, I = 0, F = 10, C = 63
  assert(word_eq(mix.A, pos_word(1337)));

  assert(load_mixal("../taocp/1.3.2-instr.mixal", &mix, &ps, &arena));
  // Inject different inmy_print directly into memory
  mix.mem[3000] = build_instr((signmag) { false, 2000 }, 0, 0, INCA); // A = -2000
  assert(run_mix(&mix) || (my_print(mix.err), false));
  assert(word_eq(mix.A, pos_word(666)));

  assert(load_mixal("../taocp/1.3.2-instr.mixal", &mix, &ps, &arena));
  mix.mem[3000] = build_instr((signmag) { true, 2000 }, 0, 0, INCA);  // A = +2000
  mix.mem[3004] = build_instr((signmag) { true, 24 }, 0, 0, INCA);    // F = (3:0)
  assert(run_mix(&mix) || (my_print(mix.err), false));
  assert(word_eq(mix.A, pos_word(666)));

  // TEST: saddle
  my_eprint("TESTING: saddle...\n");
  assert(load_mixal("../taocp/1.3.2-saddle.mixal", &mix, &ps, &arena));
  assert(run_mix(&mix) || (my_print(mix.err), false));
  assert(word_eq(mix.I[0], pos_word(42)));

  // TODO: port more programs over

  my_eprint("All integration tests passed!\n");
}

int main() {
  Arena arena = arena_new(1<<20);
  test_emulator(arena);
  test_assembler(arena);
  test_integration(arena);
}