/* More file-scope symbols than the symbol table starts with, so it has to
 * grow while calls to every one of them are still waiting to be patched.
 *
 * main comes first, so all 70 calls are forward references with a fixup
 * outstanding, and the definitions that resolve them are what pushes the
 * table past its initial 64 and reallocs it. Holding a Sym * across that
 * realloc read freed memory: the host gave the right answer anyway and the
 * Agon reported 'main' as never defined.
 *
 * Each step adds one, from -28, so the answer is 42 however many there are.
 */
int main(void) {
    int s = -28;
    s = f0(s);
    s = f1(s);
    s = f2(s);
    s = f3(s);
    s = f4(s);
    s = f5(s);
    s = f6(s);
    s = f7(s);
    s = f8(s);
    s = f9(s);
    s = f10(s);
    s = f11(s);
    s = f12(s);
    s = f13(s);
    s = f14(s);
    s = f15(s);
    s = f16(s);
    s = f17(s);
    s = f18(s);
    s = f19(s);
    s = f20(s);
    s = f21(s);
    s = f22(s);
    s = f23(s);
    s = f24(s);
    s = f25(s);
    s = f26(s);
    s = f27(s);
    s = f28(s);
    s = f29(s);
    s = f30(s);
    s = f31(s);
    s = f32(s);
    s = f33(s);
    s = f34(s);
    s = f35(s);
    s = f36(s);
    s = f37(s);
    s = f38(s);
    s = f39(s);
    s = f40(s);
    s = f41(s);
    s = f42(s);
    s = f43(s);
    s = f44(s);
    s = f45(s);
    s = f46(s);
    s = f47(s);
    s = f48(s);
    s = f49(s);
    s = f50(s);
    s = f51(s);
    s = f52(s);
    s = f53(s);
    s = f54(s);
    s = f55(s);
    s = f56(s);
    s = f57(s);
    s = f58(s);
    s = f59(s);
    s = f60(s);
    s = f61(s);
    s = f62(s);
    s = f63(s);
    s = f64(s);
    s = f65(s);
    s = f66(s);
    s = f67(s);
    s = f68(s);
    s = f69(s);
    return s;
}

int f0(int a) { return a + 1; }
int f1(int a) { return a + 1; }
int f2(int a) { return a + 1; }
int f3(int a) { return a + 1; }
int f4(int a) { return a + 1; }
int f5(int a) { return a + 1; }
int f6(int a) { return a + 1; }
int f7(int a) { return a + 1; }
int f8(int a) { return a + 1; }
int f9(int a) { return a + 1; }
int f10(int a) { return a + 1; }
int f11(int a) { return a + 1; }
int f12(int a) { return a + 1; }
int f13(int a) { return a + 1; }
int f14(int a) { return a + 1; }
int f15(int a) { return a + 1; }
int f16(int a) { return a + 1; }
int f17(int a) { return a + 1; }
int f18(int a) { return a + 1; }
int f19(int a) { return a + 1; }
int f20(int a) { return a + 1; }
int f21(int a) { return a + 1; }
int f22(int a) { return a + 1; }
int f23(int a) { return a + 1; }
int f24(int a) { return a + 1; }
int f25(int a) { return a + 1; }
int f26(int a) { return a + 1; }
int f27(int a) { return a + 1; }
int f28(int a) { return a + 1; }
int f29(int a) { return a + 1; }
int f30(int a) { return a + 1; }
int f31(int a) { return a + 1; }
int f32(int a) { return a + 1; }
int f33(int a) { return a + 1; }
int f34(int a) { return a + 1; }
int f35(int a) { return a + 1; }
int f36(int a) { return a + 1; }
int f37(int a) { return a + 1; }
int f38(int a) { return a + 1; }
int f39(int a) { return a + 1; }
int f40(int a) { return a + 1; }
int f41(int a) { return a + 1; }
int f42(int a) { return a + 1; }
int f43(int a) { return a + 1; }
int f44(int a) { return a + 1; }
int f45(int a) { return a + 1; }
int f46(int a) { return a + 1; }
int f47(int a) { return a + 1; }
int f48(int a) { return a + 1; }
int f49(int a) { return a + 1; }
int f50(int a) { return a + 1; }
int f51(int a) { return a + 1; }
int f52(int a) { return a + 1; }
int f53(int a) { return a + 1; }
int f54(int a) { return a + 1; }
int f55(int a) { return a + 1; }
int f56(int a) { return a + 1; }
int f57(int a) { return a + 1; }
int f58(int a) { return a + 1; }
int f59(int a) { return a + 1; }
int f60(int a) { return a + 1; }
int f61(int a) { return a + 1; }
int f62(int a) { return a + 1; }
int f63(int a) { return a + 1; }
int f64(int a) { return a + 1; }
int f65(int a) { return a + 1; }
int f66(int a) { return a + 1; }
int f67(int a) { return a + 1; }
int f68(int a) { return a + 1; }
int f69(int a) { return a + 1; }
