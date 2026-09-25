/* What each program in test/perf shares: a way to time its work that both
 * compilers understand, a seed the compiler cannot see through, and the
 * line it ends with.
 *
 * The time is the emulator's own count of cycles: a write to IO port 0x40
 * starts it and one to 0x41 prints it -- fab-agon-emulator 2037657 and
 * later; see test/perf.sh. acc reaches the ports through io_out, agondev
 * through its IO() macro. Only the work is timed: setting up and printing
 * are outside, and the work calls nothing in the C library, so what is
 * measured is each compiler's code and not its library.
 *
 * The seed is read from a volatile, so neither compiler can work the
 * answer out while compiling -- which clang otherwise does to any program
 * whose inputs are all constants. */
#ifndef PERF_H
#define PERF_H

#include <ez80f92.h>
#include <stdio.h>

#ifdef __clang__
#define perf_start() (IO(0x40) = 0)
#define perf_stop()  (IO(0x41) = 0)
#else
#define perf_start() io_out(0x40, 0)
#define perf_stop()  io_out(0x41, 0)
#endif

static volatile unsigned long perf_seed = 12345;

/* The check both builds have to agree on, printed after the work. */
#define perf_check(value) printf("check %lu\n", (unsigned long) (value))

#endif
