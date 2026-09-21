/*
 * limits.h -- the ranges of this machine's integer types.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * An int here is twenty-four bits, which is the width of a register and of
 * an address: that is the whole reason acc exists, and it is why INT_MAX is
 * 8388607 and not the 2147483647 a program written elsewhere expects. A long
 * is thirty-two bits and a long long sixty-four, as agondev has them.
 *
 * char is signed, as it is on this chip.
 */
#ifndef ACC_LIMITS_H
#define ACC_LIMITS_H

#define CHAR_BIT    8

#define SCHAR_MIN   (-128)
#define SCHAR_MAX   127
#define UCHAR_MAX   255
#define CHAR_MIN    SCHAR_MIN
#define CHAR_MAX    SCHAR_MAX

#define SHRT_MIN    (-32768)
#define SHRT_MAX    32767
#define USHRT_MAX   65535

#define INT_MIN     (-8388608)
#define INT_MAX     8388607
#define UINT_MAX    16777215u

#define LONG_MIN    (-2147483647L - 1L)
#define LONG_MAX    2147483647L
#define ULONG_MAX   4294967295UL

#define LLONG_MIN   (-9223372036854775807LL - 1LL)
#define LLONG_MAX   9223372036854775807LL
#define ULLONG_MAX  18446744073709551615ULL

#define MB_LEN_MAX  1

#endif
