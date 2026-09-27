/* expect: 5:13: error: '.' needs a struct or union */
struct point { int x; };
int main(void) {
    struct point p, *q = &p;
    return q.x;
}
