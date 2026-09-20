/* expect: 4: error: an array whose length is worked out as it runs cannot have an initialiser */
int main(void) {
    int n = 2;
    int a[n] = { 1, 2 };
    return a[0];
}
