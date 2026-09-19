/* Row shapes past what the type byte alone could name, and a pointer to an
 * array carried through every path a value takes: ?:, both kinds of step,
 * compound assignment, a pointer to it, and a global.
 *
 * The type's extension byte travels beside it, so each of these is a place
 * it could be dropped -- and a pointer to an array that lost it would step by
 * the wrong amount or read the wrong row.
 */
int shape1[2][1];
int shape2[2][2];
int shape3[2][3];
int shape4[2][4];
int shape5[2][5];
int shape6[2][6];
int shape7[2][7];
int shape8[2][8];
int shape9[2][9];
int shape10[2][10];
int shape11[2][11];
int shape12[2][12];
int shape13[2][13];
int shape14[2][14];
int shape15[2][15];
int shape16[2][16];
int shape17[2][17];
int shape18[2][18];
int shape19[2][19];
int shape20[2][20];
int shape21[2][21];
int shape22[2][22];
int shape23[2][23];
int shape24[2][24];
int shape25[2][25];
int shape26[2][26];
int shape27[2][27];
int shape28[2][28];
int shape29[2][29];
int shape30[2][30];

int (*global_row)[4];
int grid[3][4] = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}};

int shapes(void) {
    int sum = 0;

    shape1[1][0] = 1;
    shape2[1][1] = 2;
    shape3[1][2] = 3;
    shape4[1][3] = 4;
    shape5[1][4] = 5;
    shape6[1][5] = 6;
    shape7[1][6] = 7;
    shape8[1][7] = 8;
    shape9[1][8] = 9;
    shape10[1][9] = 10;
    shape11[1][10] = 11;
    shape12[1][11] = 12;
    shape13[1][12] = 13;
    shape14[1][13] = 14;
    shape15[1][14] = 15;
    shape16[1][15] = 16;
    shape17[1][16] = 17;
    shape18[1][17] = 18;
    shape19[1][18] = 19;
    shape20[1][19] = 20;
    shape21[1][20] = 21;
    shape22[1][21] = 22;
    shape23[1][22] = 23;
    shape24[1][23] = 24;
    shape25[1][24] = 25;
    shape26[1][25] = 26;
    shape27[1][26] = 27;
    shape28[1][27] = 28;
    shape29[1][28] = 29;
    shape30[1][29] = 30;

    sum += shape1[1][0] + shape1[0][0];
    sum += shape2[1][1] + shape2[0][0];
    sum += shape3[1][2] + shape3[0][0];
    sum += shape4[1][3] + shape4[0][0];
    sum += shape5[1][4] + shape5[0][0];
    sum += shape6[1][5] + shape6[0][0];
    sum += shape7[1][6] + shape7[0][0];
    sum += shape8[1][7] + shape8[0][0];
    sum += shape9[1][8] + shape9[0][0];
    sum += shape10[1][9] + shape10[0][0];
    sum += shape11[1][10] + shape11[0][0];
    sum += shape12[1][11] + shape12[0][0];
    sum += shape13[1][12] + shape13[0][0];
    sum += shape14[1][13] + shape14[0][0];
    sum += shape15[1][14] + shape15[0][0];
    sum += shape16[1][15] + shape16[0][0];
    sum += shape17[1][16] + shape17[0][0];
    sum += shape18[1][17] + shape18[0][0];
    sum += shape19[1][18] + shape19[0][0];
    sum += shape20[1][19] + shape20[0][0];
    sum += shape21[1][20] + shape21[0][0];
    sum += shape22[1][21] + shape22[0][0];
    sum += shape23[1][22] + shape23[0][0];
    sum += shape24[1][23] + shape24[0][0];
    sum += shape25[1][24] + shape25[0][0];
    sum += shape26[1][25] + shape26[0][0];
    sum += shape27[1][26] + shape27[0][0];
    sum += shape28[1][27] + shape28[0][0];
    sum += shape29[1][28] + shape29[0][0];
    sum += shape30[1][29] + shape30[0][0];

    return sum;
}

int main(void) {
    int r = 0;
    int (*p)[4] = grid;
    int (*q)[4] = grid + 2;
    int (**pp)[4] = &p;
    int flag = 1;

    /* 1 + 2 + ... + 30, with every row's other elements zero. */
    if (shapes() == 465) r = r + 1;

    /* Through ?:, from either side. */
    if ((flag ? p : q)[1][1] == 6 && (flag ? q : p)[0][3] == 12) r = r + 1;

    /* Postfix and prefix steps, and compound assignment. */
    p++;
    if (p[0][0] == 5) r = r + 1;
    --p;
    p += 2;
    if (p[0][1] == 10 && (*p)[2] == 11) r = r + 1;

    /* Through a pointer to it. */
    p = grid;
    if ((*pp)[2][3] == 12 && (*pp + 1)[0][0] == 5) r = r + 1;

    /* A global one, and its step inside a parenthesised statement. */
    global_row = grid;
    (*(global_row + 1))[2] = 70;
    if (grid[1][2] == 70 && global_row[2][0] == 9) r = r + 1;

    /* 6 */
    return r + 36;
}
