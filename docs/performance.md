# The code acc generates: size and speed

What acc makes of a program, against what agondev makes of the same C,
measured at 0800827 on 2026-10-10. How acc gets there is in
[OPTIMIZATIONS.md](OPTIMIZATIONS.md).

Three scripts measure it, none part of `make test`:

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
- `test/apps.sh` builds five real programs both ways and runs each on
  the emulator doing its own job: acc compiling its benchmark inputs, Tom's
  vi substituting through a file, AED running its own benchmark, zap and
  ez80asm assembling BBC BASIC. It gives each program's own code and the
  cycles of the whole run, and every run has to leave the same output from
  both builds.

The ratios are acc's over agondev `-Oz`'s: lower is better, 1.00 is even.
The means are geometric, so no one program outweighs the rest.

## Speed

| program | acc cycles | -Oz | -O2 | acc / -Oz |
|---|---|---|---|---|
| crc: CRC-32, CRC-16, Adler-32 | 25,399,682 | 30,524,864 | 26,299,167 | **0.83** |
| fp: Mandelbrot, float sums | 67,942,773 | 62,481,178 | 62,583,284 | 1.09 |
| interp: a switch-dispatched VM | 7,326,357 | 13,407,348 | 13,006,334 | **0.55** |
| lists: sorted list, binary tree | 9,439,872 | 8,260,818 | 6,788,141 | 1.14 |
| matmul: 20x20 ints, shorts | 5,665,517 | 4,107,449 | 2,631,815 | 1.38 |
| sieve: 16,000 bytes, thrice | 10,998,593 | 6,341,649 | 6,230,201 | 1.73 |
| sort: quicksort of 1,500 ints | 8,557,016 | 7,653,871 | 9,060,254 | 1.12 |
| wide64: 64-bit powmod, xorshift | 5,829,185 | 7,359,946 | 7,186,177 | **0.79** |
| words: split, hash, count | 2,340,624 | 2,608,855 | 2,242,120 | **0.90** |
| **zap assembling BBC BASIC** | 109,079,652 | 76,187,502 | 85,109,336 | 1.43 |
| **mean** | | | | **1.04** |

acc's code takes 1.04 times as long as agondev's `-Oz`. It is faster on
four of the ten: the checksums, the interpreter, the 64-bit arithmetic and
the word count -- the interpreter in a little over half agondev's time.
The sieve and the matrices are the furthest behind, at 1.73 and 1.38. zap
built by acc assembles BBC BASIC in 1.43 times agondev's time, 5.9 seconds
of the Agon's time against 4.1.

## Real programs

| program | acc code | -Oz | acc / -Oz | acc cycles | -Oz | acc / -Oz |
|---|---|---|---|---|---|---|
| acc compiling its inputs | 253,196 | 211,617 | 1.20 | 419,838,250 | 410,560,303 | 1.02 |
| vi substituting a file | 32,800 | 31,141 | 1.05 | 351,026,768 | 396,239,988 | **0.89** |
| AED's benchmark | 138,311 | 102,065 | 1.36 | 89,273,220 | 100,023,247 | **0.89** |
| zap assembling BBC BASIC | 90,989 | 80,033 | 1.14 | 109,075,882 | 76,185,079 | 1.43 |
| ez80asm assembling it | 67,853 | 54,086 | 1.25 | 138,586,423 | 100,598,162 | 1.38 |
| **mean** | | | **1.19** | | | **1.10** |

vi and AED built by acc run faster than agondev's builds of them, and acc
itself compiles about as fast built either way. The two assemblers are the
furthest behind: each spends its time in a few long functions that test an
operand's bytes and modes, and acc keeps their values in the frame where
agondev keeps them in registers -- the loads and stores of the frame are
most of zap's difference.

## Size

| program | acc code | -Oz | -O2 | acc / -Oz | acc image | -Oz | -O2 | acc / -Oz |
|---|---|---|---|---|---|---|---|---|
| crc | 1,250 | 701 | 1,408 | 1.78 | 7,495 | 7,808 | 8,506 | 0.96 |
| fp | 925 | 619 | 612 | 1.49 | 10,320 | 8,509 | 8,502 | 1.21 |
| interp | 1,123 | 931 | 940 | 1.21 | 7,338 | 8,000 | 8,009 | 0.92 |
| lists | 1,044 | 798 | 974 | 1.31 | 7,248 | 7,867 | 8,043 | 0.92 |
| matmul | 1,224 | 933 | 1,740 | 1.31 | 7,428 | 8,002 | 8,809 | 0.93 |
| sieve | 399 | 272 | 428 | 1.47 | 6,569 | 7,341 | 7,528 | 0.89 |
| sort | 1,278 | 1,000 | 1,114 | 1.28 | 7,520 | 8,085 | 8,199 | 0.93 |
| wide64 | 1,125 | 821 | 897 | 1.37 | 7,452 | 8,543 | 8,640 | 0.87 |
| words | 1,072 | 838 | 820 | 1.28 | 7,352 | 7,930 | 7,912 | 0.93 |
| acc | 253,196 | 211,617 | - | 1.20 | 269,889 | 222,771 | - | 1.21 |
| zap | 90,989 | 80,033 | 96,551 | 1.14 | 100,311 | 88,744 | 106,546 | 1.13 |
| vi | 32,800 | 31,141 | 56,166 | 1.05 | 50,232 | 42,076 | 67,091 | 1.19 |
| **mean** | | | | **1.31** | | | | **1.00** |

agondev cannot build acc at `-O2`: clang writes assembly for the code
generator that its own assembler refuses.

`code` is the program's own objects: what each compiler made of its C.
`image` is the whole program on the card, library and startup included,
and for the nine small programs is mostly the library -- printf above all
-- so it measures the libraries as much as the compilers. On the three real
programs acc's code is 1.05 to 1.20 times agondev's `-Oz`, and on the small
ones 1.21 to 1.78. The three real programs' whole images are 1.13 to 1.21
times agondev's, and eight of the nine small programs' are smaller.

`test/size.sh -f zap` lists zap's functions the furthest apart first; the
largest differences are the long functions and the ones agondev inlines
into their callers.
