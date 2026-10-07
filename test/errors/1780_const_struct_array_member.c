/* expect: 4:52: error: this is const, so it cannot be changed */
/* A const struct's array of ints is const; only pointers are not. */
struct table { int cells[4]; struct table *next; };
void clear(const struct table *t) { t->cells[1] = 0; t->next->cells[0] = 0; }
