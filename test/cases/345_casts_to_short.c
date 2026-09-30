/* A cast to a short in opt-acc's own backend: the top byte of HL cleared
 * for an unsigned short, filled with bit 15 for a signed one. It was left
 * as it was, so `(unsigned short) x >> 8` shifted the top byte down into
 * the answer: gcc's pr64718, a byte swap. Each check on its own. */
static int swap(int x)
{
    return (unsigned short) ((unsigned short) x << 8 | (unsigned short) x >> 8);
}

static int low16(int x) { return (unsigned short) x; }
static int signed16(int x) { return (short) x; }
static int high_of_short(int x) { return (short) x >> 8; }

int main(void)
{
    int right = 0;

    right += swap(0x1234) == 0x3412 && swap(0x561234) == 0x3412;
    right += low16(0x7ffffe) == 0xfffe && low16(-1) == 0xffff;
    right += signed16(0x7ffffe) == -2 && signed16(0x8000) == -32768
             && signed16(0x127fff) == 32767;
    right += high_of_short(0x12ff00) == -1 && high_of_short(0x7f00) == 127;
    return right == 4 ? 42 : right;
}
