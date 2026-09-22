char *t = __TIME__;
int check() {
    /* hh:mm:ss = 8 chars */
    int len = 0;
    while (t[len]) len++;
    return (len == 8 && t[2] == ':' && t[5] == ':') ? 0 : 1;
}

int main(void) { return check() == 0 ? 42 : 0; }

/* __TIME__ has correct format (hh:mm:ss)
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
