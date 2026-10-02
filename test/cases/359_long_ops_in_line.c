/* Longs made in line by opt-acc's leaf backend, where the runtime's routine
 * would be bigger or slower: shifts by constants that move whole bytes,
 * bits, or both -- left and right, signed and not; &, | and ^ with
 * constants whose bytes are 0, 0xff or neither; + and - through HL and A,
 * the carry into the top byte. Each made in place, the answer in its
 * operand's slot; into a slot of its own with the operand kept; and in a
 * loop, where the line may be a little bigger than the call -- and where
 * the carry comes in set, from the loop's test. Generated, with the
 * answers worked out on the host. */

static unsigned long u0_in(unsigned long x) { x = x << 1; return x; }
static unsigned long u0_keep(unsigned long x, unsigned long *was) { unsigned long y = x << 1; *was = x; return y; }
static unsigned long u0_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x << 1; return x; }
static unsigned long u1_in(unsigned long x) { x = x >> 1; return x; }
static unsigned long u1_keep(unsigned long x, unsigned long *was) { unsigned long y = x >> 1; *was = x; return y; }
static unsigned long u1_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x >> 1; return x; }
static unsigned long u2_in(unsigned long x) { x = x << 8; return x; }
static unsigned long u2_keep(unsigned long x, unsigned long *was) { unsigned long y = x << 8; *was = x; return y; }
static unsigned long u2_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x << 8; return x; }
static unsigned long u3_in(unsigned long x) { x = x >> 8; return x; }
static unsigned long u3_keep(unsigned long x, unsigned long *was) { unsigned long y = x >> 8; *was = x; return y; }
static unsigned long u3_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x >> 8; return x; }
static unsigned long u4_in(unsigned long x) { x = x << 9; return x; }
static unsigned long u4_keep(unsigned long x, unsigned long *was) { unsigned long y = x << 9; *was = x; return y; }
static unsigned long u4_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x << 9; return x; }
static unsigned long u5_in(unsigned long x) { x = x >> 9; return x; }
static unsigned long u5_keep(unsigned long x, unsigned long *was) { unsigned long y = x >> 9; *was = x; return y; }
static unsigned long u5_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x >> 9; return x; }
static unsigned long u6_in(unsigned long x) { x = x << 16; return x; }
static unsigned long u6_keep(unsigned long x, unsigned long *was) { unsigned long y = x << 16; *was = x; return y; }
static unsigned long u6_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x << 16; return x; }
static unsigned long u7_in(unsigned long x) { x = x >> 16; return x; }
static unsigned long u7_keep(unsigned long x, unsigned long *was) { unsigned long y = x >> 16; *was = x; return y; }
static unsigned long u7_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x >> 16; return x; }
static unsigned long u8_in(unsigned long x) { x = x << 17; return x; }
static unsigned long u8_keep(unsigned long x, unsigned long *was) { unsigned long y = x << 17; *was = x; return y; }
static unsigned long u8_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x << 17; return x; }
static unsigned long u9_in(unsigned long x) { x = x >> 17; return x; }
static unsigned long u9_keep(unsigned long x, unsigned long *was) { unsigned long y = x >> 17; *was = x; return y; }
static unsigned long u9_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x >> 17; return x; }
static unsigned long u10_in(unsigned long x) { x = x << 24; return x; }
static unsigned long u10_keep(unsigned long x, unsigned long *was) { unsigned long y = x << 24; *was = x; return y; }
static unsigned long u10_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x << 24; return x; }
static unsigned long u11_in(unsigned long x) { x = x >> 24; return x; }
static unsigned long u11_keep(unsigned long x, unsigned long *was) { unsigned long y = x >> 24; *was = x; return y; }
static unsigned long u11_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x >> 24; return x; }
static unsigned long u12_in(unsigned long x) { x = x << 31; return x; }
static unsigned long u12_keep(unsigned long x, unsigned long *was) { unsigned long y = x << 31; *was = x; return y; }
static unsigned long u12_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x << 31; return x; }
static unsigned long u13_in(unsigned long x) { x = x >> 31; return x; }
static unsigned long u13_keep(unsigned long x, unsigned long *was) { unsigned long y = x >> 31; *was = x; return y; }
static unsigned long u13_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x >> 31; return x; }
static unsigned long u14_in(unsigned long x) { x = x & 0xffUL; return x; }
static unsigned long u14_keep(unsigned long x, unsigned long *was) { unsigned long y = x & 0xffUL; *was = x; return y; }
static unsigned long u14_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x & 0xffUL; return x; }
static unsigned long u15_in(unsigned long x) { x = x | 0xffUL; return x; }
static unsigned long u15_keep(unsigned long x, unsigned long *was) { unsigned long y = x | 0xffUL; *was = x; return y; }
static unsigned long u15_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x | 0xffUL; return x; }
static unsigned long u16_in(unsigned long x) { x = x ^ 0xffUL; return x; }
static unsigned long u16_keep(unsigned long x, unsigned long *was) { unsigned long y = x ^ 0xffUL; *was = x; return y; }
static unsigned long u16_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x ^ 0xffUL; return x; }
static unsigned long u17_in(unsigned long x) { x = x & 0xffffff00UL; return x; }
static unsigned long u17_keep(unsigned long x, unsigned long *was) { unsigned long y = x & 0xffffff00UL; *was = x; return y; }
static unsigned long u17_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x & 0xffffff00UL; return x; }
static unsigned long u18_in(unsigned long x) { x = x | 0xffffff00UL; return x; }
static unsigned long u18_keep(unsigned long x, unsigned long *was) { unsigned long y = x | 0xffffff00UL; *was = x; return y; }
static unsigned long u18_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x | 0xffffff00UL; return x; }
static unsigned long u19_in(unsigned long x) { x = x ^ 0xffffff00UL; return x; }
static unsigned long u19_keep(unsigned long x, unsigned long *was) { unsigned long y = x ^ 0xffffff00UL; *was = x; return y; }
static unsigned long u19_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x ^ 0xffffff00UL; return x; }
static unsigned long u20_in(unsigned long x) { x = x & 0xf0f0f0fUL; return x; }
static unsigned long u20_keep(unsigned long x, unsigned long *was) { unsigned long y = x & 0xf0f0f0fUL; *was = x; return y; }
static unsigned long u20_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x & 0xf0f0f0fUL; return x; }
static unsigned long u21_in(unsigned long x) { x = x | 0xf0f0f0fUL; return x; }
static unsigned long u21_keep(unsigned long x, unsigned long *was) { unsigned long y = x | 0xf0f0f0fUL; *was = x; return y; }
static unsigned long u21_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x | 0xf0f0f0fUL; return x; }
static unsigned long u22_in(unsigned long x) { x = x ^ 0xf0f0f0fUL; return x; }
static unsigned long u22_keep(unsigned long x, unsigned long *was) { unsigned long y = x ^ 0xf0f0f0fUL; *was = x; return y; }
static unsigned long u22_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x ^ 0xf0f0f0fUL; return x; }
static unsigned long u23_in(unsigned long x) { x = x & 0x80000001UL; return x; }
static unsigned long u23_keep(unsigned long x, unsigned long *was) { unsigned long y = x & 0x80000001UL; *was = x; return y; }
static unsigned long u23_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x & 0x80000001UL; return x; }
static unsigned long u24_in(unsigned long x) { x = x | 0x80000001UL; return x; }
static unsigned long u24_keep(unsigned long x, unsigned long *was) { unsigned long y = x | 0x80000001UL; *was = x; return y; }
static unsigned long u24_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x | 0x80000001UL; return x; }
static unsigned long u25_in(unsigned long x) { x = x ^ 0x80000001UL; return x; }
static unsigned long u25_keep(unsigned long x, unsigned long *was) { unsigned long y = x ^ 0x80000001UL; *was = x; return y; }
static unsigned long u25_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x ^ 0x80000001UL; return x; }
static unsigned long u26_in(unsigned long x) { x = x + 0x1UL; return x; }
static unsigned long u26_keep(unsigned long x, unsigned long *was) { unsigned long y = x + 0x1UL; *was = x; return y; }
static unsigned long u26_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x + 0x1UL; return x; }
static unsigned long u27_in(unsigned long x) { x = x - 0x1UL; return x; }
static unsigned long u27_keep(unsigned long x, unsigned long *was) { unsigned long y = x - 0x1UL; *was = x; return y; }
static unsigned long u27_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x - 0x1UL; return x; }
static unsigned long u28_in(unsigned long x) { x = 0x1UL - x; return x; }
static unsigned long u28_keep(unsigned long x, unsigned long *was) { unsigned long y = 0x1UL - x; *was = x; return y; }
static unsigned long u28_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = 0x1UL - x; return x; }
static unsigned long u29_in(unsigned long x) { x = x + 0xffffffUL; return x; }
static unsigned long u29_keep(unsigned long x, unsigned long *was) { unsigned long y = x + 0xffffffUL; *was = x; return y; }
static unsigned long u29_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x + 0xffffffUL; return x; }
static unsigned long u30_in(unsigned long x) { x = x - 0xffffffUL; return x; }
static unsigned long u30_keep(unsigned long x, unsigned long *was) { unsigned long y = x - 0xffffffUL; *was = x; return y; }
static unsigned long u30_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x - 0xffffffUL; return x; }
static unsigned long u31_in(unsigned long x) { x = 0xffffffUL - x; return x; }
static unsigned long u31_keep(unsigned long x, unsigned long *was) { unsigned long y = 0xffffffUL - x; *was = x; return y; }
static unsigned long u31_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = 0xffffffUL - x; return x; }
static unsigned long u32_in(unsigned long x) { x = x + 0xfffffffeUL; return x; }
static unsigned long u32_keep(unsigned long x, unsigned long *was) { unsigned long y = x + 0xfffffffeUL; *was = x; return y; }
static unsigned long u32_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x + 0xfffffffeUL; return x; }
static unsigned long u33_in(unsigned long x) { x = x - 0xfffffffeUL; return x; }
static unsigned long u33_keep(unsigned long x, unsigned long *was) { unsigned long y = x - 0xfffffffeUL; *was = x; return y; }
static unsigned long u33_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = x - 0xfffffffeUL; return x; }
static unsigned long u34_in(unsigned long x) { x = 0xfffffffeUL - x; return x; }
static unsigned long u34_keep(unsigned long x, unsigned long *was) { unsigned long y = 0xfffffffeUL - x; *was = x; return y; }
static unsigned long u34_loop(unsigned long x, int n) { for (int i = 0; i < n; i++) x = 0xfffffffeUL - x; return x; }
static long s0_in(long x) { x = x << 1; return x; }
static long s0_keep(long x, long *was) { long y = x << 1; *was = x; return y; }
static long s0_loop(long x, int n) { for (int i = 0; i < n; i++) x = x << 1; return x; }
static long s1_in(long x) { x = x >> 1; return x; }
static long s1_keep(long x, long *was) { long y = x >> 1; *was = x; return y; }
static long s1_loop(long x, int n) { for (int i = 0; i < n; i++) x = x >> 1; return x; }
static long s2_in(long x) { x = x << 8; return x; }
static long s2_keep(long x, long *was) { long y = x << 8; *was = x; return y; }
static long s2_loop(long x, int n) { for (int i = 0; i < n; i++) x = x << 8; return x; }
static long s3_in(long x) { x = x >> 8; return x; }
static long s3_keep(long x, long *was) { long y = x >> 8; *was = x; return y; }
static long s3_loop(long x, int n) { for (int i = 0; i < n; i++) x = x >> 8; return x; }
static long s4_in(long x) { x = x << 9; return x; }
static long s4_keep(long x, long *was) { long y = x << 9; *was = x; return y; }
static long s4_loop(long x, int n) { for (int i = 0; i < n; i++) x = x << 9; return x; }
static long s5_in(long x) { x = x >> 9; return x; }
static long s5_keep(long x, long *was) { long y = x >> 9; *was = x; return y; }
static long s5_loop(long x, int n) { for (int i = 0; i < n; i++) x = x >> 9; return x; }
static long s6_in(long x) { x = x << 16; return x; }
static long s6_keep(long x, long *was) { long y = x << 16; *was = x; return y; }
static long s6_loop(long x, int n) { for (int i = 0; i < n; i++) x = x << 16; return x; }
static long s7_in(long x) { x = x >> 16; return x; }
static long s7_keep(long x, long *was) { long y = x >> 16; *was = x; return y; }
static long s7_loop(long x, int n) { for (int i = 0; i < n; i++) x = x >> 16; return x; }
static long s8_in(long x) { x = x << 17; return x; }
static long s8_keep(long x, long *was) { long y = x << 17; *was = x; return y; }
static long s8_loop(long x, int n) { for (int i = 0; i < n; i++) x = x << 17; return x; }
static long s9_in(long x) { x = x >> 17; return x; }
static long s9_keep(long x, long *was) { long y = x >> 17; *was = x; return y; }
static long s9_loop(long x, int n) { for (int i = 0; i < n; i++) x = x >> 17; return x; }
static long s10_in(long x) { x = x << 24; return x; }
static long s10_keep(long x, long *was) { long y = x << 24; *was = x; return y; }
static long s10_loop(long x, int n) { for (int i = 0; i < n; i++) x = x << 24; return x; }
static long s11_in(long x) { x = x >> 24; return x; }
static long s11_keep(long x, long *was) { long y = x >> 24; *was = x; return y; }
static long s11_loop(long x, int n) { for (int i = 0; i < n; i++) x = x >> 24; return x; }
static long s12_in(long x) { x = x << 31; return x; }
static long s12_keep(long x, long *was) { long y = x << 31; *was = x; return y; }
static long s12_loop(long x, int n) { for (int i = 0; i < n; i++) x = x << 31; return x; }
static long s13_in(long x) { x = x >> 31; return x; }
static long s13_keep(long x, long *was) { long y = x >> 31; *was = x; return y; }
static long s13_loop(long x, int n) { for (int i = 0; i < n; i++) x = x >> 31; return x; }
static long s14_in(long x) { x = x & (long) 0xffUL; return x; }
static long s14_keep(long x, long *was) { long y = x & (long) 0xffUL; *was = x; return y; }
static long s14_loop(long x, int n) { for (int i = 0; i < n; i++) x = x & (long) 0xffUL; return x; }
static long s15_in(long x) { x = x | (long) 0xffUL; return x; }
static long s15_keep(long x, long *was) { long y = x | (long) 0xffUL; *was = x; return y; }
static long s15_loop(long x, int n) { for (int i = 0; i < n; i++) x = x | (long) 0xffUL; return x; }
static long s16_in(long x) { x = x ^ (long) 0xffUL; return x; }
static long s16_keep(long x, long *was) { long y = x ^ (long) 0xffUL; *was = x; return y; }
static long s16_loop(long x, int n) { for (int i = 0; i < n; i++) x = x ^ (long) 0xffUL; return x; }
static long s17_in(long x) { x = x & (long) 0xffffff00UL; return x; }
static long s17_keep(long x, long *was) { long y = x & (long) 0xffffff00UL; *was = x; return y; }
static long s17_loop(long x, int n) { for (int i = 0; i < n; i++) x = x & (long) 0xffffff00UL; return x; }
static long s18_in(long x) { x = x | (long) 0xffffff00UL; return x; }
static long s18_keep(long x, long *was) { long y = x | (long) 0xffffff00UL; *was = x; return y; }
static long s18_loop(long x, int n) { for (int i = 0; i < n; i++) x = x | (long) 0xffffff00UL; return x; }
static long s19_in(long x) { x = x ^ (long) 0xffffff00UL; return x; }
static long s19_keep(long x, long *was) { long y = x ^ (long) 0xffffff00UL; *was = x; return y; }
static long s19_loop(long x, int n) { for (int i = 0; i < n; i++) x = x ^ (long) 0xffffff00UL; return x; }
static long s20_in(long x) { x = x & (long) 0xf0f0f0fUL; return x; }
static long s20_keep(long x, long *was) { long y = x & (long) 0xf0f0f0fUL; *was = x; return y; }
static long s20_loop(long x, int n) { for (int i = 0; i < n; i++) x = x & (long) 0xf0f0f0fUL; return x; }
static long s21_in(long x) { x = x | (long) 0xf0f0f0fUL; return x; }
static long s21_keep(long x, long *was) { long y = x | (long) 0xf0f0f0fUL; *was = x; return y; }
static long s21_loop(long x, int n) { for (int i = 0; i < n; i++) x = x | (long) 0xf0f0f0fUL; return x; }
static long s22_in(long x) { x = x ^ (long) 0xf0f0f0fUL; return x; }
static long s22_keep(long x, long *was) { long y = x ^ (long) 0xf0f0f0fUL; *was = x; return y; }
static long s22_loop(long x, int n) { for (int i = 0; i < n; i++) x = x ^ (long) 0xf0f0f0fUL; return x; }
static long s23_in(long x) { x = x & (long) 0x80000001UL; return x; }
static long s23_keep(long x, long *was) { long y = x & (long) 0x80000001UL; *was = x; return y; }
static long s23_loop(long x, int n) { for (int i = 0; i < n; i++) x = x & (long) 0x80000001UL; return x; }
static long s24_in(long x) { x = x | (long) 0x80000001UL; return x; }
static long s24_keep(long x, long *was) { long y = x | (long) 0x80000001UL; *was = x; return y; }
static long s24_loop(long x, int n) { for (int i = 0; i < n; i++) x = x | (long) 0x80000001UL; return x; }
static long s25_in(long x) { x = x ^ (long) 0x80000001UL; return x; }
static long s25_keep(long x, long *was) { long y = x ^ (long) 0x80000001UL; *was = x; return y; }
static long s25_loop(long x, int n) { for (int i = 0; i < n; i++) x = x ^ (long) 0x80000001UL; return x; }
static long s26_in(long x) { x = x + (long) 0x1UL; return x; }
static long s26_keep(long x, long *was) { long y = x + (long) 0x1UL; *was = x; return y; }
static long s26_loop(long x, int n) { for (int i = 0; i < n; i++) x = x + (long) 0x1UL; return x; }
static long s27_in(long x) { x = x - (long) 0x1UL; return x; }
static long s27_keep(long x, long *was) { long y = x - (long) 0x1UL; *was = x; return y; }
static long s27_loop(long x, int n) { for (int i = 0; i < n; i++) x = x - (long) 0x1UL; return x; }
static long s28_in(long x) { x = (long) 0x1UL - x; return x; }
static long s28_keep(long x, long *was) { long y = (long) 0x1UL - x; *was = x; return y; }
static long s28_loop(long x, int n) { for (int i = 0; i < n; i++) x = (long) 0x1UL - x; return x; }
static long s29_in(long x) { x = x + (long) 0xffffffUL; return x; }
static long s29_keep(long x, long *was) { long y = x + (long) 0xffffffUL; *was = x; return y; }
static long s29_loop(long x, int n) { for (int i = 0; i < n; i++) x = x + (long) 0xffffffUL; return x; }
static long s30_in(long x) { x = x - (long) 0xffffffUL; return x; }
static long s30_keep(long x, long *was) { long y = x - (long) 0xffffffUL; *was = x; return y; }
static long s30_loop(long x, int n) { for (int i = 0; i < n; i++) x = x - (long) 0xffffffUL; return x; }
static long s31_in(long x) { x = (long) 0xffffffUL - x; return x; }
static long s31_keep(long x, long *was) { long y = (long) 0xffffffUL - x; *was = x; return y; }
static long s31_loop(long x, int n) { for (int i = 0; i < n; i++) x = (long) 0xffffffUL - x; return x; }
static long s32_in(long x) { x = x + (long) 0xfffffffeUL; return x; }
static long s32_keep(long x, long *was) { long y = x + (long) 0xfffffffeUL; *was = x; return y; }
static long s32_loop(long x, int n) { for (int i = 0; i < n; i++) x = x + (long) 0xfffffffeUL; return x; }
static long s33_in(long x) { x = x - (long) 0xfffffffeUL; return x; }
static long s33_keep(long x, long *was) { long y = x - (long) 0xfffffffeUL; *was = x; return y; }
static long s33_loop(long x, int n) { for (int i = 0; i < n; i++) x = x - (long) 0xfffffffeUL; return x; }
static long s34_in(long x) { x = (long) 0xfffffffeUL - x; return x; }
static long s34_keep(long x, long *was) { long y = (long) 0xfffffffeUL - x; *was = x; return y; }
static long s34_loop(long x, int n) { for (int i = 0; i < n; i++) x = (long) 0xfffffffeUL - x; return x; }

