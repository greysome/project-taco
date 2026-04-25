#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

typedef unsigned int uint;
typedef unsigned char uchar;

#define ANSI_RED "\033[31m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_WHITE "\033[97m"
#define ANSI_UNDERLINE "\033[4m"
#define ANSI_UNUNDERLINE "\033[24m"

int mini(int a, int b);
int maxi(int a, int b);

void _die(const char *file, int line, char *format, ...);
#define die(fmt, ...) _die(__FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)
#define debug(fmt, ...) fprintf(stderr, fmt __VA_OPT__(,) __VA_ARGS__)
#define warn(fmt, ...) fprintf(stderr, ANSI_YELLOW fmt ANSI_WHITE __VA_OPT__(,) __VA_ARGS__)

#endif // UTIL_H

#ifdef UTIL_IMPLEMENTATION

int mini(int a, int b) { return a < b ? a : b; }

int maxi(int a, int b) { return a < b ? b : a; }

void _die(const char *file, int line, char *format, ...) {
  va_list args;
  va_start(args, format);
  fprintf(stderr, ANSI_RED "!! Program died at " ANSI_UNDERLINE "%s:%d" ANSI_UNUNDERLINE " -- ", file, line);
  vfprintf(stderr, format, args);
  va_end(args);
  fprintf(stderr, ANSI_WHITE "\n");
  exit(EXIT_FAILURE);
}

#endif // UTIL_IMPLEMENTATION