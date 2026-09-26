/* expect: 4:9: error: an array declared with [] needs initial values to say how long it is */
/* [] takes its length from the initialiser, and there is none. */
int main(void) {
    int a[];
    return 0;
}
