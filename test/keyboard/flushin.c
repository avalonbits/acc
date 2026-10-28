/* fflush(stdin) between two reads of the keyboard, as programs written for
 * agondev do: what was typed after the number -- the return that ended it
 * -- is let go of, and the next getchar waits for the next line. C leaves
 * fflush of an input stream undefined; agondev's library drops what the
 * keyboard was holding, and so does acc's. test/keyboard.sh types "12" and
 * a return, then "x" and a return, each time this says READY. */
#include <stdio.h>

int main(void)
{
    long n = 0;
    int c;

    printf("READY\n");
    scanf("%ld", &n);
    fflush(stdin);
    printf("READY\n");
    c = getchar();
    printf("GOT %ld %02x\n", n, c);
    printf("EVENTS\n");

    return 0;
}
