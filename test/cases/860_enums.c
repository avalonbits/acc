/* Enums: constants counted from zero or from where one is given, in
 * expressions, as case labels and as array sizes; and a variable of one is
 * unsigned when none of them is negative, as agondev has it. */
enum colour { RED, GREEN, BLUE, COUNT };
enum { BIG = 1000, BIGGER, NEG = -5, AFTER };

int table[COUNT];

int name(enum colour c) {
    switch (c) {
    case RED:   return 1;
    case GREEN: return 2;
    case BLUE:  return 4;
    }

    return 0;
}

int main(void) {
    int r = 0;
    enum colour c = BLUE;
    enum { LOCAL = 7 } local = LOCAL;

    if (RED == 0 && GREEN == 1 && BLUE == 2) r++;
    if (BIGGER == 1001 && AFTER == -4) r++;
    if (name(c) + name(GREEN) == 6) r++;
    if (sizeof table == 3 * sizeof(int)) r++;
    if (local == 7) r++;
    c = RED;
    c--;
    if (c > 0) r++;         /* unsigned: RED - 1 is large */
    if (NEG < 0) r++;       /* the constants are int */

    return r + 35;          /* 7 checks */
}
