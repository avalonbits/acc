int a = 1; /* block */
int b = 2; // line
/* multi
   line */
int c = 3;
// another line
int d = 4;
int check() { return (a + b + c + d == 10) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* // mixed with / * * / comments
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
