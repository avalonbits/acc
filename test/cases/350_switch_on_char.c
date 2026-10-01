/* A switch on a char compares the byte: signed and unsigned, with cases it
 * can never equal (no test for those), negative ones, and a char past 127. */
static int on_signed(signed char c)
{
    switch (c) {
    case -128: return 1;
    case -1:   return 2;
    case 0:    return 3;
    case 127:  return 4;
    case 128:  return 5;            /* never: a signed char cannot be 128 */
    case 255:  return 6;            /* nor 255 */
    case 'x':  return 7;
    }

    return 0;
}

static int on_unsigned(unsigned char c)
{
    switch (c) {
    case 0:    return 1;
    case 200:  return 2;
    case 255:  return 3;
    case -1:   return 4;            /* never: an unsigned char is not -1 */
    case 256:  return 5;            /* nor 256 */
    case 'x':  return 6;
    }

    return 0;
}

static int counted(const char *s)
{
    int digits = 0, spaces = 0, other = 0;

    for (; *s; s++)
        switch (*s) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            digits++;
            break;
        case ' ':
            spaces++;
            break;
        default:
            other++;
        }

    return digits * 100 + spaces * 10 + other;
}

int main(void)
{
    int right = 0;

    right += on_signed(-128) == 1 && on_signed(-1) == 2 && on_signed(0) == 3
             && on_signed(127) == 4 && on_signed('x') == 7 && on_signed(5) == 0;
    right += on_signed((signed char) 128) == 1 && on_signed((signed char) 255) == 2;
    right += on_unsigned(0) == 1 && on_unsigned(200) == 2 && on_unsigned(255) == 3
             && on_unsigned('x') == 6 && on_unsigned(1) == 0;
    right += on_unsigned((unsigned char) -1) == 3 && on_unsigned((unsigned char) 256) == 1;
    right += counted("a1 b22 c333") == 623;
    return right == 5 ? 42 : right;
}
