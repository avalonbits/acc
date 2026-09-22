/* SPDX-License-Identifier: LGPL-2.1-or-later */
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
 * That is also why the functions below answer overflow with HUGE_VAL and
 * nothing else: there is no errno here and no floating-point exception,
 * which math_errhandling says by answering zero. */
double __acc_inf(void);
double __acc_nan(void);

#define HUGE_VAL   __acc_inf()
#define HUGE_VALF  __acc_inf()
#define INFINITY   __acc_inf()
#define NAN        __acc_nan()

#define MATH_ERRNO       1
#define MATH_ERREXCEPT   2
#define math_errhandling 0

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

#endif
