/* More locals than the frame pointer can reach.
 *
 * (ix+d) carries one signed byte, so the frame reaches 128 bytes below ix
 * and the scratch area shares them. A function with a few dozen variables
 * in it ran out and acc refused to compile it at all -- which is a ceiling
 * on how large a function may be, not a property of C. What a function
 * declares past the near window now goes where the arrays and the structs
 * already went, and every use of it works out its address.
 *
 * The block of ints below is longer than (ix+d) reaches on its own, so
 * everything declared after it is the far kind and the boundary between the
 * two falls inside the block -- which is what makes the sum at the end worth
 * taking: it reads every one of them, from both sides of that boundary. The
 * checks are the ordinary things a variable has to do: be read, be assigned,
 * have its address taken, be counted up, be passed and be pointed at.
 */
static int twice(int n) { return n * 2; }
static void bump(int *p) { *p += 7; }

int main(void) {
    /* A hundred and thirty-two bytes of ints, which is past what (ix+d)
     * reaches however the window is divided, so that everything declared
     * after it is the far kind whatever the split becomes. */
    int n0=0, n1=1, n2=2, n3=3, n4=4, n5=5, n6=6, n7=7, n8=8, n9=9;
    int n10=10, n11=11, n12=12, n13=13, n14=14, n15=15, n16=16, n17=17;
    int n18=18, n19=19, n20=20, n21=21, n22=22, n23=23, n24=24, n25=25;
    int n26=26, n27=27, n28=28, n29=29, n30=30, n31=31, n32=32, n33=33;
    int n34=34, n35=35, n36=36, n37=37, n38=38, n39=39, n40=40, n41=41;
    int n42=42, n43=43;

    /* All of these are past the window. */
    char c = 'A';
    short s = 300;
    int i = 1000;
    long l = 100000L;
    long long q = 8000000000LL;
    float f = 1.5f;
    int *p;
    int arr[4];
    int r = 0;

    /* Read and write, at every width. */
    if (c == 'A' && s == 300 && i == 1000) r++;
    if (l == 100000L && q == 8000000000LL && f == 1.5f) r++;
    c = 'Z'; s = -300; i = -1000;
    if (c == 'Z' && s == -300 && i == -1000) r++;
    l = -100000L; q = -8000000000LL; f = f * 2.0f;
    if (l == -100000L && q == -8000000000LL && f == 3.0f) r++;

    /* The near ones still work, and still hold what they were given. */
    if (n0 == 0 && n15 == 15 && n29 == 29) r++;
    n15 = n15 + n29;
    if (n15 == 44) r++;

    /* Counted up and down, and assigned to compoundly. */
    i = 10;
    i++; ++i; i--;
    i += 5; i *= 2; i -= 3;
    if (i == 29) r++;
    s = 4; s <<= 3;
    if (s == 32) r++;

    /* Its address, taken and used -- by the program and by something else. */
    p = &i;
    *p = 40;
    if (i == 40) r++;
    bump(&i);
    if (i == 47) r++;
    if (p == &i && *p == 47) r++;

    /* Passed as a value, and used as a subscript. */
    i = 3;
    if (twice(i) == 6) r++;
    arr[0] = 10; arr[1] = 11; arr[2] = 12; arr[3] = 13;
    if (arr[i] == 13 && arr[n1] == 11) r++;

    /* A far one and a near one are different objects. */
    if (&i != &n0 && (char *) &i != (char *) 0) r++;

    /* And the sum of the near block is what it should be, which says none
     * of them was written over by a far one landing on top of it. */
    {
        int sum = n0+n1+n2+n3+n4+n5+n6+n7+n8+n9
                + n10+n11+n12+n13+n14+n15+n16+n17+n18+n19
                + n20+n21+n22+n23+n24+n25+n26+n27+n28+n29
                + n30+n31+n32+n33+n34+n35+n36+n37+n38+n39
                + n40+n41+n42+n43;

        if (sum == 946 + 29) r++;       /* n15 gained n29 above */
    }

    return r + 27;              /* 15 checks */
}
