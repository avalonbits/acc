/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef ACC_STDDEF_H
#define ACC_STDDEF_H

/* An eZ80 in ADL mode addresses three bytes, and acc's int is three bytes,
 * so a size and a difference between pointers are an int's width. */
typedef unsigned int size_t;
typedef int          ptrdiff_t;

#define NULL ((void *) 0)

#define offsetof(type, member) ((size_t) &((type *) 0)->member)

#endif
