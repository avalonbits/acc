/* An address masked, multiplied or complemented in a function: `(int) &v &
 * 7` asks whether v is aligned. acc folded every operation on an address
 * whose place was not settled yet, and refused the ones whose answer no
 * relocation could put right -- rightly for a global's initial value, which
 * has to be bytes now, and wrongly in a function, where the address can be
 * loaded whole and the operation done at run time. Two torture tests,
 * pr23467 and 20050215-1. */
int v[4];
long w;

int main(void)
{
    int r = 0;
    unsigned long a = (unsigned long) &v[1];

    if (((int) &v[1] & 0xffffff) == (int) a) r++;
    if (((unsigned long) &w & 3) == ((unsigned long) &w % 4)) r++;
    if (((int) &v[0] ^ (int) &v[0]) == 0) r++;      /* both loaded whole */
    if (((int) &v[0] * 2) / 2 == (int) &v[0]) r++;
    if ((~(int) &v[1] & 0xffffff) == (~(int) a & 0xffffff)) r++;
    if (-(int) &v[0] + (int) &v[0] == 0) r++;
    if (((int) &v[1] | 1) == ((int) a | 1)) r++;
    if (((int) &v[1] >> 2) == ((int) a >> 2)) r++;

    return r + 34;              /* 8 checks */
}
