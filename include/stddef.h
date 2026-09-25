/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef ACC_STDDEF_H
#define ACC_STDDEF_H

/* An eZ80 in ADL mode addresses three bytes, and acc's int is three bytes,
 * so a size and a difference between pointers are an int's width. */
#ifndef ACC_SIZE_T
#define ACC_SIZE_T
typedef unsigned int size_t;
#endif
typedef int          ptrdiff_t;

/* A wide character, as agondev has it: sixteen bits, signed. What L'x' is,
 * and what an L"..." string is an array of. */
#ifndef ACC_WCHAR_T
#define ACC_WCHAR_T
typedef short wchar_t;
#endif

#ifndef NULL
#define NULL ((void *) 0)
#endif

/* The compiler works this out: the usual `&((type *) 0)->member` takes the
 * address of something through a null pointer, which acc refuses as the
 * mistake it is everywhere else. */
#define offsetof(type, member) __builtin_offsetof(type, member)

#endif
