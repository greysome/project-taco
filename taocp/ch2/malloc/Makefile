CFLAGS=-g -fsanitize=address -fsanitize=undefined

btmalloc: btmalloc.c bench.c common.h
	gcc -o $@ $(CFLAGS) btmalloc.c
