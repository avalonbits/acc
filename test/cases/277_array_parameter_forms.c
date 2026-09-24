/* A parameter's array forms that C99 allows and acc refused: a row whose
 * length is worked out, `int a[][n]` or `[*]`, in a prototype; a pointer to
 * a row of unknown size, `int (*p)[]`; declarations of one thing that differ
 * only in what they say of an array's length, which C99 6.7.5.2p6 makes
 * compatible; and twelve dimensions, which 5.2.4.1 asks a compiler to take.
 * Fifteen of gcc.dg's tests declare these. */
void rows(int n, short a[][n]);
void star(int n, double x[3][*]);
void unknown(int (*p)[]);
void unknown(int (*p)[3]);

extern int (*const table)[];
extern int (*const table)[10];

typedef int deep[1][1][1][1][1][1][1][1][1][1][1][2];

void unknown(int (*p)[3])
{
    (*p)[1] = 41;
}

int main(void)
{
    int three[3] = { 0, 0, 0 };
    deep d;

    unknown(&three);
    d[0][0][0][0][0][0][0][0][0][0][0][1] = 1;

    return three[1] + d[0][0][0][0][0][0][0][0][0][0][0][1];
}
