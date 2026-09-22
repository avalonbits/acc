char *d = __DATE__;
int check() {
    /* Mmm dd yyyy = 11 chars */
    int len = 0;
    while (d[len]) len++;
    return (len == 11) ? 0 : 1;
}

int main(void) { return check() == 0 ? 42 : 0; }

/* __DATE__ has correct format (Mmm dd yyyy)
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
