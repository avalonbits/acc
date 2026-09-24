import math, struct, random

def f32(x): return struct.unpack('f', struct.pack('f', x))[0]
def b(x):   return struct.unpack('I', struct.pack('f', f32(x)))[0]

random.seed(23)

def pick(lo, hi, n, log=False):
    out = []
    for _ in range(n):
        if log:
            out.append(f32(math.exp(random.uniform(math.log(lo), math.log(hi)))))
        else:
            out.append(f32(random.uniform(lo, hi)))
    return out

FN1 = [
    ("exp",   math.exp,   pick(-80, 80, 12) + [0.0, 1.0, -1.0, 0.5], 2),
    ("exp2",  lambda x: 2.0 ** x, pick(-120, 120, 10) + [0.0, 1.0, 10.0], 2),
    ("expm1", math.expm1, pick(-0.9, 0.9, 8) + [0.0, 1e-5, -1e-5, 5.0], 4),
    ("log",   math.log,   pick(1e-30, 1e30, 12, log=True) + [1.0, 2.0, 0.5], 3),
    ("log2",  math.log2,  pick(1e-30, 1e30, 8, log=True) + [1.0, 8.0], 3),
    ("log10", math.log10, pick(1e-30, 1e30, 8, log=True) + [1.0, 100.0], 3),
    ("log1p", math.log1p, pick(-0.9, 2.0, 8) + [0.0, 1e-6], 4),
    ("sin",   math.sin,   pick(-100, 100, 12) + [0.0, 1.0, -1.0], 4),
    ("cos",   math.cos,   pick(-100, 100, 12) + [0.0, 1.0, -1.0], 12),
    ("tan",   math.tan,   pick(-1.4, 1.4, 10) + [0.0, 0.5], 6),
    ("atan",  math.atan,  pick(-50, 50, 10) + [0.0, 1.0, -1.0], 3),
    ("asin",  math.asin,  pick(-1, 1, 10) + [0.0, 1.0, -1.0, 0.5], 3),
    ("acos",  math.acos,  pick(-1, 1, 10) + [0.0, 1.0, -1.0, 0.5], 3),
    ("sinh",  math.sinh,  pick(-80, 80, 8) + [0.0, 0.1, -0.1, 1.0], 4),
    ("cosh",  math.cosh,  pick(-80, 80, 8) + [0.0, 1.0], 3),
    ("tanh",  math.tanh,  pick(-10, 10, 8) + [0.0, 0.1, 5.0], 5),
    ("cbrt",  lambda x: math.copysign(abs(x) ** (1.0 / 3.0), x),
              pick(-1e20, 1e20, 10) + [0.0, 1.0, 8.0, -27.0], 2),
    ("sqrt",  math.sqrt,  pick(1e-30, 1e30, 8, log=True) + [0.0, 1.0, 4.0], 0),
]

FN2 = [
    ("pow", lambda a, c: a ** c,
     pick(1e-6, 1e6, 14, log=True) + [2.0, 10.0, 32.0],
     pick(-8, 8, 14) + [10.0, 3.0, 1.0 / 3.0], 16),
    ("atan2", math.atan2,
     pick(-100, 100, 10) + [1.0, 0.0, -1.0],
     pick(-100, 100, 10) + [0.0, 1.0, -1.0], 4),
    ("hypot", math.hypot,
     pick(1e-5, 1e5, 10, log=True) + [3.0],
     pick(1e-5, 1e5, 10, log=True) + [4.0], 2),
]


# Added after the others, and drawn after them, so that their values stay
# as they were.
FN1 += [
    ("asinh", math.asinh, pick(-1e30, 1e30, 6) + pick(-3, 3, 10) + [0.0, 1e-5, 0.5, -2.0], 2),
    ("acosh", math.acosh, pick(1, 1e30, 8, log=True) + pick(1, 3, 8) + [1.0, 1.0001, 2.0], 2),
    ("atanh", math.atanh, pick(-0.999, 0.999, 14) + [0.0, 1e-5, 0.5, -0.9999], 3),
    ("erf",   math.erf,   pick(-4, 4, 20) + [0.0, 1e-5, 0.5, 1.0, -1.5, 3.0], 2),
    ("erfc",  math.erfc,  pick(-3, 9, 24) + pick(2, 10, 24) + [0.0, 0.5, 1.0, 2.0, 4.0, 9.0], 4),
    ("tgamma", math.gamma, pick(0.01, 35, 20) + pick(-10, -0.01, 10)
              + [0.5, 1.0, 2.0, 3.5, 10.0, 34.5], 10),
    ("lgamma", math.lgamma, pick(0.01, 8, 14) + pick(8, 1e30, 8, log=True)
              + pick(-10, -0.01, 6) + [0.5, 1.001, 1.999, 2.5, 3.0, 100.0], 10),
]

