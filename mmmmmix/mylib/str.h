#ifndef STR_H
#define STR_H

#include <assert.h>
#include <stdarg.h>  // for va_list
#include <stdbool.h>
#include <stdio.h>   // for vsnprintf
#include <string.h>  // for strlen, memcmp
#include "arena.h"

typedef struct {
  size_t len;
  char *s;
} StrView;

typedef struct {
  size_t len;
  size_t cap;
  char *s;
} StrBuf;

// StrView's are not null-terminated, so size has to be passed into printf.
#define sv_fmt    "%.*s"
#define sv_arg(t) (t).len, (t).s

#define sv_empty()    (StrView) {.s = NULL, .len = 0}
#define sv_empty_init {.s = NULL, .len = 0, .cap = 0}
bool sv_is_empty(StrView sv);
StrView sv_from_cstr_n(char *cstr, size_t n);
StrView sv_from_cstr(char *cstr);
StrView sv_from_sb(StrBuf sb);
bool sv_advance(StrView *sv, size_t n);
bool sv_eq_cstr(StrView sv, char *cstr);
bool sv_eq_sv(StrView sv1, StrView sv2);
bool sv_eq_sb(StrView sv, StrBuf sb);
bool sv_match_char(StrView *sv, char c);
bool sv_match_one(StrView *sv, char *chars);
bool sv_match_cstr(StrView *sv, char *cstr);
bool sv_find_char(StrView *sv, char c);
StrView sv_parse_word(StrView *sv);
bool sv_parse_uint(StrView *sv, unsigned int *x);

#define sb_empty()    (StrBuf) {.s = NULL, .len = 0, .cap = 0}
#define sb_empty_init {.s = NULL, .len = 0, .cap = 0}
bool sb_is_empty(StrBuf sb);
void sb_clear(StrBuf *sb);
StrBuf sb_alloc(size_t n, Arena *arena);
StrBuf sb_from_cstr_n(char *cstr, size_t n, Arena *arena);
StrBuf sb_from_cstr(char *cstr, Arena *arena);
StrBuf sb_from_sv(StrView sv, Arena *arena);
void sb_append_cstr_n(StrBuf *sb, char *cstr, size_t n, Arena *arena);
void sb_append_cstr(StrBuf *sb, char *cstr, Arena *arena);
void sb_append_sv(StrBuf *sb, StrView sv, Arena *arena);
void sb_appendf(StrBuf *sb, char *fmt, Arena *arena, ...);
bool sb_eq_cstr(StrBuf sb, char *cstr);

#ifdef STR_IMPLEMENTATION

bool sv_is_empty(StrView sv) {
  return sv.len == 0;
}

StrView sv_from_cstr_n(char *cstr, size_t n) {
  return (StrView) {.s = cstr, .len = n};
}

StrView sv_from_cstr(char *cstr) {
  return sv_from_cstr_n(cstr, strlen(cstr));
}

StrView sv_from_sb(StrBuf sb) {
  return sv_from_cstr_n(sb.s, sb.len);
}

bool sv_advance(StrView *sv, size_t n) {
  if (sv->len < n)
    return false;
  sv->s += n;
  sv->len -= n;
  return true;
}

bool sv_eq_cstr_n(StrView sv, char *cstr, size_t n) {
  return sv.len == n && memcmp(sv.s, cstr, n) == 0;
}

bool sv_eq_cstr(StrView sv, char *cstr) {
  return sv_eq_cstr_n(sv, cstr, strlen(cstr));
}

bool sv_eq_sv(StrView sv1, StrView sv2) {
  return sv_eq_cstr_n(sv1, sv2.s, sv2.len);
}

bool sv_eq_sb(StrView sv, StrBuf sb) {
  return sv_eq_cstr_n(sv, sb.s, sb.len);
}

bool sv_match_char(StrView *sv, char c) {
  assert(sv->len >= 1);
  if (sv->s[0] == c) {
    assert(sv_advance(sv, 1));
    return true;
  }
  return false;
}

bool sv_match_one(StrView *sv, char *chars) {
  assert(sv->len >= 1);
  for (char *c = chars; *c != '\0'; c++) {
    if (sv->s[0] == *c) {
      assert(sv_advance(sv, 1));
      return true;
    }
  }
  return false;
}

bool sv_match_cstr(StrView *sv, char *cstr) {
  size_t n = strlen(cstr);
  if (sv->len < n)
    return false;
  if (memcmp(sv->s, cstr, n) == 0) {
    assert(sv_advance(sv, n));
    return true;
  }
  else
    return false;
}

