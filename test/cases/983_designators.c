/* Designated initialisers: `.member =` and `[index] =`, which say where a
 * value goes instead of leaving it to the order. They may chain -- `[2].x`,
 * `.a.b` -- and the list carries on from the one they named, so a value
 * after `[2] = ...` goes to element 3. Everything they do not name is zero.
 */
struct point { int x, y; };
struct box { struct point at; int w, h; };
union either { int n; char c[4]; };

int ga[6] = { [4] = 5 };
int gmixed[5] = { 1, 2, [4] = 9 };
int gback[4] = { [3] = 4, [1] = 2 };
int gopen[] = { [3] = 1 };
struct point gp = { .y = 7 };
struct box gb = { .at.y = 3, .h = 9 };
struct point grow[3] = { [2].x = 5, [0].y = 6 };
char gs[5] = { [0] = 'a', [4] = 'e' };

int main(void) {
    int a[6] = { [4] = 5 };
    int mixed[5] = { 1, 2, [4] = 9 };
    int back[4] = { [3] = 4, [1] = 2 };
    int open[] = { [3] = 1 };
    int m[2][3] = { [1][2] = 8, [0][1] = 2 };
    struct point p = { .y = 7 };
    struct box b = { .at.y = 3, .h = 9 };
    struct point row[3] = { [2].x = 5, [0].y = 6 };
    union either u = { .c = "ab" };
    struct box after = { .w = 1, 2 };    /* the list goes on from w */

    /* Arrays, at file scope and in a function. */
    if (a[4] != 5 || a[0] != 0 || a[5] != 0)
        return 1;
    if (ga[4] != 5 || ga[0] != 0)
        return 2;
    if (mixed[0] != 1 || mixed[1] != 2 || mixed[2] != 0 || mixed[4] != 9)
        return 3;
    if (gmixed[0] != 1 || gmixed[2] != 0 || gmixed[4] != 9)
        return 4;
    if (back[1] != 2 || back[3] != 4 || back[0] != 0 || back[2] != 0)
        return 5;
    if (gback[1] != 2 || gback[3] != 4 || gback[0] != 0)
        return 6;
    if (sizeof open != 4 * sizeof(int) || open[3] != 1 || open[0] != 0)
        return 7;
    if (sizeof gopen != 4 * sizeof(int) || gopen[3] != 1)
        return 8;

    /* Two dimensions, by a chain of designators. */
    if (m[1][2] != 8 || m[0][1] != 2 || m[0][0] != 0 || m[1][0] != 0)
        return 9;

    /* Members, nested members, and a union's member other than the first. */
    if (p.y != 7 || p.x != 0 || gp.y != 7 || gp.x != 0)
        return 10;
    if (b.at.y != 3 || b.h != 9 || b.at.x != 0 || b.w != 0)
        return 11;
    if (gb.at.y != 3 || gb.h != 9 || gb.w != 0)
        return 12;
    if (u.c[0] != 'a' || u.c[1] != 'b' || u.c[2] != 0)
        return 13;

    /* A member of an element, and the list going on positionally. */
    if (row[2].x != 5 || row[0].y != 6 || row[1].x != 0 || row[0].x != 0)
        return 14;
    if (grow[2].x != 5 || grow[0].y != 6 || grow[1].y != 0)
        return 15;
    if (after.w != 1 || after.h != 2 || after.at.x != 0)
        return 16;
    if (gs[0] != 'a' || gs[4] != 'e' || gs[2] != 0)
        return 17;

    return 42;
}
