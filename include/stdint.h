/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef ACC_STDINT_H
#define ACC_STDINT_H

/* The eZ80 in ADL mode: a char is one byte, a short two, an int three and a
 * long four. Three is the odd one, and it is the machine's own width -- an
 * address is three bytes -- so it has a name of its own here as agondev
 * gives it one.
 *
 * All of C99 7.18, which a freestanding implementation has to have. Every
 * choice C leaves open -- which type is the fastest of a width, which is
 * the smallest that holds one -- is agondev's, so that a struct of them has
 * the same layout whichever of the two compiled it: here the least and the
 * fast types of a width are both the exact one. */
typedef signed char        int8_t;
typedef unsigned char      uint8_t;
typedef short              int16_t;
typedef unsigned short     uint16_t;
typedef int                int24_t;
typedef unsigned int       uint24_t;
typedef long               int32_t;
typedef unsigned long      uint32_t;
typedef long long          int64_t;
typedef unsigned long long uint64_t;

typedef signed char        int_least8_t;
typedef unsigned char      uint_least8_t;
typedef short              int_least16_t;
typedef unsigned short     uint_least16_t;
typedef long               int_least32_t;
typedef unsigned long      uint_least32_t;
typedef long long          int_least64_t;
typedef unsigned long long uint_least64_t;

typedef signed char        int_fast8_t;
typedef unsigned char      uint_fast8_t;
typedef short              int_fast16_t;
typedef unsigned short     uint_fast16_t;
typedef long               int_fast32_t;
typedef unsigned long      uint_fast32_t;
typedef long long          int_fast64_t;
typedef unsigned long long uint_fast64_t;

typedef int                intptr_t;
typedef unsigned int       uintptr_t;

typedef long long          intmax_t;
typedef unsigned long long uintmax_t;

/* What each of them holds. An int here is twenty-four bits, so INT24_MAX is
 * the odd one and the one that matters; the rest are the widths every C has.
 */
#define INT8_MIN     (-128)
#define INT8_MAX     127
#define UINT8_MAX    255
#define INT16_MIN    (-32768)
#define INT16_MAX    32767
#define UINT16_MAX   65535
#define INT24_MIN    (-8388608)
#define INT24_MAX    8388607
#define UINT24_MAX   16777215u
#define INT32_MIN    (-2147483647L - 1L)
#define INT32_MAX    2147483647L
#define UINT32_MAX   4294967295UL
#define INT64_MIN    (-9223372036854775807LL - 1LL)
#define INT64_MAX    9223372036854775807LL
#define UINT64_MAX   18446744073709551615ULL

#define INT_LEAST8_MIN    INT8_MIN
#define INT_LEAST8_MAX    INT8_MAX
#define UINT_LEAST8_MAX   UINT8_MAX
#define INT_LEAST16_MIN   INT16_MIN
#define INT_LEAST16_MAX   INT16_MAX
#define UINT_LEAST16_MAX  UINT16_MAX
#define INT_LEAST32_MIN   INT32_MIN
#define INT_LEAST32_MAX   INT32_MAX
#define UINT_LEAST32_MAX  UINT32_MAX
#define INT_LEAST64_MIN   INT64_MIN
#define INT_LEAST64_MAX   INT64_MAX
#define UINT_LEAST64_MAX  UINT64_MAX

#define INT_FAST8_MIN     INT8_MIN
#define INT_FAST8_MAX     INT8_MAX
#define UINT_FAST8_MAX    UINT8_MAX
#define INT_FAST16_MIN    INT16_MIN
#define INT_FAST16_MAX    INT16_MAX
#define UINT_FAST16_MAX   UINT16_MAX
#define INT_FAST32_MIN    INT32_MIN
#define INT_FAST32_MAX    INT32_MAX
#define UINT_FAST32_MAX   UINT32_MAX
#define INT_FAST64_MIN    INT64_MIN
#define INT_FAST64_MAX    INT64_MAX
#define UINT_FAST64_MAX   UINT64_MAX

#define INTPTR_MIN   INT24_MIN
#define INTPTR_MAX   INT24_MAX
#define UINTPTR_MAX  UINT24_MAX

#define INTMAX_MIN   INT64_MIN
#define INTMAX_MAX   INT64_MAX
#define UINTMAX_MAX  UINT64_MAX

/* The limits of the other integer types C99 7.18.3 puts here: ptrdiff_t is
 * an int, sig_atomic_t is one in agondev's <signal.h>, wchar_t is a short
 * and wint_t an int, all as agondev has them. */
#define PTRDIFF_MIN     INT24_MIN
#define PTRDIFF_MAX     INT24_MAX
#define SIG_ATOMIC_MIN  INT24_MIN
#define SIG_ATOMIC_MAX  INT24_MAX
#define SIZE_MAX        UINT24_MAX
#ifndef WCHAR_MIN               /* <wchar.h> has them too, spelled the same */
#define WCHAR_MIN       (-32768)
#define WCHAR_MAX       32767
#endif
#define WINT_MIN        INT24_MIN
#define WINT_MAX        INT24_MAX

/* A constant of the least type of a width, promoted: 8 and 16 bits need
 * no suffix, since either is an int once promoted. */
#define INT8_C(c)    c
#define UINT8_C(c)   c
#define INT16_C(c)   c
#define UINT16_C(c)  c
#define INT32_C(c)   c ## L
#define UINT32_C(c)  c ## UL
#define INT64_C(c)   c ## LL
#define UINT64_C(c)  c ## ULL
#define INTMAX_C(c)  c ## LL
#define UINTMAX_C(c) c ## ULL

/* Here as well as in <stddef.h>, because that is where agondev's puts it and
 * programs written against those headers reach for it after including this
 * one. Guarded, so that including both says it once. */
#ifndef NULL
#define NULL ((void *) 0)
#endif

#endif
