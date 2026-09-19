/* expect: 5: error: '.' needs a struct or union */
struct point { int x; };
int main(void) {
    struct point p, *q = &p;
    return q.x;
}
