/*
 * inttypes.h -- format conversions of integer types (C99 7.8).
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The printf and scanf lengths for <stdint.h>'s types, and the intmax_t
 * functions. The widths are this machine's: 8 bits is a char, 16 a short,
 * 24 an int -- and so a pointer -- 32 a long and 64 a long long. For printf
 * anything narrower than an int arrives as one, so those have no length;
 * scanf stores through a pointer and has to be told.
 */
#pragma once
#ifndef ACC_INTTYPES_H
#define ACC_INTTYPES_H

/* wcstoimax takes wide strings, and C99 does not give this header the
 * name wchar_t: the type under a name of the implementation's. */
typedef short __acc_wchar;
#include <stdint.h>

#define PRId8 "d"
#define PRIdLEAST8 "d"
#define PRIdFAST8 "d"
#define PRId16 "d"
#define PRIdLEAST16 "d"
#define PRIdFAST16 "d"
#define PRId24 "d"
#define PRId32 "ld"
#define PRIdLEAST32 "ld"
#define PRIdFAST32 "ld"
#define PRId64 "lld"
#define PRIdLEAST64 "lld"
#define PRIdFAST64 "lld"
#define PRIdMAX "lld"
#define PRIdPTR "d"

#define PRIi8 "i"
#define PRIiLEAST8 "i"
#define PRIiFAST8 "i"
#define PRIi16 "i"
#define PRIiLEAST16 "i"
#define PRIiFAST16 "i"
#define PRIi24 "i"
#define PRIi32 "li"
#define PRIiLEAST32 "li"
#define PRIiFAST32 "li"
#define PRIi64 "lli"
#define PRIiLEAST64 "lli"
#define PRIiFAST64 "lli"
#define PRIiMAX "lli"
#define PRIiPTR "i"

#define PRIo8 "o"
#define PRIoLEAST8 "o"
#define PRIoFAST8 "o"
#define PRIo16 "o"
#define PRIoLEAST16 "o"
#define PRIoFAST16 "o"
#define PRIo24 "o"
#define PRIo32 "lo"
#define PRIoLEAST32 "lo"
#define PRIoFAST32 "lo"
#define PRIo64 "llo"
#define PRIoLEAST64 "llo"
#define PRIoFAST64 "llo"
#define PRIoMAX "llo"
#define PRIoPTR "o"

#define PRIu8 "u"
#define PRIuLEAST8 "u"
#define PRIuFAST8 "u"
#define PRIu16 "u"
#define PRIuLEAST16 "u"
#define PRIuFAST16 "u"
#define PRIu24 "u"
#define PRIu32 "lu"
#define PRIuLEAST32 "lu"
#define PRIuFAST32 "lu"
#define PRIu64 "llu"
#define PRIuLEAST64 "llu"
#define PRIuFAST64 "llu"
#define PRIuMAX "llu"
#define PRIuPTR "u"

#define PRIx8 "x"
#define PRIxLEAST8 "x"
#define PRIxFAST8 "x"
#define PRIx16 "x"
#define PRIxLEAST16 "x"
#define PRIxFAST16 "x"
#define PRIx24 "x"
#define PRIx32 "lx"
#define PRIxLEAST32 "lx"
#define PRIxFAST32 "lx"
#define PRIx64 "llx"
#define PRIxLEAST64 "llx"
#define PRIxFAST64 "llx"
#define PRIxMAX "llx"
#define PRIxPTR "x"

#define PRIX8 "X"
#define PRIXLEAST8 "X"
#define PRIXFAST8 "X"
#define PRIX16 "X"
#define PRIXLEAST16 "X"
#define PRIXFAST16 "X"
#define PRIX24 "X"
#define PRIX32 "lX"
#define PRIXLEAST32 "lX"
#define PRIXFAST32 "lX"
#define PRIX64 "llX"
#define PRIXLEAST64 "llX"
#define PRIXFAST64 "llX"
#define PRIXMAX "llX"
#define PRIXPTR "X"

#define SCNd8 "hhd"
#define SCNdLEAST8 "hhd"
#define SCNdFAST8 "hhd"
#define SCNd16 "hd"
#define SCNdLEAST16 "hd"
#define SCNdFAST16 "hd"
#define SCNd24 "d"
#define SCNd32 "ld"
#define SCNdLEAST32 "ld"
#define SCNdFAST32 "ld"
#define SCNd64 "lld"
#define SCNdLEAST64 "lld"
#define SCNdFAST64 "lld"
#define SCNdMAX "lld"
#define SCNdPTR "d"

#define SCNi8 "hhi"
#define SCNiLEAST8 "hhi"
#define SCNiFAST8 "hhi"
#define SCNi16 "hi"
#define SCNiLEAST16 "hi"
#define SCNiFAST16 "hi"
#define SCNi24 "i"
#define SCNi32 "li"
#define SCNiLEAST32 "li"
#define SCNiFAST32 "li"
#define SCNi64 "lli"
#define SCNiLEAST64 "lli"
#define SCNiFAST64 "lli"
#define SCNiMAX "lli"
#define SCNiPTR "i"

#define SCNo8 "hho"
#define SCNoLEAST8 "hho"
#define SCNoFAST8 "hho"
#define SCNo16 "ho"
#define SCNoLEAST16 "ho"
#define SCNoFAST16 "ho"
#define SCNo24 "o"
#define SCNo32 "lo"
#define SCNoLEAST32 "lo"
#define SCNoFAST32 "lo"
#define SCNo64 "llo"
#define SCNoLEAST64 "llo"
#define SCNoFAST64 "llo"
#define SCNoMAX "llo"
#define SCNoPTR "o"

#define SCNu8 "hhu"
#define SCNuLEAST8 "hhu"
#define SCNuFAST8 "hhu"
#define SCNu16 "hu"
#define SCNuLEAST16 "hu"
#define SCNuFAST16 "hu"
#define SCNu24 "u"
#define SCNu32 "lu"
#define SCNuLEAST32 "lu"
#define SCNuFAST32 "lu"
#define SCNu64 "llu"
#define SCNuLEAST64 "llu"
#define SCNuFAST64 "llu"
#define SCNuMAX "llu"
#define SCNuPTR "u"

#define SCNx8 "hhx"
#define SCNxLEAST8 "hhx"
#define SCNxFAST8 "hhx"
#define SCNx16 "hx"
#define SCNxLEAST16 "hx"
#define SCNxFAST16 "hx"
#define SCNx24 "x"
#define SCNx32 "lx"
#define SCNxLEAST32 "lx"
#define SCNxFAST32 "lx"
#define SCNx64 "llx"
#define SCNxLEAST64 "llx"
#define SCNxFAST64 "llx"
#define SCNxMAX "llx"
#define SCNxPTR "x"

typedef struct {
    intmax_t quot;
    intmax_t rem;
} imaxdiv_t;

intmax_t  imaxabs(intmax_t j);
imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom);

intmax_t  strtoimax(const char *s, char **end, int base);
uintmax_t strtoumax(const char *s, char **end, int base);
intmax_t  wcstoimax(const __acc_wchar *s, __acc_wchar **end, int base);
uintmax_t wcstoumax(const __acc_wchar *s, __acc_wchar **end, int base);

#endif
