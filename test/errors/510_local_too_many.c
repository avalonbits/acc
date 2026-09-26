/* expect: 4:23: error: more initial values than the array has elements */
/* A local one, refused at the element with nowhere to go. */
int main(void) {
    int a[2] = {1, 2, 3};
    return a[0];
}
