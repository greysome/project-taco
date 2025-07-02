CFLAGS = -g -fsanitize=address -fsanitize=undefined -DDECIMAL

all: mmm
mmm: mmm.c analyzeprogram.c emulator.c assembler.c
test: test.c emulator.c assembler.c
clean:
	rm -f test mmm