#include "assembler.h"

char *MIXOPS[] = {
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
  "CMPA", "CMP1", "CMP2", "CMP3", "CMP4", "CMP5", "CMP6", "CMPX"
};

byte DEFAULTOPFIELDS[] = {
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
  5, 5, 5, 5, 5, 5, 5, 5
};

byte OPCODES[] = {
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
  56, 57, 58, 59, 60, 61, 62, 63
};

void initparsestate(parsestate *ps) {
  ps->star = 0;
  ps->numsyms = 0;
  ps->numfuturerefs = 0;
  for (int i = 0; i < 10; i++)
    ps->localsymcounts[i] = 0;
}

bool lookupsym(char *sym, word *val, parsestate *ps) {
  for (int i = 0; i < ps->numsyms; i++) {
    if (!strcmp(sym, ps->syms[i])) {
      *val = ps->symvals[i];
      return true;
    }
  }
  return false;
}

void addsym(char *sym, word val, parsestate *ps) {
  strcpy(ps->syms[ps->numsyms], sym);
  ps->symvals[ps->numsyms] = val;
  ps->numsyms++;
}

//// A symbol is a string of one to ten letters and/or digits,
//// containing at least one letter.
bool parsesym(char **s, char *sym) {
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

bool parseoperator(char **s, byte *C, byte *F) {
  char *t = *s, *start = *s;
  int k = 0;
  char c;
  // A MIX operator has at most 5 characters
  while (k++, !isspace(c = *(t++)))
    if (k > 4) goto err;
  *s = t-1;
  k--;
  for (int i = 0; i < sizeof(MIXOPS)/sizeof(char*); i++) {
    if (!strncmp(start, MIXOPS[i], k)) {
      *F = DEFAULTOPFIELDS[i];
      *C = OPCODES[i];
      return true;
    }
  }
err:
  *s = start;
  return false;
}

bool parsenum(char **s, uint64_t *val) {
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
  if (*val > WORDMAX)
    return false;
  return true;
err:
  *s = start;
  return false;
}

//// parseatomic() and the rest all write into a word instead of an int,
//// because words and ints are NOT equivalent.
//// Specifically, +0 != -0.

bool parseatomic(char **s, word *val, parsestate *ps) {
  char *start = *s;
  uint64_t i;
  word w;

  // Number?
  if (isdigit(**s) && parsenum(s, &i)) *val = POS(i);

  // * refers to the current line
  else if (**s == '*') {
    (*s)++;
    *val = POS(ps->star);
  }

  // Treat it as a symbol
  else {
    char sym[11];
    if (!parsesym(s, sym)) goto err;
    if (isdigit(sym[0]) && sym[1] == 'B' && sym[2] == '\0') {
      sym[1] = 'H';
      sym[2] = '#';
      int n = ps->localsymcounts[sym[0]-'0'] - 1;
      sym[3] = '0' + n/10;
      sym[4] = '0' + n%10;
      sym[5] = '\0';
    }
    if (!lookupsym(sym, &w, ps)) goto err;
    *val = w;
  }
  return true;
err:
  *s = start;
  return false;
}

bool parseexpr(char **s, word *val, parsestate *ps) {
  char *start = *s;
  word final = POS(0);
  word w;
  char prevsign = '\0';
  if (**s == '+') {
    (*s)++;
    prevsign = '+';
  }
  else if (**s == '-') {
    (*s)++;
    prevsign = '-';
  }
start:
  if (!parseatomic(s, &w, ps)) goto err;

  if (prevsign == '\0') final = w;
  else if (prevsign == '+') addword(&final, w);
  else if (prevsign == '-') subword(&final, w);
  else if (prevsign == '*') {
    word v;
    mulword(&final, &v, w);
    final = v;
  }
  else if (prevsign == '/') {
    word v = POS(0);
    divword(&v, &final, w);
    final = v;
  }
  else if (prevsign == ':') {
    word v;
    mulword(&final, &v, POS(8));
    addword(&v, w);
    final = v;
  }
  else if (prevsign == '|') {
    word v = POS(0);
    divword(&final, &v, w);
  }

  if (**s == '+' || **s == '-' || **s == '*' || **s == ':' ||
      (**s == '/' && *(*s+1) != '/')) {
    prevsign = **s;
    (*s)++;
    goto start;
  }
  else if (**s == '/' && *(*s+1) == '/') {
    prevsign = '|';
    *s += 2;
    goto start;
  }
  *val = final;
  return true;
err:
  *s = start;
  return false;
}

bool parseW(char **s, word *W, parsestate *ps);
bool parseA(char **s, word *A, parsestate *ps) {
  char *start = *s;
  char sym[11];
  //// Expression?
  if (parseexpr(s, A, ps)) return true;

  //// Future reference?
  else if (parsesym(s, sym)) {
    if (isdigit(sym[0]) && sym[1] == 'F' && sym[2] == '\0') {
      sym[1] = 'H';
      sym[2] = '#';
      int n = ps->localsymcounts[sym[0]-'0'];
      sym[3] = '0' + n/10;
      sym[4] = '0' + n%10;
      sym[5] = '\0';
    }
    futureref fr;
    fr.resolved = false;
    fr.addr = ps->star;
    fr.which = false;
    strcpy(fr.sym, sym);
    ps->futurerefs[ps->numfuturerefs++] = fr;
    *A = POS(0);  // The A-field will be filled in later.
    return true;
  }

  //// Local constant?
  else if (**s == '=') {
    word w;
    (*s)++;
    if (!parseW(s, &w, ps)) goto err;
    if (**s != '=') goto err;
    futureref fr;
    fr.resolved = false;
    fr.addr = ps->star;
    fr.which = true;
    fr.literal = w;
    ps->futurerefs[ps->numfuturerefs++] = fr;
    *A = POS(0);  // The A-field will be filled in later.
    return true;
  }

  //// There is no A-field (we will move on to the I or F field or the next line)
  else if (**s == '\t' || **s == ',' || **s == '(') {
    *A = POS(0);
    return true;
  }

err:
  *s = start;
  return false;
}

bool parseI(char **s, byte *I, parsestate *ps) {
  // Don't skip spaces here; if there is whitespace before the comma,
  // we take it to be a comment, e.g.
  // LDA     0       ,comma
  word _I;
  char sym[11];
  if (**s == ',') {
    if (!parseexpr((++*s, s), &_I, ps)) return false;
    *I = INT(_I);
  }
  else {
    *I = 0;
  }
  return true;
}

bool parseF(char **s, byte *F, parsestate *ps) {
  // Don't skip spaces here; if there is whitespace before the opening
  // bracket, we take it to be a comment, e.g.
  // LDA     0       (test comment)
  word _F;
  char *start = *s;
  if (**s == '(') {
    (*s)++;
    if (!parseexpr(s, &_F, ps) || **s != ')') return false;
    *F = INT(_F);
    (*s)++;
    return true;
  }
  *s = start;
  return true;
}

bool parseW(char **s, word *W, parsestate *ps) {
  word w; byte F;
loop:
  F = 5;
  if (!parseexpr(s, &w, ps)) return false;
  if (**s == '(' && !parseF(s, &F, ps)) return false;
  storeword(W, w, F);
  if (**s == ',') {
    (*s)++;
    goto loop;
  }
  return true;
}

bool parseALF(char **s, char *alf, parsestate *ps) {
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

//// Parse line, returning the status and updating the parse state and
//// mix instance.
bool parseline(char *line, parsestate *ps, mix *mix, extraparseinfo *extraparseinfo) {
  extraparseinfo->setdebugline = false;
  extraparseinfo->isend = false;
  //// Ignore comments
  if (line[0] == '*') return true;

  //// Parse LOC field
  bool hasLOC = !isspace(line[0]);
  char sym[11];
  if (hasLOC) {
    if (!parsesym(&line, sym)) return false;
    if (*line != '\t') return false;
    line++;  // Skip a \t

    //// Is LOC a local symbol?
    //// Internally, the kth instance of nH is mapped to the symbol
    //// "nH#k".  The presence of the non-alphanumeric symbol '#'
    //// prevents it from colliding with a user-defined symbol.
    if (isdigit(sym[0]) && sym[1] == 'H' && sym[2] == '\0') {
      int i = sym[0]-'0';
      if (ps->localsymcounts[i] >= 99)
	return false;
      sym[2] = '#';
      sym[3] = '0' + (ps->localsymcounts[i] / 10);
      sym[4] = '0' + (ps->localsymcounts[i] % 10);
      sym[5] = '\0';
      ps->localsymcounts[i]++;
    }
  }
  else line++;  // Skip a \t


  //// Parse OP field

  if (!strncmp(line, "EQU\t", 4)) {
    if (!hasLOC) return false;
    line += 4;
    word val;
    if (!parseW(&line, &val, ps)) return false;
    addsym(sym, val, ps);
  }

  else if (!strncmp(line, "ORIG\t", 5)) {
    line += 5;
    if (hasLOC) addsym(sym, POS(ps->star), ps);
    word val;
    if (!parseW(&line, &val, ps)) return false;
    if (INT(val) < 0 || INT(val) >= 4000) return false;
    ps->star = INT(val);
  }

  else if (!strncmp(line, "CON\t", 4)) {
    line += 4;
    if (hasLOC) addsym(sym, POS(ps->star), ps);
    word val;
    if (!parseW(&line, &val, ps)) return false;
    mix->mem[ps->star++] = val;
    extraparseinfo->setdebugline = true;
  }

  else if (!strncmp(line, "ALF\t", 4)) {
    line += 4;
    if (hasLOC) addsym(sym, POS(ps->star), ps);
    char alf[5];
    if (!parseALF(&line, alf, ps)) return false;
    for (int i = 0; i < 5; i++) alf[i] = mixord(alf[i]);
    mix->mem[ps->star++] = WORD(true, alf[0], alf[1], alf[2], alf[3], alf[4]);
    extraparseinfo->setdebugline = true;
  }

  else if (!strncmp(line, "END\t", 4)) {
    line += 4;
    //// Handle future references
    for (int i = 0; i < ps->numfuturerefs; i++) {
      futureref *fr = &ps->futurerefs[i];
      word val;
      if (!fr->which && lookupsym(fr->sym, &val, ps)) {
	fr->resolved = true;
	word instr = mix->mem[fr->addr];
	mix->mem[fr->addr] = INSTR(ADDR(INT(val)), getI(instr), getF(instr), getC(instr));
      }
    }

    //// Handle literal constants and undefined symbols
    for (int i = 0; i < ps->numfuturerefs; i++) {
      futureref *fr = &ps->futurerefs[i];
      word val = fr->which ? fr->literal : POS(0);
      int addr = ps->star;
      if (!fr->resolved) {
	// Check if the symbol is already defined
	// (happens with the non-first instances of an undefined symbol).
	if (!fr->which) {
	  word tmp;
	  if (lookupsym(fr->sym, &tmp, ps))
	    addr = INT(tmp);
	  else
	    addsym(fr->sym, POS(ps->star), ps);
	}
	word instr = mix->mem[fr->addr];
	mix->mem[fr->addr] = INSTR(ADDR(addr), getI(instr), getF(instr), getC(instr));
	mix->mem[ps->star] = val;
	ps->star++;
	fr->resolved = true;
      }
    }

    if (hasLOC) addsym(sym, POS(ps->star), ps);

    word val;
    if (!parseW(&line, &val, ps)) return false;
    if (INT(val) < 0 || INT(val) >= 4000) return false;
    mix->PC = INT(val);
    extraparseinfo->isend = true;
  }

  //// Treat the OP field as a MIX operator
  else {
    word A = 0;
    byte I = 0, C = 0, F = 0;
    if (!parseoperator(&line, &C, &F)) return false;
    if (hasLOC) addsym(sym, POS(ps->star), ps);
    if (*line == '\n') goto skip;
    line++;  // Skip the tab
    if (!parseA(&line, &A, ps)) return false;
    if (*line == '\n') goto skip;
    if (!parseI(&line, &I, ps)) return false;
    if (*line == '\n') goto skip;
    if (!parseF(&line, &F, ps)) return false;
skip:
    if (I < 0 || I > 6) return false;
    if (F < 0 || F >= 64) return false;
    mix->mem[ps->star++] = INSTR(ADDR(INT(A)), I, F, C);
    extraparseinfo->setdebugline = true;
  }

  return true;
}