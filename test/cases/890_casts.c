/* Casts: narrowing and widening, signedness, floating conversions, and
 * pointers of one type to another. */
int main(void) {
    int r = 0;
    int i = 0x12345;
    long l = 70000;
    float f = 2.75;
    char buf[4] = { 1, 2, 3, 4 };
    void *v = buf;

    if ((char) i == 0x45 && (unsigned char) -1 == 255) r++;
    if ((short) l == 4464 && (int) (long) -1 == -1) r++;
    if ((int) f == 2 && (float) 3 == 3.0) r++;
    if (((char *) v)[2] == 3) r++;
    if (*(short *) buf == 0x0201) r++;
    if ((long) i * 256 == 0x1234500) r++;
    if ((unsigned) -1 > 0) r++;

    return r + 35;          /* 7 checks */
}
