/* Duff's device: case labels inside a do-while inside a switch, jumped into
 * from outside the loop. A switch in acc notes where each case is as it
 * compiles the body and tests them all afterwards, so a case may be
 * anywhere in the body -- inside a loop included.
 */
int copy(char *to, char *from, int count) {
    int n = (count + 7) / 8;

    switch (count % 8) {
    case 0: do { *to++ = *from++;
    case 7:      *to++ = *from++;
    case 6:      *to++ = *from++;
    case 5:      *to++ = *from++;
    case 4:      *to++ = *from++;
    case 3:      *to++ = *from++;
    case 2:      *to++ = *from++;
    case 1:      *to++ = *from++;
            } while (--n > 0);
    }

    return count;
}

/* Copies exactly count bytes, for every count from 1 to 20, and not one more. */
int check(int count) {
    char from[24];
    char to[24];
    int i;

    for (i = 0; i < 24; i++) {
        from[i] = i + 1;
        to[i] = 0;
    }
    copy(to, from, count);
    for (i = 0; i < 24; i++)
        if (to[i] != (i < count ? i + 1 : 0))
            return 0;

    return 1;
}

int main(void) {
    int good = 0;

    for (int count = 1; count <= 20; count++)
        good += check(count);

    /* 20 */
    return good + 22;
}
