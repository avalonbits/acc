/* What chibicc's tests include as "test.h", for the conformance suite:
 * chibicc_split.py puts it in place of that file. chibicc's declares the
 * library with its host's types -- `long strlen(char *)`, a long for every
 * size -- which are not this machine's: a long is wider than a size_t
 * here, and a call through those prototypes passes the wrong bytes. So the
 * functions it declares are declared with this machine's types, and that
 * is the only change -- <stdio.h> for printf and its kind, and the rest
 * here rather than from <stdlib.h> and <string.h>, which would bring names
 * test.h does not: unicode.c defines its own wchar_t.
 *
 * ASSERT is chibicc's. assert prints nothing when the answer is right --
 * chibicc's prints every line, which the suite does not read -- and exits
 * with 1 when it is wrong, which the suite does. */
#include <stdarg.h>
#include <stdio.h>

void exit(int status);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, __SIZE_TYPE__ n);
int memcmp(const void *s1, const void *s2, __SIZE_TYPE__ n);
__SIZE_TYPE__ strlen(const char *s);
void *memcpy(void *dest, const void *src, __SIZE_TYPE__ n);
void *memset(void *s, int c, __SIZE_TYPE__ n);

#define ASSERT(x, y) assert(x, y, #y)

static void assert(int expected, int actual, char *code)
{
    if (expected != actual) {
        printf("%s => %d expected but got %d\n", code, expected, actual);
        exit(1);
    }
}
