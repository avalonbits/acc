/* A compound literal is an object (C99 6.5.2.5p4), and so can be assigned
 * to and stepped: `(int){3} = 5` is 5, as any assignment is its value.
 * From chibicc's complit.c. */
struct pair { int a, b; };

int main(void)
{
    int r, s, t;

    (int){3} = 5;
    r = (int){3} = 5;
    s = (int){2}++;
    t = (struct pair){1, 2}.b = 30;
    t += (struct pair){4, 5}.a;

    return r + s + t + (int){1};
}
