/* #ifdef, #ifndef, #else and #endif, against agondev compiling the same
 * program.
 *
 * A conditional decides which text the compiler ever sees, so what is worth
 * asking another compiler is that both saw the same text: the branches here
 * define different functions and different constants, and only one of each
 * can have been read. What is refused, and what a skipped group is allowed
 * to contain, are in test/macro.sh.
 */
#define USE_DOUBLE

#ifdef USE_DOUBLE
int scale(int v) { return v * 2; }
#else
int scale(int v) { return v * 3; }
#endif

#ifndef NOT_DEFINED
enum { BASE = 20 };
#else
enum { BASE = 1 };
#endif

#ifdef NOT_DEFINED
this text is never compiled and need not be C
#endif

int main(void) {
    int total = scale(BASE);        /* 40 */

#ifdef USE_DOUBLE
    total = total + 2;
#else
    total = total + 1000;
#endif

    return total;
}
