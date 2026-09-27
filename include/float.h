/*
 * float.h -- the characteristics of this machine's floating types.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * float and double are the same type here: IEEE single precision, four
 * bytes, as agondev has them. So every DBL_ value is the FLT_ one, and
 * says what the machine does rather than what C99 asks of a double --
 * 5.2.4.2.2 wants DBL_DIG to be at least 10 and DBL_EPSILON at most 1e-9,
 * and a four-byte float gives 6 and about 1.2e-7. A program that needs
 * that precision needs to know it will not get it, and a header that
 * claimed otherwise would only hide the fact.
 *
 * long double is agondev's IEEE double, eight bytes, and its values are
 * given as agondev gives them. acc refuses long double -- agondev's library
 * has no arithmetic for one -- so the integer LDBL_ values can be read and
 * the floating ones, being long double constants, draw the same refusal as
 * the type does. DECIMAL_DIG is long double's, as the widest floating type.
 *
 * FLT_ROUNDS is 1, to nearest with ties to even, which is what acc's
 * arithmetic and its constant folding both do. FLT_EVAL_METHOD is 0: every
 * operation is carried out in its own type, which here is always the four
 * bytes.
 */
#pragma once
#ifndef ACC_FLOAT_H
#define ACC_FLOAT_H

#define FLT_RADIX        2
#define FLT_ROUNDS       1
#define FLT_EVAL_METHOD  0
#define DECIMAL_DIG      17

#define FLT_MANT_DIG     24
#define FLT_DIG          6
#define FLT_MIN_EXP      (-125)
#define FLT_MIN_10_EXP   (-37)
#define FLT_MAX_EXP      128
#define FLT_MAX_10_EXP   38
#define FLT_MAX          3.40282347e+38F
#define FLT_EPSILON      1.19209290e-7F
#define FLT_MIN          1.17549435e-38F

#define DBL_MANT_DIG     24
#define DBL_DIG          6
#define DBL_MIN_EXP      (-125)
#define DBL_MIN_10_EXP   (-37)
#define DBL_MAX_EXP      128
#define DBL_MAX_10_EXP   38
#define DBL_MAX          3.40282347e+38
#define DBL_EPSILON      1.19209290e-7
#define DBL_MIN          1.17549435e-38

#define LDBL_MANT_DIG    53
#define LDBL_DIG         15
#define LDBL_MIN_EXP     (-1021)
#define LDBL_MIN_10_EXP  (-307)
#define LDBL_MAX_EXP     1024
#define LDBL_MAX_10_EXP  308
#define LDBL_MAX         1.7976931348623157e+308L
#define LDBL_EPSILON     2.2204460492503131e-16L
#define LDBL_MIN         2.2250738585072014e-308L

#endif
