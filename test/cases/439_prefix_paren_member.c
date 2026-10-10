/* ++ and -- of a name in parentheses with members and subscripts after
 * the `)`, as a macro writes round its argument -- Berry's
 * enter_recursion is `++(parser)->depth > MAX`: the member is stepped, and
 * the answer is its new value. A name alone in them is stepped as ever. */
struct parser { int depth; char marks[3]; };

#define ENTER(p) (++(p)->depth > 2)
#define LEAVE(p) (--(p)->depth)

int main(void)
{
    struct parser state = { 0, { 5, 6, 7 } }, *parser = &state;
    int deep = 0, n = 4, k;

    for (k = 0; k < 4; k++)
        deep += ENTER(parser);
    if (state.depth != 4 || deep != 2)
        return 1;
    if (LEAVE(parser) != 3 || state.depth != 3)
        return 2;
    if (++(parser)->marks[1] != 7 || --(state).marks[2] != 6 || state.marks[1] != 7)
        return 3;
    if (++(n) != 5 || --(n) != 4)
        return 4;

    return 42;
}
