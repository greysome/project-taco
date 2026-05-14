#ifndef IO_H
#define IO_H

#include <stdarg.h>   // for va_list, va_start, va_end, va_copy
#include <stdbool.h>  // for bool
#include <stdio.h>    // for size_t
#include <stdlib.h>   // for exit
#include <string.h>   // for strlen
#include <unistd.h>   // for read, write
#include "arena.h"    // for Arena
#include "str.h"      // for StrBuf

// TODO better naming scheme
#define ANSI_RED         "\033[31m"
#define ANSI_GREEN       "\033[32m"
#define ANSI_YELLOW      "\033[33m"
#define ANSI_WHITE       "\033[97m"
#define ANSI_UNDERLINE   "\033[4m"
#define ANSI_UNUNDERLINE "\033[24m"
#define ANSI_SHOW_CURSOR "\033[?25h"
#define ANSI_HIDE_CURSOR "\033[?25l"
#define UNDERLINE(s) "\033[4m" s "\033[24m"
#define RED(s)    "\033[31m" s "\033[37m"
#define GREEN(s)  "\033[32m" s "\033[37m"
#define YELLOW(s) "\033[33m" s "\033[37m"
#define BLUE(s)   "\033[34m" s "\033[37m"
#define CYAN(s)   "\033[36m" s "\033[37m"
#define GRAY(s)   "\033[90m" s "\033[37m"

bool my_dprint(int fd, char *cstr);
bool my_print(char *cstr);
bool my_eprint(char *cstr);
bool my_vdprintf(int fd, char *fmt, va_list args);
bool my_printf(char *fmt, ...);
bool my_eprintf(char *fmt, ...);
bool read_stdin_line(StrBuf *sb, Arena *arena);
bool read_all(int fd, char *buf, size_t n);
bool write_all(int fd, char *buf, size_t n);

void _die(const char *file, int line, char *fmt, ...);
#define die(fmt, ...) _die(__FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)
#define debug(fmt, ...) my_eprintf(fmt __VA_OPT__(,) __VA_ARGS__)
#define warn(fmt, ...) my_eprintf(YELLOW(fmt) __VA_OPT__(,) __VA_ARGS__)

#ifdef IO_IMPLEMENTATION

bool write_all(int fd, char *buf, size_t n) {
  while (n > 0) {
    ssize_t n_written = write(fd, buf, n);
    if (n_written == -1)
      return false;
    n -= n_written;
    buf += n_written;
  }
  return true;
}

bool my_dprint(int fd, char *cstr) {
  return write_all(fd, cstr, strlen(cstr));
}

bool my_print(char *cstr) {
  return my_dprint(STDOUT_FILENO, cstr);
}

bool my_eprint(char *cstr) {
  return my_dprint(STDERR_FILENO, cstr);
}

bool my_vdprintf(int fd, char *fmt, va_list args) {
  va_list args2;
  va_copy(args2, args);

  // Compute the formatted string length (limit to 4096 for convenience).
  int size = vsnprintf(NULL, 0, fmt, args);
  if (size == 0)
    return true;
  if (size > 4095)
    size = 4095;
  va_end(args);

  char buf[4096];
  // vsnprintf() always writes the null terminator, which is included in the size.
  vsnprintf(buf, size + 1, fmt, args2);
  if (!write_all(fd, buf, size))
    return false;
  va_end(args2);
  return true;
}

bool my_printf(char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  bool status = my_vdprintf(STDOUT_FILENO, fmt, args);
  va_end(args);
  return status;
}

bool my_eprintf(char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  bool status = my_vdprintf(STDERR_FILENO, fmt, args);
  va_end(args);
  return status;
}

bool read_stdin_line(StrBuf *sb, Arena *arena) {
  size_t chunk_size = 64;
  char buf[chunk_size];
  while (1) {
    ssize_t n_read = read(STDIN_FILENO, buf, chunk_size);
    if (n_read == -1)
      return false;
    if (n_read == 0)
      break;

    sb_append_cstr_n(sb, buf, n_read, arena);
    if (buf[n_read - 1] == '\n')
      break;
  }
  return true;
}

bool read_all(int fd, char *buf, size_t n) {
  while (n > 0) {
    ssize_t n_read = read(fd, buf, n);
    if (n_read == -1 || n_read == 0)
      return false;
    n -= n_read;
    buf += n_read;
  }
  return true;
}

void _die(const char *file, int line, char *fmt, ...) {
  my_eprintf(ANSI_RED "!! Program died at " UNDERLINE("%s:%d") " -- ", file, line);

  va_list args;
  va_start(args, fmt);
  my_eprintf(fmt, args);
  va_end(args);

  my_eprint(ANSI_WHITE "\n");
  exit(EXIT_FAILURE);
}

#endif // IO_IMPLEMENTATION
#endif // IO_H