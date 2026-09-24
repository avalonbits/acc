/* A comment is one space by the time the preprocessor reads a directive
 * (C99 5.1.1.2), so one after an #include's file name, an #undef's name or
 * a #line's number leaves nothing after them. acc refused all three, and
 * c-testsuite's 00201 writes `#include <stdio.h>	// printf()`. */
#include "995_include.h" // the header
#define GONE 1
#undef GONE /* no longer */
#line 100 /* a new count */

int main(void)
{
    int r = 0;

#ifndef GONE
    r++;
#endif
    if (__LINE__ == 108)
        r++;

    return r + 40;              /* 2 checks */
}
