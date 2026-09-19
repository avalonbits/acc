/* Prototypes: a function declared before it is defined, so that calls to it
 * convert their arguments and read its result by the declared types -- a
 * long, a char, a struct, void -- and declared again, in a block and with
 * or without parameter names. */
struct pair { int a, b; };

long widen(char c, long l);
char narrow(int);
struct pair make(int a, int b);
void bump(int *p);
int count(void);
int twice(int x), thrice(int);

int total;

int main(void) {
    int r = 0;
    int n = 5;
    struct pair p;

    if (widen(3, 100000) == 100003) r++;        /* 3 as a char, one slot */
    if (narrow(0x1ff) == -1) r++;               /* the char, sign-extended */
    p = make(4, 7);
    if (p.a == 4 && p.b == 7) r++;
    bump(&n);
    if (n == 6) r++;
    if (count() == 1 && count() == 2) r++;
    if (twice(3) == 6 && thrice(3) == 9) r++;
    {
        int later(int);

        if (later(2) == 20) r++;
    }

    return r + 35;          /* 7 checks */
}

long widen(char c, long l) {
    return c + l;
}

char narrow(int x) {
    return x;
}

struct pair make(int a, int b) {
    struct pair p;

    p.a = a;
    p.b = b;

    return p;
}

void bump(int *p) {
    (*p)++;
}

int count(void) {
    return ++total;
}

int twice(int x) {
    return 2 * x;
}

int thrice(int x) {
    return 3 * x;
}

int later(int x) {
    return x * 10;
}
