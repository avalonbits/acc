/* goto and labels: forwards and backwards, out of nested loops, into the
 * middle of one, a loop made of nothing but a label and a goto, labels that
 * share names with variables and with labels in other functions, and one
 * label straight after another.
 */
int count_to(int n) {
    int i = 0;

again:
    if (i < n) {
        i++;
        goto again;
    }

    return i;
}

/* The classic use: leaving two loops at once. */
int find(int target) {
    int found = -1;
    int row;
    int col;

    for (row = 0; row < 5; row++)
        for (col = 0; col < 5; col++)
            if (row * 5 + col == target) {
                found = row * 10 + col;
                goto done;
            }

done:
    return found;
}

/* The same label names as above, in another function: labels belong to the
 * function they are in. And a label named like a variable. */
int again(int n) {
    int done = 0;

    goto done;
again:
    done = done + 100;
done:
    done = done + n;
    if (done < 50)
        goto again;

    return done;
}

/* Into the middle of a loop, skipping its first test and the first half of
 * the body, and one label directly after another. */
int middle(int n) {
    int total = 0;
    int i = 0;

    goto inside;
    while (i < n) {
        total += 100;
inside:
second:
        total += 1;
        i++;
    }

    return total;
}

int main(void) {
    int r = 0;

    if (count_to(7) == 7 && count_to(0) == 0) r = r + 1;
    if (find(13) == 23 && find(99) == -1) r = r + 1;
    if (again(5) == 110 && again(60) == 60) r = r + 1;
    if (middle(3) == 203) r = r + 1;

    /* 4 */
    return r + 38;
}
