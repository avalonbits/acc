/* A call with 127 arguments, as C99 5.2.4.1 asks a compiler to take, and
 * a function with 127 parameters to take them. Every argument is pending
 * until the call is made, and acc kept 64 values pending at most, so a call
 * with more was refused as nested too deeply -- YARPGen's drivers pass
 * their test function 70 to 90. The last argument is itself a call, with
 * all the rest still pending. */
static int twice(int n)
{
    return n * 2;
}

static int sum(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, int a12, int a13, int a14, int a15, int a16, int a17, int a18, int a19, int a20, int a21, int a22, int a23, int a24, int a25, int a26, int a27, int a28, int a29, int a30, int a31, int a32, int a33, int a34, int a35, int a36, int a37, int a38, int a39, int a40, int a41, int a42, int a43, int a44, int a45, int a46, int a47, int a48, int a49, int a50, int a51, int a52, int a53, int a54, int a55, int a56, int a57, int a58, int a59, int a60, int a61, int a62, int a63, int a64, int a65, int a66, int a67, int a68, int a69, int a70, int a71, int a72, int a73, int a74, int a75, int a76, int a77, int a78, int a79, int a80, int a81, int a82, int a83, int a84, int a85, int a86, int a87, int a88, int a89, int a90, int a91, int a92, int a93, int a94, int a95, int a96, int a97, int a98, int a99, int a100, int a101, int a102, int a103, int a104, int a105, int a106, int a107, int a108, int a109, int a110, int a111, int a112, int a113, int a114, int a115, int a116, int a117, int a118, int a119, int a120, int a121, int a122, int a123, int a124, int a125, int a126)
{
    return a0 + a1 + a63 + a64 + a125 + a126 - (a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + a10 + a11 + a12 + a13 + a14 + a15 + a16 + a17 + a18 + a19 + a20 + a21 + a22 + a23 + a24 + a25 + a26 + a27 + a28 + a29 + a30 + a31 + a32 + a33 + a34 + a35 + a36 + a37 + a38 + a39 + a40 + a41 + a42 + a43 + a44 + a45 + a46 + a47 + a48 + a49 + a50 + a51 + a52 + a53 + a54 + a55 + a56 + a57 + a58 + a59 + a60 + a61 + a62) + 293;
}

int main(void)
{
    return sum(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5,
               twice(4));
}
