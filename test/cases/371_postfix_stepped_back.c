/* x++ and x-- through a pointer, and of globals and members, which acc
 * steps where they are and then steps back for the value they had: that
 * value has to come out right where the store wrapped -- an unsigned char
 * at 255, a signed one at 127, a short past its top -- and for a pointer,
 * which steps by what it points at. A _Bool is stepped as it always was:
 * its new value does not say what it had been. */
unsigned char uc = 255;
signed char sc = 127;
short sh = 32767;
int gi = 41;
_Bool gb = 0;
long gl = 7;
struct rec { int n; char c; int *p; } r = { 5, -128, 0 };
int arr[3] = { 10, 20, 30 };

int main(void)
{
    int ok = 0, *p = arr;
    struct rec *rp = &r;

    ok += uc++ == 255 && uc == 0;
    ok += sc++ == 127 && sc == -128;
    ok += sh++ == 32767 && sh == -32768;
    ok += gi-- == 41 && gi == 40;
    ok += gb++ == 0 && gb == 1 && gb++ == 1 && gb == 1;
    ok += gl++ == 7 && gl == 8;
    ok += rp->n++ == 5 && r.n == 6;
    ok += rp->c-- == -128 && r.c == 127;
    r.p = p;
    ok += *rp->p++ == 10 && *r.p == 20;
    ok += (*p)++ == 10 && arr[0] == 11;
    gi++;
    uc--;
    ok += gi == 41 && uc == 255;

    return ok == 11 ? 42 : ok;
}
