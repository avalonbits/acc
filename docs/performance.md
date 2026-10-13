# The code acc generates: size and speed

What acc makes of a program, against what agondev makes of the same C,
measured at 43044f6 on 2026-10-12. How acc gets there is in
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
| crc: CRC-32, CRC-16, Adler-32 | 23,806,544 | 30,524,864 | 26,299,167 | **0.78** |
| fp: Mandelbrot, float sums | 69,872,958 | 62,481,178 | 62,583,284 | 1.12 |
| interp: a switch-dispatched VM | 7,834,738 | 13,407,348 | 13,006,334 | **0.58** |
| lists: sorted list, binary tree | 9,439,512 | 8,260,818 | 6,788,141 | 1.14 |
| matmul: 20x20 ints, shorts | 5,524,177 | 4,107,449 | 2,631,815 | 1.34 |
| sieve: 16,000 bytes, thrice | 9,707,778 | 6,341,649 | 6,230,201 | 1.53 |
| sort: quicksort of 1,500 ints | 8,495,332 | 7,653,871 | 9,060,254 | 1.11 |
| wide64: 64-bit powmod, xorshift | 5,872,632 | 7,359,946 | 7,186,177 | **0.80** |
| words: split, hash, count | 2,305,234 | 2,608,855 | 2,242,120 | **0.88** |
| **zap assembling BBC BASIC** | 107,792,131 | 76,187,502 | 85,109,336 | 1.41 |
| **mean** | | | | **1.03** |

acc's code takes 1.03 times as long as agondev's `-Oz`. It is faster on
four of the ten: the checksums, the interpreter, the 64-bit arithmetic and
the word count -- the interpreter in a little over half agondev's time.
The sieve and the matrices are the furthest behind, at 1.53 and 1.34. zap
built by acc assembles BBC BASIC in 1.41 times agondev's time, 5.8 seconds
of the Agon's time against 4.1.

## Real programs

| program | acc code | -Oz | acc / -Oz | acc cycles | -Oz | acc / -Oz |
|---|---|---|---|---|---|---|
| acc compiling its inputs | 243,335 | 211,617 | 1.15 | 416,888,247 | 410,558,728 | 1.02 |
| vi substituting a file | 31,594 | 31,141 | 1.01 | 350,929,355 | 396,236,943 | **0.89** |
| AED's benchmark | 130,022 | 102,065 | 1.27 | 87,128,428 | 100,021,486 | **0.87** |
| zap assembling BBC BASIC | 85,522 | 80,033 | 1.07 | 107,789,728 | 76,184,974 | 1.42 |
| ez80asm assembling it | 64,644 | 54,086 | 1.20 | 136,307,092 | 100,597,169 | 1.35 |
| **mean** | | | **1.14** | | | **1.08** |

vi and AED built by acc run faster than agondev's builds of them, and acc
itself compiles about as fast built either way. The two assemblers are the
furthest behind: each spends its time in a few long functions that test an
operand's bytes and modes, and acc keeps their values in the frame where
agondev keeps them in registers -- the loads and stores of the frame are
most of zap's difference.

## Size

| program | acc code | -Oz | -O2 | acc / -Oz | acc image | -Oz | -O2 | acc / -Oz |
|---|---|---|---|---|---|---|---|---|
| crc | 1,096 | 701 | 1,408 | 1.56 | 7,254 | 7,808 | 8,506 | 0.93 |
| fp | 777 | 619 | 612 | 1.26 | 10,098 | 8,509 | 8,502 | 1.19 |
| interp | 1,075 | 931 | 940 | 1.15 | 7,210 | 8,000 | 8,009 | 0.90 |
| lists | 976 | 798 | 974 | 1.22 | 7,083 | 7,867 | 8,043 | 0.90 |
| matmul | 1,127 | 933 | 1,740 | 1.21 | 7,234 | 8,002 | 8,809 | 0.90 |
| sieve | 362 | 272 | 428 | 1.33 | 6,431 | 7,341 | 7,528 | 0.88 |
| sort | 1,210 | 1,000 | 1,114 | 1.21 | 7,357 | 8,085 | 8,199 | 0.91 |
| wide64 | 929 | 821 | 897 | 1.13 | 7,247 | 8,543 | 8,640 | 0.85 |
| words | 1,009 | 838 | 820 | 1.20 | 7,194 | 7,930 | 7,912 | 0.91 |
| acc | 243,335 | 211,617 | - | 1.15 | 259,690 | 222,771 | - | 1.17 |
| zap | 85,522 | 80,033 | 96,551 | 1.07 | 94,798 | 88,744 | 106,546 | 1.07 |
| vi | 31,594 | 31,141 | 56,166 | 1.01 | 48,571 | 42,076 | 67,091 | 1.15 |
| **mean** | | | | **1.20** | | | | **0.97** |

agondev cannot build acc at `-O2`: clang writes assembly for the code
generator that its own assembler refuses.

`code` is the program's own objects: what each compiler made of its C.
`image` is the whole program on the card, library and startup included,
and for the nine small programs is mostly the library -- printf above all
-- so it measures the libraries as much as the compilers. On the three real
programs acc's code is 1.01 to 1.15 times agondev's `-Oz`, and on the small
ones 1.13 to 1.56. The three real programs' whole images are 1.07 to 1.17
times agondev's, and eight of the nine small programs' are smaller.

`test/size.sh -f zap` lists zap's functions the furthest apart first; the
largest differences are the long functions and the ones agondev inlines
into their callers.
