# The code acc generates: size and speed

What acc makes of a program, against what agondev makes of the same C,
measured at d268584 on 2026-09-28. How acc gets there is in
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
| crc: CRC-32, CRC-16, Adler-32 | 25,499,956 | 30,524,339 | 26,298,852 | **0.84** |
| fp: Mandelbrot, float sums | 130,319,442 | 62,480,443 | 62,582,759 | 2.09 |
| interp: a switch-dispatched VM | 8,686,824 | 13,407,138 | 13,006,334 | **0.65** |
| lists: sorted list, binary tree | 9,719,878 | 8,260,713 | 6,788,141 | 1.18 |
| matmul: 20x20 ints, shorts | 6,708,871 | 4,107,449 | 2,631,815 | 1.63 |
| sieve: 16,000 bytes, thrice | 10,998,294 | 6,341,439 | 6,230,096 | 1.73 |
| sort: quicksort of 1,500 ints | 8,994,282 | 7,653,661 | 9,060,149 | 1.18 |
| wide64: 64-bit powmod, xorshift | 5,841,896 | 7,359,946 | 7,186,072 | **0.79** |
| words: split, hash, count | 2,555,203 | 2,608,855 | 2,242,120 | **0.98** |
| **zap assembling BBC BASIC** | 120,421,891 | 76,187,319 | 85,109,258 | 1.58 |
| **mean** | | | | **1.18** |

acc's code takes 1.18 times as long as agondev's `-Oz`. It is faster on
four of the ten: the checksums, the interpreter, the 64-bit arithmetic and
the word count. The floating-point program is the furthest behind, at
twice agondev's time. zap built by acc assembles BBC
BASIC in 1.58 times agondev's time, 6.5 seconds of the Agon's time against
4.1.

## Size

| program | acc code | -Oz | -O2 | acc / -Oz | acc image | -Oz | -O2 | acc / -Oz |
|---|---|---|---|---|---|---|---|---|
| crc | 1,234 | 701 | 1,408 | 1.76 | 7,993 | 7,808 | 8,506 | 1.02 |
| fp | 931 | 619 | 612 | 1.50 | 10,453 | 8,509 | 8,502 | 1.23 |
| interp | 1,199 | 931 | 940 | 1.29 | 8,022 | 8,000 | 8,009 | 1.00 |
| lists | 1,033 | 798 | 974 | 1.29 | 7,845 | 7,867 | 8,043 | 1.00 |
| matmul | 1,219 | 933 | 1,740 | 1.31 | 8,064 | 8,002 | 8,809 | 1.01 |
| sieve | 394 | 272 | 428 | 1.45 | 7,078 | 7,341 | 7,528 | 0.96 |
| sort | 1,290 | 1,000 | 1,114 | 1.29 | 8,136 | 8,085 | 8,199 | 1.01 |
| wide64 | 1,110 | 821 | 897 | 1.35 | 7,437 | 8,543 | 8,640 | 0.87 |
| words | 1,082 | 838 | 820 | 1.29 | 8,019 | 7,930 | 7,912 | 1.01 |
| acc | 261,373 | 211,617 | - | 1.24 | 277,870 | 222,771 | - | 1.25 |
| zap | 97,269 | 80,033 | 96,551 | 1.22 | 106,628 | 88,744 | 106,546 | 1.20 |
| vi | 33,838 | 31,141 | 56,166 | 1.09 | 51,019 | 42,076 | 67,091 | 1.21 |
| **mean** | | | | **1.33** | | | | **1.06** |

agondev cannot build acc at `-O2`: clang writes assembly for the code
generator that its own assembler refuses.

`code` is the program's own objects: what each compiler made of its C.
`image` is the whole program on the card, library and startup included,
and for the nine small programs is mostly the library -- printf above all
-- so it measures the libraries as much as the compilers. On the three real
programs acc's code is 1.09 to 1.24 times agondev's `-Oz`, and on the small
ones 1.29 to 1.76. Whole images are within a quarter of agondev's, and
smaller for three of the small programs.

`test/size.sh -f zap` lists zap's functions the furthest apart first; the
largest differences are the long functions and the ones agondev inlines
into their callers.
