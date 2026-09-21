/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef ACC_STDINT_H
#define ACC_STDINT_H

/* The eZ80 in ADL mode: a char is one byte, a short two, an int three and a
 * long four. Three is the odd one, and it is the machine's own width -- an
 * address is three bytes -- so it has a name of its own here as agondev
 * gives it one. */
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

typedef int                intptr_t;
typedef unsigned int       uintptr_t;

/* Here as well as in <stddef.h>, because that is where agondev's puts it and
 * programs written against those headers reach for it after including this
 * one. Guarded, so that including both says it once. */
#ifndef NULL
#define NULL ((void *) 0)
#endif

#define INT8_MIN   (-128)
#define INT8_MAX   127
#define UINT8_MAX  255
#define INT16_MIN  (-32768)
#define INT16_MAX  32767
#define UINT16_MAX 65535
#define INT24_MIN  (-8388608)
#define INT24_MAX  8388607
#define UINT24_MAX 16777215U
#define INT32_MIN  (-2147483647L - 1)
#define INT32_MAX  2147483647L
#define UINT32_MAX 4294967295UL

#endif
