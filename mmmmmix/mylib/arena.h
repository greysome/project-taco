#ifndef ARENA_H
#define ARENA_H

#include <assert.h>
#include <errno.h>
#include <stdalign.h> // for max_align_t
#include <stddef.h>   // for size_t
#include <stdint.h>   // for uintptr_t
#include <string.h>   // for strerror
#include <sys/mman.h> // for mmap

// From io.h; prevent circular dependency
extern void _die(const char *file, int line, char *fmt, ...);
#define die(fmt, ...) _die(__FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)

typedef struct {
  uintptr_t cur;
  uintptr_t end;
} Arena;

Arena arena_new(size_t n);
void *arena_alloc(size_t size, Arena *arena);

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
    die("arena %p full: could not alloc %zu bytes", arena, actual_size);
  void *p = (void *)(arena->cur + padding);
  arena->cur += actual_size;
  return p;
}

#endif // ARENA_IMPLEMENTATION
#endif // ARENA_H
