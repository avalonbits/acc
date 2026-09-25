/* gcc's spellings of restrict, as a header written for it declares
 * memcpy: the same qualifier, not two parameters of the same name. */
typedef __SIZE_TYPE__ size_t;
void *memcpy(void *__restrict, const void *__restrict, size_t);

static int first(int *__restrict__ p, int *__restrict q)
{
    *q = 2;

    return *p;
}

int main(void)
{
    int a = 40, b = 0, c;

    memcpy(&c, &a, sizeof a);

    return first(&c, &b) + b;
}