bool sv_find_char(StrView *sv, char c) {
  char *match = memchr(sv->s, c, sv->len);
  if (match == NULL)
    return false;
  assert(sv_advance(sv, match - sv->s));
  return true;
}

// Returns the next contiguous sequence of nonspace characters (not ' ', '\n', '\t'),
// skipping leading spaces.
StrView sv_parse_word(StrView *sv) {
  char *start = sv->s;
  char *end;
  while (1) {
    if (sv_is_empty(*sv))
      return sv_empty();
    if (*start != ' ' && *start != '\t' && *start != '\n')
      break;
    sv_advance(sv, 1);
    start++;
  }
  end = start;
  while (1) {
    if (sv_is_empty(*sv) || *end == ' ' || *end == '\t' || *end == '\n')
      break;
    sv_advance(sv, 1);
    end++;
  }
  return (StrView) {.s = start, .len = end - start};
}

// Parses a nonempty sequence of digits into an integer, until a
// nondigit char is reached. Return false if the first non-whitespace
// char is not a digit (includes \0).
bool sv_parse_uint(StrView *sv, unsigned int *x) {
  StrView oldsv = *sv;

  if (sv_is_empty(*sv))
    return false;

  while (1) {
    if (sv_is_empty(*sv)) {
      *sv = oldsv;
      return false;
    }
    char c = *sv->s;
    if (c != ' ' && c != '\t' && c != '\n')
      break;
    sv_advance(sv, 1);
  }

  char c = *sv->s;
  if (c < '0' || c > '9') {
    *sv = oldsv;
    return false;
  }

  unsigned int x0 = 0;
  while (1) {
    char c = sv->s[0];
    if (c >= '0' && c <= '9')
      x0 = 10 * x0 + (c - '0');
    else
      break;

    sv_advance(sv, 1);
    if (sv_is_empty(*sv))
      break;
  }
  *x = x0;
  return true;
}

bool sb_is_empty(StrBuf sb) {
  assert((sb.s == NULL) == (sb.cap == 0));
  return sb.s == NULL && sb.cap == 0;
}

void sb_clear(StrBuf *sb) {
  sb->len = 0;
  if (sb->s != NULL && sb->cap >= 1)
    sb->s[0] = '\0';
}

StrBuf sb_alloc(size_t n, Arena *arena) {
  return (StrBuf) {.s = arena_alloc(n, arena), .len = 0, .cap = n};
}

StrBuf sb_from_cstr_n(char *cstr, size_t n, Arena *arena) {
  StrBuf sb;
  sb.len = n;
  sb.cap = n == 0 ? 1 : n << 1;
  sb.s = arena_alloc(sb.cap, arena);
  memcpy(sb.s, cstr, n);
  sb.s[sb.len] = '\0';
  return sb;
}

StrBuf sb_from_cstr(char *cstr, Arena *arena) {
  return sb_from_cstr_n(cstr, strlen(cstr), arena);
}

StrBuf sb_from_sv(StrView sv, Arena *arena) {
  return sb_from_cstr_n(sv.s, sv.len, arena);
}

void sb_append_cstr_n(StrBuf *sb, char *cstr, size_t n, Arena *arena) {
  if (sb->len + n + 1 >= sb->cap) {
    if (sb->cap > 0)
      sb->cap <<= 1;
    else
      sb->cap = 16;
    char *new = arena_alloc(sb->cap, arena);
    if (sb->len > 0)
      memcpy(new, sb->s, sb->len);
    sb->s = new;
  }
  memcpy(sb->s + sb->len, cstr, n);
  sb->len += n;
  sb->s[sb->len] = '\0';
}

void sb_append_cstr(StrBuf *sb, char *cstr, Arena *arena) {
  sb_append_cstr_n(sb, cstr, strlen(cstr), arena);
}

void sb_append_sv(StrBuf *sb, StrView sv, Arena *arena) {
  sb_append_cstr_n(sb, sv.s, sv.len, arena);
}

void sb_appendf(StrBuf *sb, char *fmt, Arena *arena, ...) {
  va_list args1, args2;
  va_start(args1, arena);
  va_copy(args2, args1);

  int size = vsnprintf(NULL, 0, fmt, args1);
  if (size == 0)
    return;
  if (size > 4096)
    size = 4096;
  va_end(args1);

  char buf[4096];
  // vsnprintf() always writes the null terminator, which is included in the size.
  vsnprintf(buf, size + 1, fmt, args2);
  sb_append_cstr_n(sb, buf, size, arena);
  va_end(args2);
  return;
}

bool sb_eq_cstr(StrBuf sb, char *cstr) {
  return sv_eq_cstr(sv_from_sb(sb), cstr);
}

#endif // STR_IMPLEMENTATION
#endif // STR_H