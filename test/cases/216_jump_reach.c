/* Jumps at the edge of what a relative one can reach.
 *
 * acc writes every jump as a four-byte `jp` and then, once the file is read
 * and everything is where it will stay, writes the ones whose target is
 * within 127 bytes again as a two-byte `jr`. The two forms count the
 * distance differently -- one is an address and the other is a step from the
 * instruction after the jump -- so the place a mistake in that arithmetic
 * shows is exactly where a jump stops reaching.
 *
 * So these are the same loop and the same `if` at a range of body lengths,
 * a few instructions apart, from comfortably inside that distance to
 * comfortably outside it. One of them is at the edge whatever the bodies
 * happen to compile to, and agondev is asked to agree about all of them.
 */
#define B1  t += 1;
#define B2  B1 B1
#define B4  B2 B2
#define B8  B4 B4
#define B16 B8 B8

static int loop9(int n) { int t = 0; while (n--) { B8 B1 } return t; }
static int loop10(int n) { int t = 0; while (n--) { B8 B2 } return t; }
static int loop11(int n) { int t = 0; while (n--) { B8 B2 B1 } return t; }
static int loop12(int n) { int t = 0; while (n--) { B8 B4 } return t; }
static int loop13(int n) { int t = 0; while (n--) { B8 B4 B1 } return t; }
static int loop14(int n) { int t = 0; while (n--) { B8 B4 B2 } return t; }
static int loop16(int n) { int t = 0; while (n--) { B16 } return t; }
static int loop18(int n) { int t = 0; while (n--) { B16 B2 } return t; }
static int loop20(int n) { int t = 0; while (n--) { B16 B4 } return t; }
static int loop24(int n) { int t = 0; while (n--) { B16 B8 } return t; }

static int skip9(int a) { int t = 0; if (a) { B8 B1 } return t; }
static int skip10(int a) { int t = 0; if (a) { B8 B2 } return t; }
static int skip11(int a) { int t = 0; if (a) { B8 B2 B1 } return t; }
static int skip12(int a) { int t = 0; if (a) { B8 B4 } return t; }
static int skip13(int a) { int t = 0; if (a) { B8 B4 B1 } return t; }
static int skip14(int a) { int t = 0; if (a) { B8 B4 B2 } return t; }
static int skip16(int a) { int t = 0; if (a) { B16 } return t; }
static int skip18(int a) { int t = 0; if (a) { B16 B2 } return t; }
static int skip20(int a) { int t = 0; if (a) { B16 B4 } return t; }
static int skip24(int a) { int t = 0; if (a) { B16 B8 } return t; }

int main(void)
{
    int r = 0;

    if (loop9(3) == 27) r++;
    if (loop10(3) == 30) r++;
    if (loop11(3) == 33) r++;
    if (loop12(3) == 36) r++;
    if (loop13(3) == 39) r++;
    if (loop14(3) == 42) r++;
    if (loop16(3) == 48) r++;
    if (loop18(3) == 54) r++;
    if (loop20(3) == 60) r++;
    if (loop24(3) == 72) r++;
    if (skip9(1) == 9 && skip9(0) == 0) r++;
    if (skip10(1) == 10 && skip10(0) == 0) r++;
    if (skip11(1) == 11 && skip11(0) == 0) r++;
    if (skip12(1) == 12 && skip12(0) == 0) r++;
    if (skip13(1) == 13 && skip13(0) == 0) r++;
    if (skip14(1) == 14 && skip14(0) == 0) r++;
    if (skip16(1) == 16 && skip16(0) == 0) r++;
    if (skip18(1) == 18 && skip18(0) == 0) r++;
    if (skip20(1) == 20 && skip20(0) == 0) r++;
    if (skip24(1) == 24 && skip24(0) == 0) r++;

    return r + 22;                        /* 20 checks */
}
