/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef ACC_TIME_H
#define ACC_TIME_H

#include <stddef.h>

/* MOS counts hundredths of a second, so that is what a clock tick is here --
 * the same figure agondev uses, and the reason a program can divide by it
 * without reaching for floating point. */
#define CLOCKS_PER_SEC 100UL

typedef unsigned long clock_t;

clock_t clock(void);

#endif
