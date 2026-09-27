/* expect: 2:24: error: an array with no size has to be the last member */
struct s { int n; char b[]; int m; };
int main(void) { return sizeof(struct s); }
