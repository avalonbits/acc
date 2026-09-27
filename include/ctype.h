/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#ifndef ACC_CTYPE_H
#define ACC_CTYPE_H

/* C99 7.4. Each takes a value that fits in an unsigned char, or EOF, and
 * anything else is undefined -- which matters here, because a program that
 * hands one a plain `char` on a machine where char is signed has handed it a
 * negative number for every byte past 127. These are written to answer 0 for
 * one rather than to read off the end of anything, but the program is still
 * wrong and the cast at the call is still the fix.
 *
 * Functions rather than macros: C99 allows either, and a macro cannot have
 * its address taken. They are one comparison or two each. */
int isalnum(int c);
int isalpha(int c);
int isblank(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);

int tolower(int c);
int toupper(int c);

#endif
