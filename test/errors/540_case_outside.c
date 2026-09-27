/* expect: 4:5: error: 'case' is not inside a switch */
/* A case label belongs to a switch. */
int main(void) {
    case 1:
    return 0;
}
