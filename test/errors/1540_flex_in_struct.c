/* expect: 3: error: 'struct s' ends in an array with no size, so it cannot be a member */
struct s { int n; char b[]; };
struct t { struct s s; int x; };
int main(void) { return sizeof(struct t); }
