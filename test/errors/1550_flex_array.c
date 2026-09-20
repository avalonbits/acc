/* expect: 3: error: 'struct s' ends in an array with no size, so there is no saying where the next element would start */
struct s { int n; char b[]; };
struct s a[2];
int main(void) { return a[0].n; }
