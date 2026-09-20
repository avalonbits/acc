/* #include, against agondev compiling the same two files.
 *
 * What is checked here is that a declaration made in one file is usable in
 * the other -- a struct, an enum constant, a prototype -- and that the
 * program is the same one however it was split up. Where a header is looked
 * for and what is said when it is missing are in test/include.sh, which
 * needs no second compiler.
 */
#include "995_include.h"

int scaled(int v) {
    return v * SCALE;
}

int sum_point(struct point p) {
    return p.x + p.y;
}

int main(void) {
    struct point p;

    p.x = scaled(4);        /* 12 */
    p.y = scaled(6);        /* 18 */

    return sum_point(p) + SCALE * 4;
}
