/* Block statics whose initial values hold addresses, in functions opt-acc
 * makes from their SSA form: their bytes laid down where the code starts,
 * and each address moved with what it points at -- a string of the same
 * initial value, another static of the function, a global, a static that
 * starts at zero, a function defined before and one defined after, and
 * variables defined further down the file. */
#include <string.h>

int seed = 7;
static int later(int x);
extern int late_var;
extern char late_buf[];

static int twice(int x)
{
    return x * 2;
}

struct entry { const char *name; int (*fn)(int); };

static const char *word(int k)
{
    static const char *words[] = { "alpha", "beta", "gamma" };

    return words[k];
}

static int pointers(int k)
{
    static int table[4] = { 10, 20, 30, 40 };
    static int *third = &table[2];
    static int *global = &seed;
    static int count;
    static int *counted = &count;

    *counted += k;
    (*third)++;

    return *third + *global + count;
}

static int dispatch(int k, int x)
{
    static const struct entry entries[] = { { "twice", twice }, { "later", later } };

    return entries[k].fn(x) + (int) strlen(entries[k].name);
}

static int later(int x)
{
    return x + 100;
}

static int externs(void)
{
    static int *var = &late_var;
    static char *buf = late_buf + 2;

    return *var + *buf;
}

int late_var = 5;
char late_buf[4] = { 1, 2, 30, 4 };

int main(void)
{
    int ok = 0;

    ok += !strcmp(word(0), "alpha") && !strcmp(word(2), "gamma");
    ok += pointers(1) == 31 + 7 + 1 && pointers(2) == 32 + 7 + 3;
    ok += dispatch(0, 4) == 8 + 5 && dispatch(1, 4) == 104 + 5;
    ok += externs() == 35;

    return ok == 4 ? 42 : ok;
}
