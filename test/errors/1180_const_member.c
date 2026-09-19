/* expect: 5: error: this is const, so it cannot be changed */
struct point { const int id; int x; };
int main(void) {
    struct point p = { 1, 2 };
    p.id = 5;
    return 0;
}
