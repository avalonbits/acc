/* const: on variables, parameters, pointers either side of the star,
 * members and globals, read as any other would be. */
const int answer = 42;
const char *const greeting = "hi";
const int table[3] = { 1, 2, 3 };
const char initial = 'a';

struct box { const int id; int value; };

int sum(const int *p, const int n) {
    int s = 0;

    for (int i = 0; i < n; i++)
        s += p[i];

    return s;
}

int main(void) {
    int r = 0;
    const int local = 7;
    int x = 3;
    int *const fixed = &x;
    const struct box b = { 9, 10 };
    const char c = 'q';

    if (answer == 42 && local == 7) r++;
    *fixed = 4;
    if (x == 4) r++;
    if (greeting[1] == 'i' && sum(table, 3) == 6) r++;
    if (b.id + b.value == 19 && c == 'q') r++;
    if (sizeof(const int) == 3 && (const int) 5 == 5) r++;

    {
        const int *pa = &answer, *pl = &local;
        struct box *const pb = (struct box *) &b;

        if (*pa == 42 && *pl == 7 && pb->value == 10) r++;
        if (sizeof initial == 1 && sizeof c == 1 && sizeof(answer) == 3) r++;
    }

    return r + 35;          /* 7 checks */
}
