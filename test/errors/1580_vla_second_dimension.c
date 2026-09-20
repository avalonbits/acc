/* expect: 4: error: only an array's first dimension may be worked out as it runs */
int main(void) {
    int n = 2, m = 3;
    int a[n][m];
    return a[0][0];
}
