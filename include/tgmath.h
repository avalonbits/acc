/*
 * tgmath.h -- type-generic math (C99 7.22).
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * A generic macro picks the float, double or long double function by the
 * type of its arguments, with an integer taken as a double. Here float and
 * double are one four-byte type, there is no long double and no complex,
 * so every choice it could make is the double function: an argument of
 * any real type converts to double at the call, which is what the macro
 * would have done. So the names below are the <math.h> functions
 * themselves, each also defined as itself so that `#ifdef sin` says the
 * generic macro is there.
 *
 * nexttoward is missing, since its second argument is a long double.
 */
#pragma once
#ifndef ACC_TGMATH_H
#define ACC_TGMATH_H

#include <math.h>

#define acos(x)             acos(x)
#define asin(x)             asin(x)
#define atan(x)             atan(x)
#define acosh(x)            acosh(x)
#define asinh(x)            asinh(x)
#define atanh(x)            atanh(x)
#define cos(x)              cos(x)
#define sin(x)              sin(x)
#define tan(x)              tan(x)
#define cosh(x)             cosh(x)
#define sinh(x)             sinh(x)
#define tanh(x)             tanh(x)
#define exp(x)              exp(x)
#define log(x)              log(x)
#define pow(x, y)           pow(x, y)
#define sqrt(x)             sqrt(x)
#define fabs(x)             fabs(x)

#define atan2(y, x)         atan2(y, x)
#define cbrt(x)             cbrt(x)
#define ceil(x)             ceil(x)
#define copysign(x, y)      copysign(x, y)
#define erf(x)              erf(x)
#define erfc(x)             erfc(x)
#define exp2(x)             exp2(x)
#define expm1(x)            expm1(x)
#define fdim(x, y)          fdim(x, y)
#define floor(x)            floor(x)
#define fma(x, y, z)        fma(x, y, z)
#define fmax(x, y)          fmax(x, y)
#define fmin(x, y)          fmin(x, y)
#define fmod(x, y)          fmod(x, y)
#define frexp(x, e)         frexp(x, e)
#define hypot(x, y)         hypot(x, y)
#define ilogb(x)            ilogb(x)
#define ldexp(x, e)         ldexp(x, e)
#define lgamma(x)           lgamma(x)
#define llrint(x)           llrint(x)
#define llround(x)          llround(x)
#define log10(x)            log10(x)
#define log1p(x)            log1p(x)
#define log2(x)             log2(x)
#define logb(x)             logb(x)
#define lrint(x)            lrint(x)
#define lround(x)           lround(x)
#define nearbyint(x)        nearbyint(x)
#define nextafter(x, y)     nextafter(x, y)
#define remainder(x, y)     remainder(x, y)
#define remquo(x, y, q)     remquo(x, y, q)
#define rint(x)             rint(x)
#define round(x)            round(x)
#define scalbn(x, n)        scalbn(x, n)
#define scalbln(x, n)       scalbln(x, n)
#define tgamma(x)           tgamma(x)
#define trunc(x)            trunc(x)

#endif
