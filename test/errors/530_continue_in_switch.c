/* expect: 6:9: error: 'continue' is not inside a loop */
/* A switch is not a loop, so there is nothing for a continue to go on with. */
int main(void) {
    switch (1) {
    case 1:
        continue;
    }
    return 0;
}
