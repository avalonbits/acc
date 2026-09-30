/* ++ and -- of a local kept in memory, and through a pointer, made by
 * opt-acc's own backend: a byte stepped where it is, anything wider
 * through HL; the old value for x++, the new for ++x, and nothing kept
 * where the answer is thrown away. A signed char wraps from 127 to -128,
 * an unsigned one from 255 to 0; a pointer steps by what it points at.
 * Each step is checked on its own, so that two wrong answers cannot make
 * a right total. */
static void look(void *p) { (void) p; }

struct wide { int a, b; };             /* six bytes: a step past four */

static int local_steps(void)
{
    int n = 10;
    signed char c = 127;
    unsigned char u = 255;
    char text[4];
    char *p = text;
    struct wide pair[3];
    struct wide *w = pair;
    int right = 0;

    text[0] = 'a'; text[1] = 'b'; text[2] = 'c'; text[3] = 0;
    look(&n); look(&c); look(&u); look(&p); look(&w);
    right += n++ == 10;
    right += ++n == 12;
    n--;
    right += --n == 10;
    right += c++ == 127;
    right += c == -128;
    right += c-- == -128;
    right += --c == 126;
    right += ++u == 0;
    right += u-- == 0;
    right += u == 255;
    right += *p++ == 'a';
    right += *++p == 'c';
    w++;
    right += (char *) w - (char *) pair == 6;
    ++w;
    right += (char *) w - (char *) pair == 12;

    return right;                       /* 14 */
}

static int indirect_steps(int *ip, signed char *cp, unsigned char *up,
                          char **pp)
{
    int right = 0;

    right += (*ip)++ == 5;
    right += ++*ip == 7;
    --*ip;
    right += *ip == 6;
    right += (*cp)++ == 127;
    right += *cp == -128;
    right += (*cp)-- + 200 == 72;      /* all of the int, not its byte */
    right += --*cp == 126;
    right += ++*up == 0;
    right += (*up)-- == 0;
    right += *up == 255;
    right += *(*pp)++ == 'x';
    right += **pp == 'y';
    right += *--*pp == 'x';

    return right;                       /* 13 */
}

int main(void)
{
    int i = 5;
    signed char c = 127;
    unsigned char u = 255;
    char text[3] = "xy";
    char *p = text;
    int score = 0;

    score += local_steps() == 14;
    score += indirect_steps(&i, &c, &u, &p) == 13;
    score += i == 6 && c == 126 && u == 255 && p == text;
    return score == 3 ? 42 : score;
}
