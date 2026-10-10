/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#ifndef ACC_MATH_H
#define ACC_MATH_H

/* C99 7.12, as far as a machine with one floating type can go.
 *
 * float, double and long double are all four-byte IEEE 754 single precision
 * here, as they are in agondev: the chip has no floating-point unit and the
 * library that does the arithmetic does it in single. So `sin` and `sinf`
 * are the same function under two names, which is what C allows when the
 * types coincide, and the long double family is not here at all -- acc
 * refuses long double, and a name for it would be a promise it cannot keep.
 *
 * Twenty-four bits of mantissa is about seven decimal digits. Everything
 * below is written to be within a couple of units in the last place of the
 * right answer, which is as much as single precision has to give; a program
 * that needs more than that needs a wider float than this machine has.
 */

/* FLT_EVAL_METHOD is 0: each type is evaluated as itself. */
typedef float  float_t;
typedef double double_t;

/* The classification macros, which C99 asks for by these names. Written
 * against functions rather than as bit tests in the macro, so that each is
 * evaluated once and the argument may be any expression. */
#define FP_NAN       0
#define FP_INFINITE  1
#define FP_ZERO      2
#define FP_SUBNORMAL 3
#define FP_NORMAL    4

int __acc_fpclassify(double x);
int __acc_signbit(double x);

#define fpclassify(x) __acc_fpclassify(x)
#define signbit(x)    __acc_signbit(x)
#define isnan(x)      (fpclassify(x) == FP_NAN)
#define isinf(x)      (fpclassify(x) == FP_INFINITE)
#define isfinite(x)   (fpclassify(x) > FP_INFINITE)
#define isnormal(x)   (fpclassify(x) == FP_NORMAL)

/* The comparisons that are false, and quietly so, when either side is a
 * NaN. The arithmetic's own comparisons already are; these are functions
 * so that each argument is evaluated once. */
int __acc_isgreater(double x, double y);
int __acc_isgreaterequal(double x, double y);
int __acc_isless(double x, double y);
int __acc_islessequal(double x, double y);
int __acc_islessgreater(double x, double y);
int __acc_isunordered(double x, double y);

#define isgreater(x, y)      __acc_isgreater(x, y)
#define isgreaterequal(x, y) __acc_isgreaterequal(x, y)
#define isless(x, y)         __acc_isless(x, y)
#define islessequal(x, y)    __acc_islessequal(x, y)
#define islessgreater(x, y)  __acc_islessgreater(x, y)
#define isunordered(x, y)    __acc_isunordered(x, y)

/* Infinity and not-a-number, as the bit patterns IEEE 754 gives them.
 *
 * Functions, where C99 asks for constant expressions, because on this
 * machine there is no way to write one: a literal too large to represent
 * does not become an infinity -- 1e40f comes out as 0x00800000, in agondev
 * as well -- and neither does a division by zero, which the runtime answers
 * with the same pattern rather than with an infinity. So there is nothing
 * for the compiler to fold, and `static double x = HUGE_VAL;` is not
 * something this header can offer.
 *
 * What is offered is honest about what the machine does. The two values
 * exist and can be made, passed and tested: the classifying macros above
 * read the bits and answer correctly for both. What they cannot do is take
 * part in arithmetic -- the library that adds and multiplies has no cases
 * for them, so an infinity plus one is not an infinity and a NaN does not
 * compare unequal to itself. A program on this machine that wants to know
 * whether a result went out of range asks isinf or isnan, and does not
 * expect either to propagate the way it would elsewhere.
 *
 * Errors are reported the way C99 7.12.1 asks when math_errhandling says
 * MATH_ERRNO: an argument outside a function's domain sets errno to EDOM
 * and answers a NaN, and a pole or a result too large sets it to ERANGE
 * and answers HUGE_VAL with the right sign. A result too small is not an
 * error here; it is the nearest number there is, a denormal or zero. */
double __acc_inf(void);
double __acc_nan(void);

