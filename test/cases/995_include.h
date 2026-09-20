/* The header half of 995_include.c.
 *
 * Not a case of its own: run.sh compiles test/cases/*.c, so a .h beside
 * them is picked up by the one that includes it and by nothing else.
 */
struct point {
    int x, y;
};

enum { SCALE = 3 };

int scaled(int v);
