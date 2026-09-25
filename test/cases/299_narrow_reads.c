/* A narrow value read and then converted to a type as wide -- through a
 * pointer, from a local, returned from a function -- is widened once, as
 * the type it goes to wants; a byte read or returned and branched on is
 * tested in A; and `(x & m) != 0` and `== 0` read the AND's flags. All
 * of it on bytes with the top bit set and without, both signednesses. */

static const signed char sc[] = { -128, -1, 0, 1, 127, -77 };
static const unsigned char uc[] = { 0, 1, 127, 128, 200, 255 };
static const short ss[] = { -32768, -1, 0, 1, 32767, -300 };
static const unsigned short us[] = { 0, 1, 32767, 32768, 50000, 65535 };
static const char pc[] = { 'a', ' ', '\t', (char) 0x80, 0, 'z' };

static _Bool odd(unsigned char c) { return (c & 1) != 0; }
static _Bool none(unsigned char c) { return (c & 0x81) == 0; }
static char ch(int i) { return pc[i]; }
static unsigned char uch(int i) { return uc[i]; }
static signed char sch(int i) { return sc[i]; }

static unsigned long mix(unsigned long h, long x)
{
    return (h * 31 + (unsigned long) x) & 0xffffffffUL;
}

int main(void)
{
    unsigned long h = 0;
    int i;

    for (i = 0; i < 6; i++) {
        const signed char *psc = &sc[i];
        const unsigned char *puc = &uc[i];
        const short *pss = &ss[i];
        const unsigned short *pus = &us[i];
        signed char lsc = sc[i];
        unsigned char luc = uc[i];
        short lss = ss[i];
        unsigned short lus = us[i];

        h = mix(h, (unsigned char) *psc);
        h = mix(h, (signed char) *puc);
        h = mix(h, (char) *puc);
        h = mix(h, (unsigned short) *pss);
        h = mix(h, (short) *pus);
        h = mix(h, (unsigned char) lsc);
        h = mix(h, (signed char) luc);
        h = mix(h, (unsigned short) lss);
        h = mix(h, (short) lus);
        h = mix(h, (unsigned char) ch(i));
        h = mix(h, (signed char) uch(i));
        h = mix(h, (unsigned char) sch(i));
        h = mix(h, odd(*puc) + 2 * none(*puc));
        h = mix(h, (*puc & 0x40) != 0);
        h = mix(h, (lsc & 0x80) == 0);
        if (*psc) h = mix(h, 1);
        if (!*puc) h = mix(h, 2);
        if (odd(luc)) h = mix(h, 3);
        if (!none(luc)) h = mix(h, 4);
        if (ch(i)) h = mix(h, 5);
        if ((lss & 0x100) != 0) h = mix(h, 6);
        if ((*pus & 0x8000) == 0) h = mix(h, 7);
        while (*psc && psc > sc)
            psc--;
        h = mix(h, (long) (psc - sc));
    }

    /* From gcc on the host, where char is signed as it is here. */
    return h == 2145328071UL ? 42 : 1;
}
