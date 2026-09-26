/* Members and elements of variables declared extern and defined further
 * down, whose addresses the end of the file fills in: acc adds a constant
 * offset into the slot the address goes in, rather than loading it and
 * adding. Positive offsets, negative ones through a pointer taken back,
 * members of elements, and addresses kept and compared. */
struct rec { char tag; int n; long big; short w[3]; };

extern struct rec one;
extern struct rec many[4];
extern int nums[8];
extern char text[16];

static unsigned long h = 1;

static void mix(long v)
{
    h = (h * 31 + (unsigned long) v) & 0xffffffffUL;
}

static void fill(void)
{
    int i;

    one.tag = 'o';
    one.n = -300;
    one.big = 123456789L;
    one.w[2] = 77;
    for (i = 0; i < 4; i++) {
        many[i].tag = (char) ('a' + i);
        many[i].n = i * 1000;
        many[i].w[1] = (short) -i;
    }
    many[3].big = -9L;
    nums[0] = 5;
    nums[7] = 70000;
    text[0] = 'h';
    text[15] = '!';
}

static void read_back(void)
{
    int *end = &nums[8];
    struct rec *last = &many[3];

    mix(one.tag); mix(one.n); mix(one.big); mix(one.w[2]);
    mix(many[2].tag); mix(many[3].n); mix(many[1].w[1]); mix(many[3].big);
    mix(nums[0]); mix(nums[7]); mix(end[-1]); mix(*(end - 8));
    mix(text[0]); mix(text[15]);
    mix(last->n); mix((last - 1)->tag); mix(last - &many[0]);
    mix(&nums[7] - &nums[2]);
    mix(*(&nums[8] - 1)); mix((&many[4] - 2)->n);
    mix(&one.w[2] == &one.w[0] + 2);
    mix((char *) &many[1] - (char *) &many[0] == sizeof(struct rec));
}

struct rec one;
struct rec many[4];
int nums[8];
char text[16];

int main(void)
{
    fill();
    read_back();

    /* From gcc on the host. */
    return h == 973955921UL ? 42 : 1;
}
