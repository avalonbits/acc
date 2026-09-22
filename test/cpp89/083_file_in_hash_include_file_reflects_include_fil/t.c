#include "check_file.h"
int check() {
    char *p = inc_file;
    while (*p) {
        if (p[0]=='c' && p[1]=='h' && p[2]=='e' && p[3]=='c' && p[4]=='k')
            return 0;
        p++;
    }
    return 1;
}

int main(void) { return check() == 0 ? 42 : 0; }

/* __FILE__ in #include file reflects include file
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
