# The code acc generates: size and speed

What acc makes of a program, against what agondev makes of the same C. This
page is the baseline, taken at b582980 on 2026-09-25, before any work on
either; it is how the work that follows will be judged.

Two scripts measure it, neither part of `make test`:

- `test/perf.sh` times nine programs in `test/perf`, each doing one kind of
  work from a seed neither compiler can see through, by the emulator's own
  count of cycles over the work alone. Each is built by acc and by agondev
  at `-Oz` and `-O2`, and every build has to give the same answer -- or the
  answer the program says it has to give, where one has been worked out
  apart from either compiler. And one real program: zap, built the same
  three ways, assembling BBC BASIC for the Agon -- its twenty files, from
  zap's repository -- timed over the whole command, loading included, and
  every build has to write the same binary.
- `test/size.sh` builds those nine and three real programs -- acc itself,
  at the milestone tag; zap, at a tag from its checkout; and Tom's vi,
  fetched at a pinned commit -- and gives
  each program's own code and its whole image. `-f <name>` sets one
  program's functions side by side; acc's `-map` names them, statics
  included.

The ratios are acc's over agondev `-Oz`'s: lower is better, 1.00 is even.
The means are geometric, so no one program outweighs the rest.

## Speed

| program | acc cycles | -Oz | -O2 | acc / -Oz |
|---|---|---|---|---|
| crc: CRC-32, CRC-16, Adler-32 | 124,373,074 | 30,524,444 | 26,298,957 | 4.07 |
| fp: Mandelbrot, float sums | 132,152,265 | 62,480,548 | 62,582,759 | 2.12 |
| interp: a switch-dispatched VM | 10,807,281 | 13,407,138 | 13,006,334 | **0.81** |
| lists: sorted list, binary tree | 11,616,270 | 8,260,713 | 6,788,141 | 1.41 |
| matmul: 20x20 ints, shorts | 7,677,782 | 4,107,449 | 2,631,815 | 1.87 |
| sieve: 16,000 bytes, thrice | 13,667,463 | 6,341,544 | 6,230,096 | 2.16 |
| sort: quicksort of 1,500 ints | 16,149,209 | 7,653,661 | 9,060,149 | 2.11 |
| wide64: 64-bit powmod, xorshift | 6,351,230 | 7,359,946 | 7,186,282 | **0.86** |
| words: split, hash, count | 3,341,472 | (4,255,398) | (3,976,344) | (0.79) |
| **zap assembling BBC BASIC** | 220,021,572 | 76,187,603 | 85,109,437 | **2.89** |
| **mean** | | | | **1.81** |

acc's code takes 1.8 times as long as agondev's, and ranges from faster --
the interpreter, the 64-bit arithmetic -- to four times as long, on the
checksums, which are unsigned long arithmetic and table lookups. The real
program is the one that says most: zap built by acc takes 2.9 times as long
to assemble BBC BASIC as zap built by agondev, 11.9 seconds of the Agon's
time against 4.1.

agondev gets `words` wrong, at both levels: its `-Oz` reads the byte after
the one `*p++` names in the hash, and its `-O2` comes to the same wrong
table. acc's answer is the one a model of the program with this machine's
widths gives. Its times are shown in brackets and left out of the mean.

Since this baseline, `words` hashes with `p[i]` rather than `*p++`, which
agondev compiles correctly: all three builds give the answer, and it is
in the mean. agondev's time for it then was for different work -- a wrong
table has other collisions -- so the 0.79 above is not a comparison.

## Size

| program | acc code | -Oz | -O2 | acc / -Oz | acc image | -Oz | -O2 | acc / -Oz |
|---|---|---|---|---|---|---|---|---|
| crc | 2,097 | 701 | 1,408 | 2.99 | 15,900 | 7,808 | 8,506 | 2.04 |
| fp | 1,552 | 619 | 612 | 2.51 | 15,355 | 8,509 | 8,502 | 1.80 |
| interp | 1,619 | 931 | 940 | 1.74 | 15,422 | 8,000 | 8,009 | 1.93 |
| lists | 1,595 | 798 | 974 | 2.00 | 15,398 | 7,867 | 8,043 | 1.96 |
| matmul | 1,774 | 933 | 1,740 | 1.90 | 15,577 | 8,002 | 8,809 | 1.95 |
| sieve | 536 | 272 | 428 | 1.97 | 14,339 | 7,341 | 7,528 | 1.95 |
| sort | 1,738 | 1,000 | 1,114 | 1.74 | 15,541 | 8,085 | 8,199 | 1.92 |
| wide64 | 2,758 | 821 | 897 | 3.36 | 16,561 | 8,543 | 8,640 | 1.94 |
| words | 1,652 | 821 | 833 | 2.01 | 15,455 | 7,913 | 7,925 | 1.95 |
| zap | 144,352 | 80,033 | 96,551 | 1.80 | 160,670 | 88,744 | 106,546 | 1.81 |
| vi | 45,477 | 31,141 | 56,166 | 1.46 | 74,081 | 42,076 | 67,091 | 1.76 |
| acc | 352,505 | 211,617 | - | 1.67 | 379,438 | 222,771 | - | 1.70 |
| **mean** | | | | **2.03** | | | | **1.89** |

agondev cannot build acc at `-O2`: clang writes assembly for gen.c that its
own assembler refuses.

`code` is the program's own objects: what each compiler made of its C.
`image` is the whole program on the card, library and startup included,
and for the nine small programs is mostly the library -- printf above all
-- so it measures the libraries as much as the compilers. On the three real
programs acc's code is 1.5 to 1.8 times agondev's `-Oz` -- acc's own, 1.67
-- and on the small ones about twice.

`test/size.sh -f zap` lists zap's functions the furthest apart first; the
largest differences are the long functions -- `directive_line`, 7,011 bytes
against 3,699 -- and the ones agondev inlines into their callers.
