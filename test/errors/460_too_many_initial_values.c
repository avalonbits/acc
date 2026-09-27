/* expect: 3:19: error: more initial values than the array has elements */
/* A global one, counted as the elements are read. */
int a[2] = {1, 2, 3};

int main(void) {
    return a[0];
}
