/* Floats worked out through the runtime's routines and assigned to a
 * variable, which acc works out in the variable itself -- one operator
 * and chains of them -- against the same expressions worked out and
 * passed on without being stored, which are built in scratch as before.
 * Bit for bit, over values that round. acc against itself, as the host's
 * float routines are not the Agon's. */
static unsigned long bits(float f)
{
    union { float f; unsigned long u; } b;

    b.f = f;

    return b.u;
}

static int wrong;

static void same(float stored, float passed)
{
    if (bits(stored) != bits(passed))
        wrong++;
}

static void floats(float f, float e)
{
    float g = f, k, m;

    g = g * 1.5f + 0.25f;
    same(g, f * 1.5f + 0.25f);
    k = g * g - f;
    same(k, g * g - f);
    m = k / 3.0f + g;
    same(m, k / 3.0f + g);
    k = g;
    g = ((g * 3.0f + e) * 7.0f + 0.1f) / 13.0f;
    same(g, ((k * 3.0f + e) * 7.0f + 0.1f) / 13.0f);
    m = e;
    e = e - g;
    same(e, m - g);
}

int main(void)
{
    static const float v[] = { 0.0f, 1.0f, -2.5f, 0.1f, 1e10f, 3.3f };
    int i;

    for (i = 0; i < 6; i++)
        floats(v[i], v[(i + 1) % 6]);

    return wrong == 0 ? 42 : 1;
}
