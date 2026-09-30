/* Members of a global struct, read and written by opt-acc's own backend as
 * (nn): the global's address with the member's offset in the instruction,
 * which the link adds the address to -- ld hl, (nn), ld a, (nn),
 * ld (nn), hl, ld (nn), a -- a member past 127 bytes in included; of a
 * global whose address the link fills in, and of one it is known of. */
struct state {
    unsigned char mode;
    int count;
    char pad[140];
    signed char last;
    int total;
};

/* Defined at the end: until then, its address is the link's to fill in,
 * and a member's offset rides in the instruction for it to add to. */
extern struct state st;

static int bump(int by)
{
    st.count = st.count + by;
    st.mode = (unsigned char) (st.mode | 2);

    return st.count;
}

static int far_members(int v)
{
    st.total = v;
    st.last = (signed char) -v;

    return st.total + st.last;
}

struct state st;

/* And one never declared elsewhere, defined before it is used: its
 * members' addresses are constants, moved to where the bss is. */
static struct { int x; unsigned char y; } small;

/* And one with an initial value, in the image: a relocation moves it. */
static struct { int a; unsigned char b; } given = { 7, 9 };

static int from_image(void)
{
    given.b = (unsigned char) (given.b + 1);

    return given.a + given.b;
}

static int count_again(void)
{
    small.x = st.count - 1;
    small.y = (unsigned char) (small.x - 1000);

    return small.x + small.y;
}

int main(void)
{
    int right = 0;

    st.mode = 1;
    st.count = 1000;
    right += bump(234) == 1234 && st.count == 1234;
    right += st.mode == 3;
    right += far_members(5) == 0 && st.last == -5 && st.total == 5;
    right += far_members(300) == 300 - 44;      /* -300 as a signed char is -44 */
    right += count_again() == 1233 + 233 && small.y == 233;
    right += from_image() == 17 && given.b == 10;
    return right == 6 ? 42 : right;
}
