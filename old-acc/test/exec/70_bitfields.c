/* expect:
set 5 21 300 99
store 301
rmw 302 303
pack 200 301
sizes 1 2 3 2 3
signed -3 -100 5
enum 0 40 300 3
*/
int printf(const char *, ...);

/* The container here is three bytes and c straddles a byte boundary at bit 8,
   which is the shape that broke: the value was cast into a two-byte container
   through a shift pair computed against a four-byte register. */
struct F { unsigned a : 3; unsigned b : 5; unsigned c : 9; unsigned d : 7; };
struct A { unsigned a : 3; };
struct B { unsigned a : 8; unsigned b : 8; };
struct C { unsigned a : 8; unsigned b : 9; };
struct D { unsigned a : 9; };
struct S { signed a : 3; signed b : 9; signed c : 4; };
enum E { E0, E1 = 40, E2 = 300 };

int main(void) {
    struct F f;
    struct C z;
    struct S s;
    enum E e = E2;

    f.a = 5; f.b = 21; f.c = 300; f.d = 99;
    printf("set %d %d %d %d\r\n", f.a, f.b, f.c, f.d);
    f.c = 301;
    printf("store %d\r\n", f.c);
    f.c = f.c + 1;
    { int first = f.c; f.c += 1; printf("rmw %d %d\r\n", first, f.c); }

    z.a = 200; z.b = 301;
    printf("pack %d %d\r\n", z.a, z.b);

    printf("sizes %d %d %d %d %d\r\n", (int)sizeof(struct A), (int)sizeof(struct B),
           (int)sizeof(struct C), (int)sizeof(struct D), (int)sizeof(struct F));

    /* sign extension out of a field narrower than the container */
    s.a = -3; s.b = -100; s.c = 5;
    printf("signed %d %d %d\r\n", s.a, s.b, s.c);

    printf("enum %d %d %d %d\r\n", E0, E1, (int) e, (int) sizeof(enum E));
    return 0;
}
