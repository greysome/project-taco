CC := gcc
CFLAGS := -std=c23 -D_POSIX_C_SOURCE=200809L -lm -Wall -Wextra -Wpedantic -Wshadow -Wno-conversion -Wdouble-promotion -Wno-sign-conversion -Wno-unused-variable -Wno-unused-parameter
MYLIB_HEADERS := $(wildcard mylib/*.h)
MYLIB_OBJS = $(patsubst mylib/%.h, build/%.o, $(MYLIB_HEADERS))
HEADERS = $(wildcard *.h)
OBJS = $(patsubst %.h, build/%.o, $(HEADERS))

ifdef RELEASE
	BUILD := release
	CFLAGS += -O3 -march=native
else
	BUILD := debug
	CFLAGS += -g -fsanitize=address -fsanitize=undefined
endif

.PHONY: test clean

mmm: mmm.c $(MYLIB_OBJS) $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

test: test.c build/emulator.o build/assembler.o $(MYLIB_OBJS)
	$(CC) $(CFLAGS) $^ -o $@
	./test

$(MYLIB_OBJS): build/%.o: mylib/%.h
	@mkdir -p build
	$(CC) $(CFLAGS) -D$(shell echo $* | tr '[:lower:]' '[:upper:]')_IMPLEMENTATION -x c -c $< -o $@

$(OBJS): build/%.o: %.h
	@mkdir -p build
	$(CC) $(CFLAGS) -D$(shell echo $* | tr '[:lower:]' '[:upper:]')_IMPLEMENTATION -x c -c $< -o $@

clean:
	rm -rf build/ mmm test
