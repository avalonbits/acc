/* expect: 27:5: error: this switch already has a case for 3 */
/* A case the same as one before it, past the sixteen a switch compares
 * each with: found in the table of them -- with a switch inside, its own
 * cases the same values, which are not this switch's. */
int main(void) {
    int x = 1;

    switch (x) {
    case 0: x++;
    case 1: x++;
    case 2: x++;
    case 3: x++;
    case 4: x++;
    case 5: switch (x) { case 3: case 4: x++; }
    case 6: x++;
    case 7: x++;
    case 8: x++;
    case 9: x++;
    case 10: x++;
    case 11: x++;
    case 12: x++;
    case 13: x++;
    case 14: x++;
    case 15: x++;
    case 16: x++;
    case 17: x++;
    case 3:
        return 1;
    }
    return 0;
}
