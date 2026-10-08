/* Ints and longs worked on and stored to a byte, which opt-acc's machine-
 * level backend makes on the low bytes alone: sums that carry out of the
 * byte, differences that borrow, ands, ors, xors and small shifts, of
 * longs and of ints -- and one answer stored to a byte and read whole as
 * well, which must not be cut. */
typedef struct {
    unsigned char opcode, low, mix;
    int whole;
} out_t;

typedef struct {
    unsigned char reg;
    long immediate;
    int count;
} op_t;

static __attribute__((noinline)) void make(out_t *out, const op_t *op)
{
    out->opcode = 0x40;
    out->opcode |= (op->reg << 3);
    out->opcode |= op->immediate & 7;
    out->low = op->immediate + op->count;           /* carries past the byte */
    out->mix = (op->immediate - 3) ^ (op->count << 2);
    out->whole = op->count + 0x123;
    out->mix += (unsigned char) out->whole;
}

static __attribute__((noinline)) int both(const op_t *op, unsigned char *byte)
{
    int sum = op->count + op->reg;

    *byte = sum;                /* its byte stored, */

    return sum;                 /* and all of it answered */
}

int main(void)
{
    op_t op = { 5, 0x12345L, 0x1f0 };
    out_t out;
    unsigned char b;
    int ok = 0;

    make(&out, &op);
    ok += out.opcode == (0x40 | (5 << 3) | 5);
    ok += out.low == (unsigned char) (0x12345L + 0x1f0);
    ok += out.mix == (unsigned char) ((unsigned char) ((0x12345L - 3) ^ (0x1f0 << 2))
                                      + (unsigned char) (0x1f0 + 0x123));
    ok += out.whole == 0x1f0 + 0x123;
    ok += both(&op, &b) == 0x1f5 && b == 0xf5;

    return ok == 5 ? 42 : ok;
}
