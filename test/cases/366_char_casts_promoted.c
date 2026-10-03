/* A value cast to a char and promoted again: widened as the char it was
 * made, not as the int it is held in -- (unsigned char) 510 is 254, half
 * of it 127, where widened by its sign it was -2 and half of that 255 in
 * the byte. And a constant cast the same way. As gcc's 20020614-1 has it,
 * which opt-acc's machine-level backend got wrong. */
int halve_signed(int i)
{
    signed char j = ((signed char) (i << 1)) / 2;

    return j;
}

int halve_unsigned(int i)
{
    unsigned char k = ((unsigned char) (i << 1)) / 2;

    return k;
}

int unsigned_const(void)
{
    return (unsigned char) 254 + 0;
}

int signed_const(void)
{
    return (signed char) 254 + 0;
}

int main(void)
{
    int r = 0;

    r += halve_signed(127) == -1;
    r += halve_unsigned(255) == 127;
    r += unsigned_const() == 254;
    r += signed_const() == -2;

    return r == 4 ? 42 : r;
}
