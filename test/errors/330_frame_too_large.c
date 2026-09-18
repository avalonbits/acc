/* expect: 48: error: this function's frame is too large: a local at -129 is out of reach of (ix+d), which spans -128 to 127 */
/* (ix+d) is a signed byte, so a frame reaches 128 bytes below ix and no
 * further: 42 ints, and the 43rd is refused where it is declared rather
 * than addressed through a displacement that has wrapped round. */
int main(void) {
    int v0 = 0;
    int v1 = 1;
    int v2 = 2;
    int v3 = 3;
    int v4 = 4;
    int v5 = 5;
    int v6 = 6;
    int v7 = 7;
    int v8 = 8;
    int v9 = 9;
    int v10 = 10;
    int v11 = 11;
    int v12 = 12;
    int v13 = 13;
    int v14 = 14;
    int v15 = 15;
    int v16 = 16;
    int v17 = 17;
    int v18 = 18;
    int v19 = 19;
    int v20 = 20;
    int v21 = 21;
    int v22 = 22;
    int v23 = 23;
    int v24 = 24;
    int v25 = 25;
    int v26 = 26;
    int v27 = 27;
    int v28 = 28;
    int v29 = 29;
    int v30 = 30;
    int v31 = 31;
    int v32 = 32;
    int v33 = 33;
    int v34 = 34;
    int v35 = 35;
    int v36 = 36;
    int v37 = 37;
    int v38 = 38;
    int v39 = 39;
    int v40 = 40;
    int v41 = 41;
    int v42 = 42;
    return 42;
}