/* Constants, as C99 7.12 asks, which a static may be initialised to, as
 * Berry's math module's table is: a float literal past the largest float
 * is an infinity, as Annex F has it, and zero over zero, folded, the
 * quiet NaN. */
#define HUGE_VAL   1e39
#define HUGE_VALF  1e39f
#define INFINITY   1e39f
#define NAN        (0.0f / 0.0f)

#define MATH_ERRNO       1
#define MATH_ERREXCEPT   2
#define math_errhandling MATH_ERRNO

/* Where the library says so: errno set to EDOM and a NaN answered, or to
 * ERANGE and the value given. */
double __acc_domain_error(void);
double __acc_range_error(double v);

/* Whole numbers and pieces of numbers: none of these is an approximation,
 * so each is exact for every value it is given. */
double fabs(double x);
double copysign(double x, double y);
double floor(double x);
double ceil(double x);
double trunc(double x);
double round(double x);
double nearbyint(double x);
double rint(double x);
double fmod(double x, double y);
double modf(double x, double *ip);
double frexp(double x, int *exp);
double ldexp(double x, int exp);
double scalbn(double x, int exp);
double fdim(double x, double y);
double fmax(double x, double y);
double fmin(double x, double y);
double nan(const char *tag);

long   lround(double x);
long   lrint(double x);
long long llround(double x);
long long llrint(double x);

double remainder(double x, double y);
double remquo(double x, double y, int *quo);
double nextafter(double x, double y);
double fma(double x, double y, double z);
double scalbln(double x, long exp);
double logb(double x);
int    ilogb(double x);

/* What ilogb answers for a zero and for a NaN, which have no exponent. */
#define FP_ILOGB0   (-8388607 - 1)
#define FP_ILOGBNAN 8388607

/* And the approximations. */
double sqrt(double x);
double cbrt(double x);
double hypot(double x, double y);

double exp(double x);
double exp2(double x);
double expm1(double x);
double log(double x);
double log2(double x);
double log10(double x);
double log1p(double x);
double pow(double x, double y);

double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);

double sinh(double x);
double cosh(double x);
double tanh(double x);
double asinh(double x);
double acosh(double x);
double atanh(double x);

double erf(double x);
double erfc(double x);
double tgamma(double x);
double lgamma(double x);

/* The float family, which on this machine is the same set of functions
 * under the names a program that says `float` reaches for. */
float fabsf(float x);
float copysignf(float x, float y);
float floorf(float x);
float ceilf(float x);
float truncf(float x);
float roundf(float x);
float nearbyintf(float x);
float rintf(float x);
float fmodf(float x, float y);
float modff(float x, float *ip);
float frexpf(float x, int *exp);
float ldexpf(float x, int exp);
float scalbnf(float x, int exp);
float fdimf(float x, float y);
float fmaxf(float x, float y);
float fminf(float x, float y);

float sqrtf(float x);
float cbrtf(float x);
float hypotf(float x, float y);
float expf(float x);
float exp2f(float x);
float expm1f(float x);
float logf(float x);
float log2f(float x);
float log10f(float x);
float log1pf(float x);
float powf(float x, float y);
float sinf(float x);
float cosf(float x);
float tanf(float x);
float asinf(float x);
float acosf(float x);
float atanf(float x);
float atan2f(float y, float x);
float sinhf(float x);
float coshf(float x);
float tanhf(float x);
float asinhf(float x);
float acoshf(float x);
float atanhf(float x);
float erff(float x);
float erfcf(float x);
float tgammaf(float x);
float lgammaf(float x);

float nanf(const char *tag);
float remainderf(float x, float y);
float remquof(float x, float y, int *quo);
float nextafterf(float x, float y);
float fmaf(float x, float y, float z);
float scalblnf(float x, long exp);
float logbf(float x);
int   ilogbf(float x);
long  lroundf(float x);
long  lrintf(float x);
long long llroundf(float x);
long long llrintf(float x);

#endif
