/* expect: 2:17: error: an array with no size has to come after another member */
struct s { char b[]; };
int main(void) { return sizeof(struct s); }
