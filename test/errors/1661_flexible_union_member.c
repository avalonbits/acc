/* expect: 5:29: error: 'union holder' ends in an array with no size, so it cannot be a member */
/* A union that holds such a struct is held to the struct's rule. */
struct packet { int size; unsigned char data[]; };
union holder { struct packet packet; int word; };
struct outer { union holder held; int after; };
int main(void) { return 0; }
