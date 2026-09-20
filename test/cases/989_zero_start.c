/* Variables with no initial value, which C says start at zero -- and which
 * acc now gives room past the image's last byte rather than in the file.
 *
 * Every shape of them: a scalar, an array, a char array, a struct, a
 * pointer, a long, a file-scope static, and one said twice and given a value
 * the second time, which is not one of them after all. */
int counter;
int table[8];
char text[16];
struct point { int x, y; };
struct point origin;
int *slot;
long wide;
static int hidden;

int given;
int given = 7;

int sum(void) {
    int i, s = 0;

    for (i = 0; i < 8; i++)
        s += table[i];

    return s;
}

int main(void) {
    int r = 0;

    if (counter == 0 && hidden == 0 && wide == 0) r++;
    if (origin.x == 0 && origin.y == 0 && slot == 0) r++;
    if (text[0] == 0 && text[15] == 0 && sum() == 0) r++;

    counter = 10;
    table[3] = 5;
    text[2] = 'a';
    origin.y = 4;
    slot = &counter;

    if (counter + table[3] + origin.y == 19) r++;
    if (*slot == 10 && text[2] == 'a') r++;
    if (given == 7) r++;

    return r + 36;              /* 6 checks */
}
