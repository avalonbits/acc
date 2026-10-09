/* A parenthesis that opens on a star and goes on past what the star reads
 * with a comma: `(*p, 0)`, `(*p = 3, *p)`. Refused at the comma once. */

static int calls;

static int *touch(int *p)
{
    calls++;

    return p;
}

int main(void)
{
    int a = 5, b = 7, *p = &a, **pp = &p, x, ok = 0;

    ok += (*p, 0) == 0;
    ok += (*p + 1, 9) == 9;
    x = (*p, 2);
    ok += x == 2;
    ok += (**pp, 4) == 4;
    if ((*p, 1))
        ok++;
    (*p, b = 11);
    ok += b == 11;
    ok += (*p = 3, *p) == 3 && a == 3;
    ok += (*p ? 1 : 2, 6) == 6;
    ok += (*touch(p), *touch(&b), 8) == 8 && calls == 2;
    ok += ((*p)++, a) == 4;

    return ok == 10 ? 42 : ok;
}
