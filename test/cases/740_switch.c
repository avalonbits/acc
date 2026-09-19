/* switch: the promoted type of the value, fall-through, default anywhere or
 * nowhere, constant expressions as cases, and one switch inside another.
 */
int classify(int x) {
    switch (x) {
    case 1:
        return 10;
    case 2:
    case 3:
        x = x + 100;
        break;
    default:
        x = -1;
    }

    return x;
}

/* default in the middle: tried last whatever its place, and falls into the
 * case after it. */
int middle(int x) {
    int n = 0;

    switch (x) {
    case 5:
        n += 5;
    default:
        n += 1;
    case 9:
        n += 9;
    }

    return n;
}

int by_char(char c) {
    switch (c) {
    case -1:  return 1;
    case 65:  return 2;
    case 200: return 3;         /* a char never holds 200: never taken */
    }

    return 0;
}

int by_long(long v) {
    switch (v) {
    case 100000:      return 1;
    case -2000000000: return 2;
    case 16777217:    return 3;  /* differs from 1 only above 24 bits */
    case 1:           return 4;
    }

    return 0;
}

int by_unsigned(unsigned int u) {
    switch (u) {
    case 0xffffff: return 1;
    case 2 * 3:    return 2;
    case 1 << 4:   return 3;
    }

    return 0;
}

/* A sign binds to its operand only: -3 + 2 is -1, not -5. */
int signed_sum(int x) {
    switch (x) {
    case -3 + 2: return 1;
    case -5:     return 2;
    }

    return 0;
}

int nested(int a, int b) {
    switch (a) {
    case 0:
        switch (b) {
        case 0:  return 1;
        default: break;
        }
        return 2;
    case 1:
        return 3;
    }

    return 4;
}

int main(void) {
    int r = 0;
    int i = 0;
    int n = 0;

    if (classify(1) == 10 && classify(2) == 102 && classify(3) == 103
        && classify(7) == -1) r = r + 1;
    if (middle(5) == 15 && middle(9) == 9 && middle(0) == 10) r = r + 1;
    if (by_char(-1) == 1 && by_char(65) == 2 && by_char(-56) == 0) r = r + 1;
    if (by_long(100000) == 1 && by_long(-2000000000) == 2) r = r + 1;
    if (by_long(16777217) == 3 && by_long(1) == 4 && by_long(2) == 0) r = r + 1;
    if (by_unsigned(6) == 2 && by_unsigned(0xffffff) == 1
        && by_unsigned(16) == 3) r = r + 1;
    if (signed_sum(-1) == 1 && signed_sum(-5) == 2) r = r + 1;
    if (nested(0, 0) == 1 && nested(0, 5) == 2 && nested(1, 0) == 3
        && nested(2, 0) == 4) r = r + 1;

    /* The value is worked out once. */
    switch (i++) {
    case 0:
        n = 1;
        break;
    case 1:
        n = 2;
        break;
    }
    if (n == 1 && i == 1) r = r + 1;

    /* No case matches and there is no default: nothing runs. */
    n = 0;
    switch (i) {
    case 7:
        n = 1;
    }
    if (n == 0) r = r + 1;

    /* 10 */
    return r + 32;
}
