/* extern: a variable declared before it is defined, used in between, and
 * then given its value; a function declared extern; and a declaration in a
 * block that names the file-scope one. */
extern int limit;
extern long big;
extern int values[4];
extern int scale(int);

/* Said three times, given its value by the one in the middle. */
int twice;
int twice = 7;
int twice;

struct pair { int a, b; };
extern struct pair both;

int use(void) {
    return limit + values[2] + big / 1000;
}

int first_pair(void) {
    return both.a * 10 + both.b;
}

struct pair both = { 3, 4 };

int limit = 10;
long big = 5000;
int values[4] = { 1, 2, 3, 4 };

int scale(int x) {
    return x * limit;
}

int main(void) {
    int r = 0;

    if (use() == 10 + 3 + 5) r++;
    limit = 20;
    if (scale(2) == 40) r++;
    {
        extern int limit;

        if (limit == 20) r++;
    }

    if (twice == 7 && first_pair() == 34) r++;

    return r + 38;          /* 4 checks */
}
