/* typedef: names for scalar types, pointers, arrays and structs, used in
 * declarations, parameters, casts and sizeof, declared in a block, and
 * shadowed by a variable. */
typedef unsigned char byte;
typedef int *intp;
typedef int row[3];
typedef row matrix[2];
typedef byte bytes4[4], *bytep;
typedef int (*rowp)[3];
typedef struct node { int value; struct node *next; } node;
typedef struct { byte r, g, b; } rgb;

int sum_row(row x) {
    return x[0] + x[1] + x[2];
}

byte low(int v) {
    return (byte) v;
}

matrix global_m = { { 1, 1, 1 }, { 2, 2, 2 } };

int main(void) {
    int r = 0;
    byte b = 255;
    int n = 5;
    intp p = &n;
    row m[2] = { { 1, 2, 3 }, { 4, 5, 6 } };
    bytes4 four = { 1, 2, 3, 4 };
    bytep bp = four;
    rowp rp = m;
    node a, z;
    rgb c = { 1, 2, 3 };

    b++;
    if (b == 0) r++;
    *p = 6;
    if (n == 6) r++;
    if (sum_row(m[1]) == 15 && sizeof m == 18 && sizeof(row) == 9) r++;
    if (sizeof(matrix) == 18 && sum_row(global_m[1]) == 6) r++;
    if (sizeof four == 4 && bp[3] == 4 && sizeof(bytep) == 3) r++;
    rp++;
    if ((*rp)[2] == 6) r++;
    if (low(0x1234) == 0x34) r++;
    a.value = 1;
    a.next = &z;
    z.value = 2;
    z.next = 0;
    if (a.next->value == 2 && sizeof(rgb) == 3 && c.b == 3) r++;
    {
        int byte = 3;           /* a variable, here, not a type */

        if (byte * 2 == 6) r++;
    }
    {
        typedef long wide;
        wide w = 100000;

        if (sizeof w == 4 && w == 100000) r++;
    }
    for (byte i = 250; i != 0; i++)
        n++;
    if (n == 12) r++;

    return r + 31;          /* 11 checks */
}
