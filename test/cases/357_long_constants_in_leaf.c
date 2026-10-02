/* A narrow constant written as a long, in functions opt-acc's leaf backend
 * makes: its four bytes are the constant's own value widened by its own
 * type. An unsigned int's 0xffffff is held as -1, 24 bits run up through
 * the host's word, and must come out 0x00ffffff, not 0xffffffff -- through
 * a pointer, through a member the pointer to which is in IY, and as the
 * operand of a long's operator. gcc's 20050922-1 found it. */
struct pair { int id; unsigned long big; };

void put(unsigned long *p)
{
    p[0] = 0xffff;
    p[1] = 0xffffff;
    p[2] = 0x800000u;
}

void member(struct pair *h)
{
    h->big = 0xffffff;
}

unsigned long plus(unsigned long x)
{
    return x + 0xffffffu;
}

unsigned long ored(unsigned long x)
{
    return x | 0x800000u;
}

int main(void)
{
    static unsigned long out[3];
    static struct pair h;
    int r = 0;

    put(out);
    r += out[0] == 0xffffUL && out[1] == 0xffffffUL && out[2] == 0x800000UL;
    member(&h);
    r += h.big == 0xffffffUL;
    r += plus(1UL) == 0x1000000UL;
    r += ored(0x1000000UL) == 0x1800000UL;

    return r == 4 ? 42 : r;
}
