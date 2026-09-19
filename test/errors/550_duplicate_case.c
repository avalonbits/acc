/* expect: 8: error: this switch already has a case for 16777215 */
/* Two cases that are the same once converted to the switch's type: -1 as an
 * unsigned int is 0xffffff. */
int main(void) {
    unsigned int u = 1;
    switch (u) {
    case 0xffffff:
    case -1:
        return 1;
    }
    return 0;
}
