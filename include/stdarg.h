/*
 * stdarg.h -- reading the arguments a function was not declared to take.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The type and the four macros C99 7.15 names, over acc's own words for
 * them: va_arg has to know the width of what it reads to step over it,
 * which no macro written in C can tell it. Those words are spelled as names
 * reserved to the implementation, so a program that does not include this
 * may use va_list and the rest for itself, and one that does can ask
 * whether va_copy is defined.
 */
#pragma once
#ifndef ACC_STDARG_H
#define ACC_STDARG_H

typedef __va_list va_list;

#define va_start(ap, last)  __va_start(ap, last)
#define va_arg(ap, type)    __va_arg(ap, type)
#define va_end(ap)          __va_end(ap)
#define va_copy(to, from)   __va_copy(to, from)

#endif
