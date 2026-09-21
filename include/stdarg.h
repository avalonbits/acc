/*
 * stdarg.h -- reading the arguments a function was not declared to take.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Nothing is declared here. va_list, va_start, va_arg, va_end and va_copy
 * are acc's own words -- it has to know the width of what is being read to
 * step over it, which no macro written in C can tell it -- so this header
 * exists to be included and not to define them. A program that includes it
 * gets what it asked for either way, which is what portable source expects.
 */
#ifndef ACC_STDARG_H
#define ACC_STDARG_H
#endif
