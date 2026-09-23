/* Two things gcc's rotate tests found.
 *
 * sizeof of a cast. A cast's result has the type it names, so
 * `sizeof ((char) v)` is one. acc left the result promoted to int, ready for
 * the arithmetic that usually follows a cast and that sizeof does not do,
 * and answered three. The rotate macros in 20020226-1 and four more work
 * out a rotation's width from `sizeof (a) * CHAR_BIT`, with `a` a cast, and
 * rotated a char by twenty bits instead of eight.
 *
 * The type of a folded shift. C99 6.5.7 gives a shift the promoted type of
 * its left operand and nothing from its count, so `-64 >> 3U` is a signed
 * -8. The code for a shift at run time already said so; the folder took the
 * type from either side, as the other operators do, and made it unsigned.
 * A rotation's count is `sizeof (a) * CHAR_BIT - b`, which is unsigned.
 */
int v = 1000;
short s = (short) 0xf234;
int shift1 = 4;

int main(void) {
    int r = 0;

    /* The size of what a cast makes. */
    if (sizeof ((char) v) == 1 && sizeof ((unsigned char) 0x1234U) == 1) r++;
    if (sizeof ((short) v) == 2 && sizeof ((long) v) == 4) r++;
    if (sizeof ((long long) v) == 8 && sizeof ((int) 'a') == sizeof (int)) r++;

    /* A shift is the type of its left operand, folded or not. */
    if ((-64 >> 3U) == -8 && (-64 >> 3U) < 0) r++;
    if (((short) 0xf234 >> 12U) == -1) r++;
    if ((1 << 3U) - 9 < 0) r++;
    if ((s >> (16U - shift1)) == -1) r++;

    /* And the rotation the tests were for, a char and a short, both ways. */
    {
        unsigned char uc = 0x34;
        short t = (short) 0xf234;

        if ((((uc >> shift1) | (uc << (sizeof (uc) * 8 - shift1))) & 0xff) == 0x43
            && ((((unsigned char) 0x1234U >> 4)
                 | ((unsigned char) 0x1234U << (sizeof ((unsigned char) 0x1234U) * 8 - 4)))
                & 0xff) == 0x43
            && ((t << shift1) | (t >> (sizeof (t) * 8 - shift1)))
               == (((short) 0xf234 << 4) | ((short) 0xf234 >> (sizeof ((short) 0xf234) * 8 - 4))))
            r++;
    }

    return r + 34;              /* 8 checks */
}
