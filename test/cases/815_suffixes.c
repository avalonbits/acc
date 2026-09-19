/* Integer constants with suffixes, in every base: the hex ones were read
 * with their suffix as a digit and refused. Their types follow C99's
 * table, which sizeof and signedness show. */
int main(void) {
    int r = 0;

    if (0xffu == 255 && 0x10L == 16 && 0x10UL + 0x1ul == 17) r++;
    if (sizeof 0x10L == 4 && sizeof(0xFFu) == 3 && sizeof 0xffffffU == 3) r++;
    if (0xFFu > 0 && -0x1u > 0 && 0x7fffffffL > 0) r++;
    if (017L == 15 && sizeof 017L == 4 && 10u == 10 && sizeof 10lu == 4) r++;
    if (0xABCDEFul == 11259375L && 0XaL == 10) r++;

    return r + 37;          /* 5 checks */
}
