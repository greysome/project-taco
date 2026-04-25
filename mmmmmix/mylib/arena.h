#ifndef ARENA_H
#define ARENA_H

#include <assert.h>
#include <errno.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include "util.h"

typedef struct {
  uintptr_t cur;
  uintptr_t end;
} Arena;

Arena arena_new(size_t n);
void *arena_alloc(size_t size, Arena *arena);

#endif // ARENA_H

#ifdef ARENA_IMPLEMENTATION

Arena arena_new(size_t n) {
  void *p = mmap(NULL, n, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  if (p == MAP_FAILED)
    die("mmap(%zu) failed: %s", n, strerror(errno));
  return (Arena) {
    .cur = (uintptr_t) p,
    .end = (uintptr_t) p + n,
  };
}

void *arena_alloc(size_t size, Arena *arena) {
  size_t padding = (size_t)(-arena->cur & (alignof(max_align_t) - 1));
  size_t actual_size = size + padding;
  if (arena->end - arena->cur < actual_size)
    die("arena %p full", arena);
  void *p = (void *)(arena->cur + padding);
  arena->cur += actual_size;
  return p;
}

#endif // ARENA_IMPLEMENTATION