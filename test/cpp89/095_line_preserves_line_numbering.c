int a = __LINE__;
// comment on line 2
int b = __LINE__;
// comment on line 4
int c = __LINE__;
int check() { return (a == 1 && b == 3 && c == 5) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* // preserves line numbering
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
