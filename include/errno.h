/*
 * errno.h -- the number a library function leaves when it fails (C99 7.5).
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The three C names, with the numbers Linux gives them, so that a number
 * printed on the Agon means what it would mean anywhere else -- and POSIX's
 * EINVAL, which strtonum leaves (C99 7.5 lets an implementation add names
 * that begin E and a digit or capital). Nothing in the library sets errno
 * to anything but these.
 *
 * One program, one thread: errno is an int in the library's data, and the
 * macro is the object itself.
 */
#pragma once
#ifndef ACC_ERRNO_H
#define ACC_ERRNO_H

#define EDOM    33              /* an argument outside a function's domain */
#define ERANGE  34              /* a result too large for its type */
#define EILSEQ  84              /* bytes that are not a character */
#define EINVAL  22              /* an argument not valid: strtonum's */

extern int errno;
#define errno errno

#endif
