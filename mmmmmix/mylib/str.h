#ifndef STR_H
#define STR_H

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "arena.h"
#include "util.h"

#define STR_INIT_CAPACITY 16

typedef struct {
  char *s;
  size_t len;
  size_t capacity;
} Str;

Str str_new(Arena *arena);
Str str_alloc(size_t n, Arena *arena);
Str str_from_cstr_n(char *cstr, size_t n);
Str str_from_cstr(char *cstr);
Str str_from_cstr_till(char *cstr, int c);
void str_append_cstr_n(Str *s, char *cstr, size_t n, Arena *arena);
void str_append_cstr(Str *s, char *cstr, Arena *arena);
void str_append_char(Str *s, char c, Arena *arena);
void str_append_str(Str *s1, Str *s2, Arena *arena);
void str_append_int(Str *s, int i, Arena *arena);
int str_eq_cstr(Str *s, char *cstr);
bool str_parse_uint(Str *s, uint *x);
Str str_parse_word(Str *s, Arena *arena);
void str_appendf(Str *s, Arena *arena, const char *fmt, ...);

#endif // STR_H

#ifdef STR_IMPLEMENTATION

Str str_alloc(size_t n, Arena *arena) {
  Str s;
  s.s = arena_alloc(STR_INIT_CAPACITY, arena);
  s.s[0] = '\0';
  s.len = 0;
  s.capacity = STR_INIT_CAPACITY;
  return s;
}

Str str_new(Arena *arena) {
  return str_alloc(STR_INIT_CAPACITY, arena);
}

Str str_from_cstr_n(char *cstr, size_t n) {
  Str s;
  s.s = cstr;
  s.len = n;
  s.capacity = n+1;
  return s;
}

Str str_from_cstr(char *cstr) {
  return str_from_cstr_n(cstr, strlen(cstr));
}

Str str_from_cstr_till(char *cstr, int c) {
  uintptr_t a = (uintptr_t) cstr;
  uintptr_t b = (uintptr_t) strchr(cstr, c);
  assert(b != (uintptr_t)NULL);
  return str_from_cstr_n(cstr, b - a);
}

void str_append_cstr_n(Str *s, char *cstr, size_t n, Arena *arena) {
  size_t len_new = s->len + n;
  if (len_new + 1 > s->capacity) {
    char *p = arena_alloc(len_new * 2, arena);
    memcpy(p, s->s, s->len);
    memcpy(p + s->len, cstr, n);
    s->s = p;
    s->len = len_new;
    s->s[len_new] = '\0';
    s->capacity = len_new * 2;
  }
  else {
    memcpy(s->s + s->len, cstr, n);
    s->len = len_new;
    s->s[len_new] = '\0';
  }
}

void str_append_cstr(Str *s, char *cstr, Arena *arena) {
  str_append_cstr_n(s, cstr, strlen(cstr), arena);
}

void str_append_char(Str *s, char c, Arena *arena) {
  str_append_cstr_n(s, &c, 1, arena);
}

void str_append_str(Str *s1, Str *s2, Arena *arena) {
  str_append_cstr_n(s1, s2->s, s2->len, arena);
}

void str_append_u16(Str *s, int i, Arena *arena) {
  str_append_cstr_n(s, (char *)&i, sizeof(int), arena);
}

int str_eq_cstr(Str *s, char *cstr) {
  return memcmp(s->s, cstr, s->len) == 0;
}

bool str_parse_uint(Str *s, uint *x) {
  Str original = *s;
  char c;

  while (1) {
    if (s->len == 0) {
      *s = original;
      return false;
    }

    c = *s->s;
    if (c == ' ' || c == '\n' || c == '\t') {
      s->len--;
      s->capacity--;
      s->s++;
    }
    else
      break;
  }

  if (c < '0' || c > '9') {
    *s = original;
    return false;
  }

  uint y = 0;

  while (1) {
    c = *s->s;
    if (c < '0' || c > '9' || s->len == 0) {
      *x = y;
      return true;
    }

    y = 10 * y + (uint)(c - '0');
    s->len--;
    s->capacity--;
    s->s++;
  }
}

Str str_parse_word(Str *s, Arena *arena) {
  char c;
  Str s_new = str_new(arena);

  while (1) {
    if (s->len == 0) {
      s_new.s[0] = '\0';
      return s_new;
    }

    c = *s->s;
    if (c == ' ' || c == '\n' || c == '\t') {
      s->len--;
      s->capacity--;
      s->s++;
    }
    else
      break;
  }

  size_t word_len = 0;
  char *word_start = s->s;

  while (1) {
    c = *s->s;
    if (c == ' ' || c == '\n' || c == '\t' || s->len == 0) {
      str_append_cstr_n(&s_new, word_start, word_len, arena);
      return s_new;
    }
    s->s++;
    s->len--;
    word_len++;
  }
}

void str_appendf(Str *s, Arena *arena, const char *fmt, ...) {
  va_list args1, args2;
  va_start(args1, fmt);
  va_copy(args2, args1);

  int required_len = vsnprintf(NULL, 0, fmt, args1);
  va_end(args1);

  if (required_len < 0) {
    va_end(args2);
    return;
  }

  char *buf = arena_alloc(required_len + 1, arena);
  vsnprintf(buf, required_len + 1, fmt, args2);
  va_end(args2);
  str_append_cstr(s, buf, arena);
}

#endif // STR_IMPLEMENTATION