struct ucase {
    unsigned long (*in)(unsigned long);
    unsigned long (*keep)(unsigned long, unsigned long *);
    unsigned long (*loop)(unsigned long, int);
    unsigned long want[3];
};

struct scase {
    long (*in)(long);
    long (*keep)(long, long *);
    long (*loop)(long, int);
    unsigned long want[3];
};

static const unsigned long input[3] = { 0x89abcdefUL, 0xffffffUL, 0x12345678UL };

static const struct ucase ucases[] = {
    { u0_in, u0_keep, u0_loop, { 0x13579bdeUL, 0x1fffffeUL, 0x2468acf0UL } },
    { u1_in, u1_keep, u1_loop, { 0x44d5e6f7UL, 0x7fffffUL, 0x91a2b3cUL } },
    { u2_in, u2_keep, u2_loop, { 0xabcdef00UL, 0xffffff00UL, 0x34567800UL } },
    { u3_in, u3_keep, u3_loop, { 0x89abcdUL, 0xffffUL, 0x123456UL } },
    { u4_in, u4_keep, u4_loop, { 0x579bde00UL, 0xfffffe00UL, 0x68acf000UL } },
    { u5_in, u5_keep, u5_loop, { 0x44d5e6UL, 0x7fffUL, 0x91a2bUL } },
    { u6_in, u6_keep, u6_loop, { 0xcdef0000UL, 0xffff0000UL, 0x56780000UL } },
    { u7_in, u7_keep, u7_loop, { 0x89abUL, 0xffUL, 0x1234UL } },
    { u8_in, u8_keep, u8_loop, { 0x9bde0000UL, 0xfffe0000UL, 0xacf00000UL } },
    { u9_in, u9_keep, u9_loop, { 0x44d5UL, 0x7fUL, 0x91aUL } },
    { u10_in, u10_keep, u10_loop, { 0xef000000UL, 0xff000000UL, 0x78000000UL } },
    { u11_in, u11_keep, u11_loop, { 0x89UL, 0x0UL, 0x12UL } },
    { u12_in, u12_keep, u12_loop, { 0x80000000UL, 0x80000000UL, 0x0UL } },
    { u13_in, u13_keep, u13_loop, { 0x1UL, 0x0UL, 0x0UL } },
    { u14_in, u14_keep, u14_loop, { 0xefUL, 0xffUL, 0x78UL } },
    { u15_in, u15_keep, u15_loop, { 0x89abcdffUL, 0xffffffUL, 0x123456ffUL } },
    { u16_in, u16_keep, u16_loop, { 0x89abcd10UL, 0xffff00UL, 0x12345687UL } },
    { u17_in, u17_keep, u17_loop, { 0x89abcd00UL, 0xffff00UL, 0x12345600UL } },
    { u18_in, u18_keep, u18_loop, { 0xffffffefUL, 0xffffffffUL, 0xffffff78UL } },
    { u19_in, u19_keep, u19_loop, { 0x765432efUL, 0xff0000ffUL, 0xedcba978UL } },
    { u20_in, u20_keep, u20_loop, { 0x90b0d0fUL, 0xf0f0fUL, 0x2040608UL } },
    { u21_in, u21_keep, u21_loop, { 0x8fafcfefUL, 0xfffffffUL, 0x1f3f5f7fUL } },
    { u22_in, u22_keep, u22_loop, { 0x86a4c2e0UL, 0xff0f0f0UL, 0x1d3b5977UL } },
    { u23_in, u23_keep, u23_loop, { 0x80000001UL, 0x1UL, 0x0UL } },
    { u24_in, u24_keep, u24_loop, { 0x89abcdefUL, 0x80ffffffUL, 0x92345679UL } },
    { u25_in, u25_keep, u25_loop, { 0x9abcdeeUL, 0x80fffffeUL, 0x92345679UL } },
    { u26_in, u26_keep, u26_loop, { 0x89abcdf0UL, 0x1000000UL, 0x12345679UL } },
    { u27_in, u27_keep, u27_loop, { 0x89abcdeeUL, 0xfffffeUL, 0x12345677UL } },
    { u28_in, u28_keep, u28_loop, { 0x76543212UL, 0xff000002UL, 0xedcba989UL } },
    { u29_in, u29_keep, u29_loop, { 0x8aabcdeeUL, 0x1fffffeUL, 0x13345677UL } },
    { u30_in, u30_keep, u30_loop, { 0x88abcdf0UL, 0x0UL, 0x11345679UL } },
    { u31_in, u31_keep, u31_loop, { 0x77543210UL, 0x0UL, 0xeecba987UL } },
    { u32_in, u32_keep, u32_loop, { 0x89abcdedUL, 0xfffffdUL, 0x12345676UL } },
    { u33_in, u33_keep, u33_loop, { 0x89abcdf1UL, 0x1000001UL, 0x1234567aUL } },
    { u34_in, u34_keep, u34_loop, { 0x7654320fUL, 0xfeffffffUL, 0xedcba986UL } },
};

