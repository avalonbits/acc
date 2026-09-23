/* main runs on a stack of its own, at the top of the program's memory.
 *
 * MOS calls a program on MOS's stack, which is in MOS's own RAM above the
 * program's, with MOS's variables below it. acc used to leave the program
 * there, and a program a few tens of kilobytes deep wrote over those
 * variables; the next interrupt took the machine down. gcc's multi-ix is a
 * function with forty 1,500-byte arrays in its frame.
 *
 * The startup now keeps MOS's stack pointer, moves to the top of the
 * program's memory -- where the heap already stops short for the stack to
 * come down into -- and puts MOS's back before it returns.
 *
 * Here and not in test/cases because agondev's reference build starts the
 * way the oracle's startup does, on MOS's stack, and cannot run this.
 */
#define TOP 0xb0000UL           /* the end of the program's 448 KB */

typedef int row[400];

static int deep(void) {
    row a0, a1, a2, a3, a4, a5, a6, a7, a8, a9;
    row a10, a11, a12, a13, a14, a15, a16, a17, a18, a19;
    row a20, a21, a22, a23, a24, a25, a26, a27, a28, a29;
    row a30, a31, a32, a33, a34, a35, a36, a37, a38, a39;
    int *rows[40];
    int i, j;

    rows[0] = a0;   rows[1] = a1;   rows[2] = a2;   rows[3] = a3;
    rows[4] = a4;   rows[5] = a5;   rows[6] = a6;   rows[7] = a7;
    rows[8] = a8;   rows[9] = a9;   rows[10] = a10; rows[11] = a11;
    rows[12] = a12; rows[13] = a13; rows[14] = a14; rows[15] = a15;
    rows[16] = a16; rows[17] = a17; rows[18] = a18; rows[19] = a19;
    rows[20] = a20; rows[21] = a21; rows[22] = a22; rows[23] = a23;
    rows[24] = a24; rows[25] = a25; rows[26] = a26; rows[27] = a27;
    rows[28] = a28; rows[29] = a29; rows[30] = a30; rows[31] = a31;
    rows[32] = a32; rows[33] = a33; rows[34] = a34; rows[35] = a35;
    rows[36] = a36; rows[37] = a37; rows[38] = a38; rows[39] = a39;

    /* Every byte of 48 KB written, then read back. */
    for (i = 0; i < 40; i++)
        for (j = 0; j < 400; j++)
            rows[i][j] = i * 1000 + j;
    for (i = 0; i < 40; i++)
        for (j = 0; j < 400; j++)
            if (rows[i][j] != i * 1000 + j)
                return 0;

    return 1;
}

int main(void) {
    int r = 0;
    int here;

    /* In the program's memory, and not in MOS's above it: checked first,
     * so that the wrong stack says so rather than taking the machine down
     * in what follows. */
    if ((unsigned long) &here >= TOP)
        return 1;
    r++;
    if ((unsigned long) &here > TOP - 256)
        r++;                    /* and near its top, not somewhere else */

    if (deep()) r++;

    return r + 39;              /* 3 checks */
}
