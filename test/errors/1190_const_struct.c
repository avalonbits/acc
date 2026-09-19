/* expect: 5: error: this is const, so it cannot be changed */
/* Every member of a const struct is const. */
struct point { int id; int x; };
const struct point origin = { 1, 2 };
int main(void) { origin.x = 5; return 0; }
