/* memset and memcpy at the counts their tests for none decide on, made in
 * the routines rather than called: a fill or a copy of none, a fill of one
 * (whose first byte is written by hand, leaving none to copy along), of
 * two, and of a struct; and counts known only as they run. */
#include <string.h>

struct token { char *start, *next; char terminator; };

static unsigned char buf[16];
static int n_zero, n_one;

static int filled(int from, int to, int byte)
{
    int at;

    for (at = from; at != to; at++)
        if (buf[at] != byte)
            return 0;

    return 1;
}

int main(void)
{
    struct token t = { buf, buf + 1, 'x' }, u;
    int ok = 0;

    memset(buf, 0xee, sizeof buf);
    ok += memset(buf + 1, 0, 1) == buf + 1 && buf[0] == 0xee && buf[1] == 0 && buf[2] == 0xee;
    ok += memset(buf + 3, 7, 2) == buf + 3 && filled(3, 5, 7) && buf[5] == 0xee;
    ok += memset(buf + 5, 9, 0) == buf + 5 && buf[5] == 0xee;
    memset(&t, 0, sizeof t);
    ok += t.start == 0 && t.next == 0 && t.terminator == 0;
    t.start = (char *) buf;
    t.terminator = 'q';
    ok += memcpy(&u, &t, sizeof u) == &u && u.start == (char *) buf && u.terminator == 'q';
    ok += memcpy(buf + 8, buf + 3, 1) == buf + 8 && buf[8] == 7 && buf[9] == 0xee;
    ok += memcpy(buf + 9, buf, 0) == buf + 9 && buf[9] == 0xee;
    n_zero = 0;
    n_one = 3;
    ok += memset(buf + 10, 4, n_one) == buf + 10 && filled(10, 13, 4) && buf[13] == 0xee;
    ok += memcpy(buf + 13, buf, n_zero) == buf + 13 && buf[13] == 0xee;

    return ok == 9 ? 42 : ok;
}
