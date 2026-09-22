#define F(x) ((x) + 1)
#define G(x) F(F(x))
#define H(x) G(G(x))
int check() { return (H(0) == 4) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* Triple nested prescan: H(0)
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
