/* Not valid, on purpose: acc should say so and say where.
 *
 * It used to be multiplication. That works now, so this asks for a pointer
 * instead -- the point of the file is to show what a refusal looks like, not
 * which feature happens to be missing this week. */
int main(void) {
    int n = 6;
    return *n;
}