static const struct scase scases[] = {
    { s0_in, s0_keep, s0_loop, { 0x13579bdeUL, 0x1fffffeUL, 0x2468acf0UL } },
    { s1_in, s1_keep, s1_loop, { 0xc4d5e6f7UL, 0x7fffffUL, 0x91a2b3cUL } },
    { s2_in, s2_keep, s2_loop, { 0xabcdef00UL, 0xffffff00UL, 0x34567800UL } },
    { s3_in, s3_keep, s3_loop, { 0xff89abcdUL, 0xffffUL, 0x123456UL } },
    { s4_in, s4_keep, s4_loop, { 0x579bde00UL, 0xfffffe00UL, 0x68acf000UL } },
    { s5_in, s5_keep, s5_loop, { 0xffc4d5e6UL, 0x7fffUL, 0x91a2bUL } },
    { s6_in, s6_keep, s6_loop, { 0xcdef0000UL, 0xffff0000UL, 0x56780000UL } },
    { s7_in, s7_keep, s7_loop, { 0xffff89abUL, 0xffUL, 0x1234UL } },
    { s8_in, s8_keep, s8_loop, { 0x9bde0000UL, 0xfffe0000UL, 0xacf00000UL } },
    { s9_in, s9_keep, s9_loop, { 0xffffc4d5UL, 0x7fUL, 0x91aUL } },
    { s10_in, s10_keep, s10_loop, { 0xef000000UL, 0xff000000UL, 0x78000000UL } },
    { s11_in, s11_keep, s11_loop, { 0xffffff89UL, 0x0UL, 0x12UL } },
    { s12_in, s12_keep, s12_loop, { 0x80000000UL, 0x80000000UL, 0x0UL } },
    { s13_in, s13_keep, s13_loop, { 0xffffffffUL, 0x0UL, 0x0UL } },
    { s14_in, s14_keep, s14_loop, { 0xefUL, 0xffUL, 0x78UL } },
    { s15_in, s15_keep, s15_loop, { 0x89abcdffUL, 0xffffffUL, 0x123456ffUL } },
    { s16_in, s16_keep, s16_loop, { 0x89abcd10UL, 0xffff00UL, 0x12345687UL } },
    { s17_in, s17_keep, s17_loop, { 0x89abcd00UL, 0xffff00UL, 0x12345600UL } },
    { s18_in, s18_keep, s18_loop, { 0xffffffefUL, 0xffffffffUL, 0xffffff78UL } },
    { s19_in, s19_keep, s19_loop, { 0x765432efUL, 0xff0000ffUL, 0xedcba978UL } },
    { s20_in, s20_keep, s20_loop, { 0x90b0d0fUL, 0xf0f0fUL, 0x2040608UL } },
    { s21_in, s21_keep, s21_loop, { 0x8fafcfefUL, 0xfffffffUL, 0x1f3f5f7fUL } },
    { s22_in, s22_keep, s22_loop, { 0x86a4c2e0UL, 0xff0f0f0UL, 0x1d3b5977UL } },
    { s23_in, s23_keep, s23_loop, { 0x80000001UL, 0x1UL, 0x0UL } },
    { s24_in, s24_keep, s24_loop, { 0x89abcdefUL, 0x80ffffffUL, 0x92345679UL } },
    { s25_in, s25_keep, s25_loop, { 0x9abcdeeUL, 0x80fffffeUL, 0x92345679UL } },
    { s26_in, s26_keep, s26_loop, { 0x89abcdf0UL, 0x1000000UL, 0x12345679UL } },
    { s27_in, s27_keep, s27_loop, { 0x89abcdeeUL, 0xfffffeUL, 0x12345677UL } },
    { s28_in, s28_keep, s28_loop, { 0x76543212UL, 0xff000002UL, 0xedcba989UL } },
    { s29_in, s29_keep, s29_loop, { 0x8aabcdeeUL, 0x1fffffeUL, 0x13345677UL } },
    { s30_in, s30_keep, s30_loop, { 0x88abcdf0UL, 0x0UL, 0x11345679UL } },
    { s31_in, s31_keep, s31_loop, { 0x77543210UL, 0x0UL, 0xeecba987UL } },
    { s32_in, s32_keep, s32_loop, { 0x89abcdedUL, 0xfffffdUL, 0x12345676UL } },
    { s33_in, s33_keep, s33_loop, { 0x89abcdf1UL, 0x1000001UL, 0x1234567aUL } },
    { s34_in, s34_keep, s34_loop, { 0x7654320fUL, 0xfeffffffUL, 0xedcba986UL } },
};

int main(void)
{
    unsigned long wasu;
    long wass;
    int one = 1;

    for (int c = 0; c < (int) (sizeof ucases / sizeof ucases[0]); c++)
        for (int x = 0; x < 3; x++) {
            if (ucases[c].in(input[x]) != ucases[c].want[x])
                return 1 + c % 100;
            if (ucases[c].keep(input[x], &wasu) != ucases[c].want[x] || wasu != input[x])
                return 1 + c % 100;
            if (ucases[c].loop(input[x], one) != ucases[c].want[x])
                return 1 + c % 100;
        }
    for (int c = 0; c < (int) (sizeof scases / sizeof scases[0]); c++)
        for (int x = 0; x < 3; x++) {
            if ((unsigned long) scases[c].in((long) input[x]) != scases[c].want[x])
                return 101 + c % 100;
            if ((unsigned long) scases[c].keep((long) input[x], &wass) != scases[c].want[x]
                || (unsigned long) wass != input[x])
                return 101 + c % 100;
            if ((unsigned long) scases[c].loop((long) input[x], one) != scases[c].want[x])
                return 101 + c % 100;
        }

    return 42;
}
