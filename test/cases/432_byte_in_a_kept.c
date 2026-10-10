/* A byte left in A -- the value of an assignment to one, `--w` -- kept
 * while the next operand is read: a global byte, a byte through a
 * pointer, an element, another assignment's byte, each read through A.
 * acc compared `--w < g` as g with itself, and added g to itself for
 * `(--w) + g`. And a call's argument kept while the next is read; and the
 * ways that were right already, a call and a signed local, kept so. */

typedef unsigned char u8;

u8 g = 5, arr[4] = { 9, 5, 2, 7 };

__attribute__((noinline)) int five(void)
{
    return 5;
}

__attribute__((noinline)) int two(int a, int b)
{
    return a * 100 + b;
}

__attribute__((noinline)) int deref_global(u8 w) { return --w < g; }
__attribute__((noinline)) int deref_ptr(u8 w, u8 *p) { return --w < *p; }
__attribute__((noinline)) int index_ptr(u8 w, u8 *p, u8 i) { return --w < arr[*p + i]; }
__attribute__((noinline)) int call(u8 w) { return --w < five(); }
__attribute__((noinline)) int signed_local(u8 w, signed char s) { return --w < s; }
__attribute__((noinline)) int plus_global(u8 w) { return (--w) + g; }
__attribute__((noinline)) int assign_then(u8 w, u8 x) { u8 c; return --w + (c = x + 1); }
__attribute__((noinline)) int branch(u8 w, u8 *p) { if (--w < *p) return 1; return 2; }
__attribute__((noinline)) int args(u8 w, u8 *p) { return two(--w, *p); }
__attribute__((noinline)) int args_after(u8 w, u8 *p) { return two(*p, --w); }
__attribute__((noinline)) int index_of(u8 w) { return arr[--w] + g; }
__attribute__((noinline)) int comma(u8 w, u8 *p) { int r; r = (--w, *p); return r + w; }
__attribute__((noinline)) int ternary(u8 w, u8 *p) { return --w ? *p : 0; }
__attribute__((noinline)) int store(u8 w, u8 *p) { u8 c; c = --w; return c + *p; }
__attribute__((noinline)) int compound(u8 w, u8 *p) { int x = 10; x += --w; return x + *p; }
__attribute__((noinline)) int nested(u8 w, u8 x) { u8 c; return (c = --w) < (x = g); }

int main(void)
{
    int bad = 0;
    u8 v = 5, z = 0;

    if (deref_global(6) != 0 || deref_global(5) != 1) bad |= 1;
    if (deref_ptr(6, &v) != 0 || deref_ptr(5, &v) != 1) bad |= 2;
    if (index_ptr(9, &z, 0) != 1 || index_ptr(10, &z, 0) != 0) bad |= 4;
    if (call(6) != 0 || call(5) != 1) bad |= 8;
    if (signed_local(10, 9) != 0 || signed_local(9, 9) != 1) bad |= 16;
    if (plus_global(10) != 14) bad |= 32;
    if (assign_then(10, 3) != 13) bad |= 64;
    if (branch(5, &v) != 1 || branch(7, &v) != 2) bad |= 128;
    if (args(10, &v) != 905) bad |= 256;
    if (args_after(10, &v) != 509) bad |= 512;
    if (index_of(3) != 7) bad |= 1024;
    if (comma(10, &v) != 14) bad |= 2048;
    if (ternary(10, &v) != 5 || ternary(1, &v) != 0) bad |= 4096;
    if (store(10, &v) != 14) bad |= 8192;
    if (compound(10, &v) != 24) bad |= 16384;
    if (nested(5, 0) != 1 || nested(7, 0) != 0) bad |= 32768;

    /* One byte of answer: which went wrong, as the lowest bit set. */
    if (bad) {
        int k = 1;

        while (!(bad & 1)) {
            bad >>= 1;
            k++;
        }

        return k;
    }

    return 42;
}
