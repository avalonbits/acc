/* Switches whose cases are joined by the case before falling into them,
 * with a pointer stepped and a count changed on the way -- a phi in the
 * block a case jumps to, which opt-acc's machine-level backend copies
 * into on the case's own edge: nested three deep, as ez80asm's
 * parse_operand has them, and in a loop. */
typedef struct {
    unsigned char reg, mode, idx;
} operand;

static int errors;

static void parse(const char *s, operand *op)
{
    const char *p = s;

    op->reg = op->mode = op->idx = 0;
    if (*p == '(') {
        op->mode = 1;
        p++;
    }
    switch (*p++) {
    case 'a': case 'A':
        switch (*p++) {
        case 0:
            op->reg = 1;
            return;
        case 'f': case 'F':
            switch (*p++) {
            case 0: case '\'':
                op->reg = 2;
                return;
            }
            break;
        }
        break;
    case 'i':
        p++;                    /* "ix" written "iix": one more to skip */
        /* fall through */
    case 'I':
        switch (*p++) {
        case 'x':
            op->reg = 5;
            op->idx = (unsigned char) *p;
            return;
        case 'y':
            op->reg = 6;
            op->idx = (unsigned char) *p;
            return;
        }
        break;
    }
    errors += *p ? *p : 100;
}

static int count(const char *s)
{
    int n = 0;

    for (;;) {
        switch (*s++) {
        case 0:
            return n;
        case 'a':
            n += 4;
            s++;
            /* fall through */
        case 'b':
            n += *s == 'z';
            break;
        default:
            n--;
        }
    }
}

int main(void)
{
    operand op;
    int ok = 0;

    parse("a", &op);
    ok += op.reg == 1 && op.mode == 0;
    parse("(af'", &op);
    ok += op.reg == 2 && op.mode == 1;
    parse("iix7", &op);
    ok += op.reg == 5 && op.idx == '7';
    parse("Iy+", &op);
    ok += op.reg == 6 && op.idx == '+';
    parse("ag", &op);
    parse("q", &op);
    ok += errors == 100 + 100 && op.reg == 0;
    ok += count("axzbzq") == 4 + 1 - 1 + 1 - 1 - 1;

    return ok == 6 ? 42 : ok;
}
