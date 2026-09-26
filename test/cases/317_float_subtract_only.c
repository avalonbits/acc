/* Floating-point subtraction and nothing else of floating point. The
 * runtime's fsub turns the right side's sign over and falls into fadd, so
 * a program that only subtracts still needs fadd laid down straight after
 * it: acc carries the runtime by the groups a program reaches, and the two
 * are one group because the first falls into the second. */
static float sub(float a, float b)
{
    return a - b;
}

int main(void)
{
    float x = sub(50.0f, 8.0f), y = sub(-1.5f, -2.0f), z = sub(0.25f, 0.25f);

    return x == 42.0f && y == 0.5f && z == 0.0f ? 42 : 1;
}
