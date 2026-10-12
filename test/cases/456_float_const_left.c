/* A float constant on the left of + or *, moved to the right, where it is
 * read from the pool: the same answers. (Not a negative zero or an
 * infinity: agondev's library, which the case is held to, gets neither.) */
static float add40(float x) { return 40.0f + x; }
static float times(float x) { return 2.5f * x; }
static float chain(float x) { return 1.0f + 3.0f * x; }

int main(void)
{
    if (add40(2.0f) != 42.0f || add40(-40.0f) != 0.0f)
        return 1;
    if (times(4.0f) != 10.0f || times(-2.0f) != -5.0f)
        return 2;
    if (chain(2.0f) != 7.0f)
        return 3;
    if (times(0.5f) != 1.25f || add40(0.25f) != 40.25f)
        return 5;

    return 42;
}
