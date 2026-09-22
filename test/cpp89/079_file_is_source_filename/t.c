char *f = __FILE__;
int check() {
    /* Check that filename contains "t.c" */
    char *p = f;
    while (*p) {
        if (p[0] == 't' && p[1] == '.' && p[2] == 'c')
            return 0;
        p++;
    }
    return 1;
}

int main(void) { return check() == 0 ? 42 : 0; }

/* __FILE__ is source filename
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
