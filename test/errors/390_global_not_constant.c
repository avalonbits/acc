/* expect: 5: error: a global's initial value has to be a constant */
/* Written into the image when it is declared, so it has to be known then.
 */
int a = 1;
int b = a + 1;

int main(void) {
    return b;
}