out = []
w = out.append
w("/* What every one of these should answer, worked out to more digits than")
w(" * a float holds and rounded to one. Generated; the generator and the")
w(" * measurements are in the commit that added them. */")
w("#include <math.h>")
w("")
w("static unsigned long ub(double x)")
w("{")
w("    union { float f; unsigned long u; } v;")
w("")
w("    v.f = (float) x;")
w("")
w("    return v.u;")
w("}")
w("")
w("static double fb(unsigned long u)")
w("{")
w("    union { float f; unsigned long u; } v;")
w("")
w("    v.u = u;")
w("")
w("    return v.f;")
w("}")
w("")
w("/* How many representable steps apart two values are. Both have the same")
w(" * sign in every case here, so their bit patterns count up the way the")
w(" * numbers do. */")
w("static long apart(unsigned long a, unsigned long c)")
w("{")
w("    if ((a ^ c) & 0x80000000UL)")
w("        return 1000;")
w("")
w("    return (long) (a > c ? a - c : c - a);")
w("}")
w("")
w("typedef double (*fn1)(double);")
w("typedef double (*fn2)(double, double);")
w("")
w("static int check1(fn1 f, const unsigned long *in, const unsigned long *want,")
w("                  int n, int tol)")
w("{")
w("    int i, bad = 0;")
w("")
w("    for (i = 0; i < n; i++)")
w("        if (apart(ub(f(fb(in[i]))), want[i]) > tol)")
w("            bad++;")
w("")
w("    return bad;")
w("}")
w("")
w("static int check2(fn2 f, const unsigned long *in, const unsigned long *want,")
w("                  int n, int tol)")
w("{")
w("    int i, bad = 0;")
w("")
w("    for (i = 0; i < n; i++)")
w("        if (apart(ub(f(fb(in[2 * i]), fb(in[2 * i + 1]))), want[i]) > tol)")
w("            bad++;")
w("")
w("    return bad;")
w("}")
w("")

def ok(v):
    return not (abs(v) > 3.0e38 or (v != 0 and abs(v) < 1.2e-38))

for name, f, xs, tol in FN1:
    ins, wants = [], []
    for x in xs:
        try:
            v = f(f32(x))
        except (ValueError, OverflowError):
            continue
        if not ok(v):
            continue
        ins.append(b(x)); wants.append(b(v))
    w("static const unsigned long %s_in[] = {" % name)
    w("    " + ", ".join("0x%08xUL" % v for v in ins))
    w("};")
    w("static const unsigned long %s_want[] = {" % name)
    w("    " + ", ".join("0x%08xUL" % v for v in wants))
    w("};")

for name, f, xs, ys, tol in FN2:
    ins, wants = [], []
    for x, y in zip(xs, ys):
        try:
            v = f(f32(x), f32(y))
        except (ValueError, OverflowError, ZeroDivisionError):
            continue
        if not ok(v):
            continue
        ins.append((b(x), b(y))); wants.append(b(v))
    w("static const unsigned long %s_in[] = {" % name)
    w("    " + ", ".join("0x%08xUL, 0x%08xUL" % p for p in ins))
    w("};")
    w("static const unsigned long %s_want[] = {" % name)
    w("    " + ", ".join("0x%08xUL" % v for v in wants))
    w("};")

w("")
w("#define N(a) ((int) (sizeof (a) / sizeof *(a)))")
w("")
w("int main(void) {")
w("    int bad = 0, r = 0;")
w("")
for name, f, xs, tol in FN1:
    w("    bad += check1(%s, %s_in, %s_want, N(%s_want), %d);"
      % (name, name, name, name, tol))
for name, f, xs, ys, tol in FN2:
    w("    bad += check2(%s, %s_in, %s_want, N(%s_want), %d);"
      % (name, name, name, name, tol))
w("    if (bad == 0) r++;")
w("")
w("    /* The ones whose answer is exact, which no tolerance should cover. */")
w("    if (ub(pow(2.0f, 10.0f)) == ub(1024.0f) && ub(pow(9.0f, 0.5f)) == ub(3.0f)) r++;")
w("    if (ub(pow(-2.0f, 3.0f)) == ub(-8.0f) && ub(pow(-2.0f, 2.0f)) == ub(4.0f)) r++;")
w("    if (ub(pow(5.0f, 0.0f)) == ub(1.0f) && ub(pow(1.0f, 99.0f)) == ub(1.0f)) r++;")
w("    if (ub(exp(0.0f)) == ub(1.0f) && ub(log(1.0f)) == ub(0.0f)) r++;")
w("    if (ub(sin(0.0f)) == ub(0.0f) && ub(cos(0.0f)) == ub(1.0f)) r++;")
w("    if (ub(cbrt(-27.0f)) == ub(-3.0f) && ub(cbrt(8.0f)) == ub(2.0f)) r++;")
w("    if (ub(hypot(3.0f, 4.0f)) == ub(5.0f) && ub(atan2(0.0f, 1.0f)) == ub(0.0f)) r++;")
w("    if (ub(sqrt(144.0f)) == ub(12.0f) && ub(log2(1024.0f)) == ub(10.0f)) r++;")
w("")
w("    return r + 33 + bad;        /* 9 checks */")
w("}")

print("\n".join(out))
