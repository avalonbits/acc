/* A _Bool written through a pointer is 0 or 1 whatever is written to it:
 * opt-acc's own backend writes a value that may be anything as its test
 * against 0, and one that is 0 or 1 already -- a constant, a comparison,
 * a _Bool read -- as it is, BC's or DE's. 256 and 0x10000 have a low byte of 0, which a
 * store of the byte would have written as false. */
struct flags {
    int count;
    _Bool on;
    _Bool off;
};

static int set_from(struct flags *f, int value)
{
    f->on = value;
    f->off = !f->on;

    return f->on;
}

static void set_many(struct flags *f, int value, int limit)
{
    int left = limit, right = value;

    f->count = left + right;
    f->on = right;
    f->off = left < right;
}

static int answer_of(struct flags *f, int value)
{
    return f->on = value;
}

/* `step` is used most in the loop, so it has a register: the test of it
 * is of BC. 0, then 256 and 512, whose low bytes are 0. */
static int spread(_Bool *flag)
{
    int step = 0;

    while (step != 768) {
        flag[3] = step - 256;
        *flag++ = step;
        step += 256;
    }

    return flag[-1] + flag[-2] + (flag[0] << 2);
}

static int read_back(struct flags *f)
{
    if (f->on && !f->off)
        return 2;

    return f->on + f->off;
}

int main(void)
{
    struct flags f;
    int score = 0;

    score += set_from(&f, 256) == 1 && f.on == 1 && f.off == 0;
    score += set_from(&f, 0x10000) == 1 && f.off == 0;
    score += set_from(&f, -1) == 1;
    score += set_from(&f, 0) == 0 && f.off == 1;
    set_many(&f, 512, 3);
    score += f.on == 1 && f.off == 1 && f.count == 515;
    set_many(&f, 0, 3);
    score += f.on == 0 && f.off == 0;
    score += answer_of(&f, 768) == 1;
    f.on = 2;
    score += read_back(&f) == 2;
    {
        _Bool many[6];

        score += spread(many) == 6 && many[0] == 0 && many[4] == 0;
    }
    return score == 9 ? 42 : score;
}
