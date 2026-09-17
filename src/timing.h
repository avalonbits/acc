/*
 * How long a compile took.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 *
 * The same reading zap reports, measured the same way, so that a timing from
 * one can be compared against a timing from the other.
 */

#ifndef ACC_TIMING_H
#define ACC_TIMING_H

#include <time.h>

/* Hundredths of a second between two clock readings.
 *
 * CLOCKS_PER_SEC is 100 on the Agon and 1000000 on a host, and both divide by
 * 100 exactly, so this needs no floating point. That matters on the eZ80,
 * where a single %f would pull the whole float formatter into a binary whose
 * whole point is to leave the heap as much room as possible.
 *
 * Anything shorter than a hundredth reads as zero.
 *
 * It lives in a header so it can be tested against known clock values rather
 * than against how long a real compile happens to take, which depends on the
 * machine running the tests. */
static inline unsigned elapsed_cs(clock_t begin, clock_t end)
{
    return (unsigned) ((end - begin) / (CLOCKS_PER_SEC / 100));
}

#endif  /* ACC_TIMING_H */
