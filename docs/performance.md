# The code acc generates: size and speed

What acc makes of a program, against what agondev makes of the same C,
measured at 15d17a4 on 2026-10-05. How acc gets there is in
[OPTIMIZATIONS.md](OPTIMIZATIONS.md).

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
  at the `c99-first-milestone` tag; zap, at a tag from its checkout; and
  Tom's vi, fetched at a pinned commit -- and gives each program's own code
  and its whole image. `-f <name>` sets one program's functions side by
  side; acc's `-map` names them, statics included.

The ratios are acc's over agondev `-Oz`'s: lower is better, 1.00 is even.
The means are geometric, so no one program outweighs the rest.

## Speed

| program | acc cycles | -Oz | -O2 | acc / -Oz |
|---|---|---|---|---|
| crc: CRC-32, CRC-16, Adler-32 | 25,398,803 | 30,524,444 | 26,298,852 | **0.83** |
| fp: Mandelbrot, float sums | 67,940,057 | 62,480,548 | 62,582,864 | 1.09 |
| interp: a switch-dispatched VM | 8,682,132 | 13,407,138 | 13,006,334 | **0.65** |
| lists: sorted list, binary tree | 9,517,781 | 8,260,713 | 6,788,141 | 1.15 |
| matmul: 20x20 ints, shorts | 6,671,269 | 4,107,449 | 2,631,815 | 1.62 |
| sieve: 16,000 bytes, thrice | 10,998,292 | 6,341,544 | 6,230,201 | 1.73 |
| sort: quicksort of 1,500 ints | 8,571,167 | 7,653,661 | 9,060,149 | 1.12 |
| wide64: 64-bit powmod, xorshift | 5,841,894 | 7,359,946 | 7,186,072 | **0.79** |
| words: split, hash, count | 2,452,832 | 2,608,960 | 2,242,120 | **0.94** |
| **zap assembling BBC BASIC** | 113,890,836 | 76,187,506 | 85,109,340 | 1.49 |
| **mean** | | | | **1.09** |

acc's code takes 1.09 times as long as agondev's `-Oz`. It is faster on
four of the ten: the checksums, the interpreter, the 64-bit arithmetic and
the word count. The sieve and the matrices are the furthest behind, at 1.73
and 1.62. zap built by acc assembles BBC BASIC in 1.49 times agondev's
time, 6.2 seconds of the Agon's time against 4.1.

## Size

| program | acc code | -Oz | -O2 | acc / -Oz | acc image | -Oz | -O2 | acc / -Oz |
|---|---|---|---|---|---|---|---|---|
| crc | 1,225 | 701 | 1,408 | 1.75 | 7,464 | 7,808 | 8,506 | 0.96 |
| fp | 920 | 619 | 612 | 1.49 | 10,309 | 8,509 | 8,502 | 1.21 |
| interp | 1,195 | 931 | 940 | 1.28 | 7,404 | 8,000 | 8,009 | 0.93 |
| lists | 1,019 | 798 | 974 | 1.28 | 7,217 | 7,867 | 8,043 | 0.92 |
| matmul | 1,219 | 933 | 1,740 | 1.31 | 7,417 | 8,002 | 8,809 | 0.93 |
| sieve | 394 | 272 | 428 | 1.45 | 6,558 | 7,341 | 7,528 | 0.89 |
| sort | 1,263 | 1,000 | 1,114 | 1.26 | 7,499 | 8,085 | 8,199 | 0.93 |
| wide64 | 1,110 | 821 | 897 | 1.35 | 7,431 | 8,543 | 8,640 | 0.87 |
| words | 1,052 | 838 | 820 | 1.26 | 7,326 | 7,930 | 7,912 | 0.92 |
| acc | 251,321 | 211,617 | - | 1.19 | 267,131 | 222,771 | - | 1.20 |
| zap | 90,705 | 80,033 | 96,551 | 1.13 | 99,273 | 88,744 | 106,546 | 1.12 |
| vi | 32,743 | 31,141 | 56,166 | 1.05 | 49,252 | 42,076 | 67,091 | 1.17 |
| **mean** | | | | **1.31** | | | | **1.00** |

agondev cannot build acc at `-O2`: clang writes assembly for the code
generator that its own assembler refuses.

`code` is the program's own objects: what each compiler made of its C.
`image` is the whole program on the card, library and startup included,
and for the nine small programs is mostly the library -- printf above all
-- so it measures the libraries as much as the compilers. On the three real
programs acc's code is 1.05 to 1.19 times agondev's `-Oz`, and on the small
ones 1.26 to 1.75. The three real programs' whole images are 1.12 to 1.20
times agondev's, and eight of the nine small programs' are smaller.

`test/size.sh -f zap` lists zap's functions the furthest apart first; the
largest differences are the long functions and the ones agondev inlines
into their callers.
