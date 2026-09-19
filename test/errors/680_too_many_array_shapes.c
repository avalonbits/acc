/* expect: 25: error: this program has more than 22 different array shapes, which acc cannot tell apart yet */
/* Row shapes are named by the type codes no scalar uses, and there are 22. */
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

int main(void) {
    return 0;
}
