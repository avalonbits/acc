/* Arrays declared with no size, used, and only then defined -- by an
 * initialiser that says how long, by a size alone, in a block. */
extern int values[];
extern char name[];
extern const int limits[];
struct pair { int a, b; };
extern struct pair pairs[];

int sum(int n) {
    int s = 0;

    for (int i = 0; i < n; i++)
        s += values[i];

    return s;
}

int second(void) {
    extern int later[];

    return later[1];
}

int values[] = { 1, 2, 3, 4 };
char name[6] = "hello";
const int limits[2] = { 7, 8 };
struct pair pairs[2] = { { 1, 2 }, { 3, 4 } };
int later[3] = { 5, 6, 7 };

int main(void) {
    int r = 0;
    int *p = values;

    if (sum(4) == 10 && sizeof values == 12) r++;
    if (name[1] == 'e' && limits[1] == 8) r++;
    if (pairs[1].b == 4 && second() == 6) r++;
    p[0] = 10;
    if (sum(1) == 10) r++;

    return r + 38;          /* 4 checks */
}
