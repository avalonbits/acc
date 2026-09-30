/* A value made a _Bool is 0 or 1 by whether it is 0, whatever its low byte:
 * 256 is true. opt-acc's own backend cut an int passed to a _Bool
 * parameter down to its byte, and passed 256 as false. */
static int got(_Bool b) { return b; }

static int pass(int x) { return got(x) + 40; }

static int local(int x)
{
    _Bool b = x;
    _Bool c = x - 256;

    return b + b + c;
}

static int cast(int x) { return (_Bool) x + (_Bool) (x - 256); }

int main(void)
{
    int score = 0;

    score += pass(256) == 41 && pass(0) == 40 && pass(-1) == 41;
    score += local(256) == 2 && local(512) == 3 && local(0) == 1;
    score += cast(256) == 1 && cast(1) == 2 && cast(0) == 1;
    return score == 3 ? 42 : score;
}
