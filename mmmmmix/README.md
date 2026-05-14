# Module for Manipulating the Mythical Machine MIX

This is a MIX emulator I wrote to follow along the code examples in Knuth's _The Art of Computer Programming_. Actually, there is an emulator, an assembler and a debugger called the _MIX Management Module_ (which is also used to run MIXAL programs).

## Installation

There are two executables: `mmm` the MIX Management Module and `test`, which just runs a series of asserts to sanity-check that the emulator and assembler work as intended. They can be built via `make` and `make test` respectively. The only dependency is the C standard library, and I compile with C17 (older versions of C will probably work too).

## Basic usage

```
> mmm program.mixal
Loaded MIXAL file program.mixal
MIX Management Module, by wyan
Type h for help
>>
```

The most basic task is to run a program as-is. This can be done by typing `g` in the prompt. Note that after the program is done, `g` does nothing; to run again, send `l` first to reset the MIX machine. The full list of commands is displayed with `h`.

**NOTE**: Only a few I/O devices have been implemented currently, namely the card reader, line printer and tape units. The `IOC` operation in MIX has not been fully implemented for tape units.

## Card format

A card file (`.cards`) consists of a series of words, and a word is a sequence of 5 bytes encoded in MIX's character set, with one modification: the characters $\Delta$, $\Sigma$ and $\Pi$ (corresponding to codes 10,20,21) are replaced with `!`, `[` and `]`, for compatibility with ASCII.

There must be a newline after every 16 words, or 80 characters.

## Tape format

A tape file (`.tape`) consists of a series of words. Unlike cards however, the sign is also stored at the start of each word: `#` for positive and `~` for negative (because `+` and `-` are already used in the character set). The encoding of bytes is exactly the same as in cards.

There must be a newline after every 100 words, or 600 characters.