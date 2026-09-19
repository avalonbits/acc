/* A struct member or array element given a struct of its own type, rather
 * than braces: it is copied in whole. Given anything else, it is the first
 * of the member's values with the braces left out. */
struct point { int x, y; };
struct line { struct point from, to; char tag; };

struct point make(int x, int y) {
    struct point p;

    p.x = x;
    p.y = y;

    return p;
}

int main(void) {
    int r = 0;
    struct point a = { 1, 2 }, b = { 3, 4 };
    struct line l = { a, b, 'l' };
    struct line m = { make(5, 6), 7, 8, 'm' };  /* a call, then elided */
    struct point row[3] = { b, a, make(9, 10) };
    struct line n = { { 11, 12 }, a };

    if (l.from.x == 1 && l.to.y == 4 && l.tag == 'l') r++;
    if (m.from.y == 6 && m.to.x == 7 && m.to.y == 8 && m.tag == 'm') r++;
    if (row[0].x == 3 && row[1].y == 2 && row[2].x == 9) r++;
    if (n.from.x == 11 && n.to.y == 2 && n.tag == 0) r++;
    a.x = 100;
    if (l.from.x == 1) r++;             /* a copy */

    return r + 37;          /* 5 checks */
}
