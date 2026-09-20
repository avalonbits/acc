/* `static` and the qualifiers inside a parameter's brackets, which C99
 * allows and which say something about the caller rather than change the
 * type: the parameter is the pointer an array parameter always becomes. */
int head(int a[static 3]) { return a[0]; }

int firm(int a[const 3]) { return a[1]; }

int through(char s[restrict]) { return s[0]; }

int both(int a[static const 4]) { return a[3]; }

int row(int m[static 2][3]) { return m[1][2]; }

int plain(int a[]) { return a[0]; }

/* A declaration and a definition that agree, with the brackets on both. */
int later(int a[static 2]);

int main(void) {
    int a[4];
    int m[2][3];
    char s[3];

    a[0] = 1; a[1] = 2; a[2] = 3; a[3] = 4;
    m[0][0] = 5; m[1][2] = 6;
    s[0] = 7;

    if (head(a) != 1 || firm(a) != 2 || both(a) != 4)
        return 1;
    if (through(s) != 7 || row(m) != 6 || plain(a) != 1)
        return 2;
    if (later(a) != 4)
        return 3;

    /* The parameter really is a pointer: it can be moved along. */
    if (sizeof a != 4 * sizeof(int))
        return 4;

    return 42;
}

int later(int a[static 2]) {
    a++;

    return a[1] + 1;
}
