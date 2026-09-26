/* expect: 7:5: error: this switch already has a default */
/* One default to a switch. */
int main(void) {
    switch (1) {
    default:
        break;
    default:
        break;
    }
    return 0;
}
