/* What printf makes of a format, printed rather than measured.
 *
 * Every line here is written by acc's printf on the Agon and by the host's
 * on the machine running the test, and the two have to agree character for
 * character. What that pins is the part of printf that is arithmetic and
 * rules rather than machinery: where the padding goes, what a precision does
 * to a number and to a string, how a negative number is signed, what the
 * hex digits are.
 *
 * The values are all inside a 24-bit int and a 32-bit long, which is where
 * the two machines still agree about what a number is. `%p` is not here: the
 * address a pointer holds is not the same on both, and its spelling is the
 * implementation's to choose.
 *
 * Only `\n` ends a line: the host writes one byte for it and MOS's console
 * takes it, and the test strips the carriage returns MOS adds. */

#include <stdio.h>

int main(void)
{
    int n = 0;

    printf("plain\n");
    printf("[%d] [%d] [%d]\n", 0, 42, -42);
    printf("[%5d] [%-5d] [%05d]\n", 42, 42, 42);
    printf("[%5d] [%-5d] [%05d]\n", -42, -42, -42);
    printf("[%+d] [% d] [%+d]\n", 42, 42, -42);
    printf("[%u] [%7u] [%-7u]\n", 4000000u, 12345u, 12345u);
    printf("[%x] [%X] [%08x] [%#o]\n", 48879u, 48879u, 255u, 8u);
    printf("[%o] [%o]\n", 0u, 511u);
    printf("[%ld] [%ld] [%lu]\n", 100000L, -100000L, 4000000000UL);
    printf("[%lX] [%lx]\n", 3735928559UL, 3735928559UL);
    /* The widest there is, which is the path every other number goes down
     * after being widened to it: a 64-bit divide, which this chip does by
     * calling for it. */
    printf("[%llu] [%lld]\n", 12345678901234ULL, -12345678901234LL);
    printf("[%llx] [%#llX]\n", 81985529216486895ULL, 81985529216486895ULL);
    printf("[%s] [%10s] [%-10s]\n", "abc", "abc", "abc");
    printf("[%.2s] [%.0s] [%.10s]\n", "abcdef", "abcdef", "abc");
    printf("[%*d] [%-*d] [%.*d]\n", 6, 42, 6, 42, 5, 42);
    printf("[%.*s] [%*s]\n", 3, "abcdef", 5, "ab");
    printf("[%c] [%c] [%%]\n", 65, 122);
    printf("[%.0d] [%.0d] [%.3d]\n", 0, 7, 7);
    printf("[%d%s%d]\n", 1, "-", 2);
    printf("[%6.3d] [%-6.3d]\n", 5, 5);

    /* What it answers with is the number of characters it wrote. */
    n = printf("");
    printf("wrote %d\n", n);
    n = printf("%5s|", "ab");
    printf("\nwrote %d\n", n);
    n = printf("%ld", -1234567L);
    printf("\nwrote %d\n", n);

    return 0;
}
