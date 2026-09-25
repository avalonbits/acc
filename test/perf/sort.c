/* Sorting: a quicksort over 1,500 ints, recursive, with an insertion sort
 * for the short runs, and the same numbers sorted again after a shuffle.
 * Comparisons, swaps through indices, recursion and loops over an array. */
#include "perf.h"

#define COUNT 1500

static int values[COUNT];

static void insertion(int *a, int n)
{
    for (int i = 1; i < n; i++) {
        int v = a[i], j = i - 1;

        while (j >= 0 && a[j] > v) {
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = v;
    }
}

static void quick(int *a, int n)
{
    while (n > 12) {
        int pivot = a[n / 2], i = 0, j = n - 1;

        while (i <= j) {
            while (a[i] < pivot)
                i++;
            while (a[j] > pivot)
                j--;
            if (i <= j) {
                int t = a[i];

                a[i] = a[j];
                a[j] = t;
                i++;
                j--;
            }
        }
        if (j + 1 < n - i) {
            quick(a, j + 1);
            a += i;
            n -= i;
        } else {
            quick(a + i, n - i);
            n = j + 1;
        }
    }
    insertion(a, n);
}

int main(void)
{
    unsigned long state = perf_seed, check = 0;

    for (int i = 0; i < COUNT; i++) {
        state = state * 1103515245UL + 12345UL;
        values[i] = (int) ((state >> 8) & 0x7fff) - 16384;
    }

    perf_start();
    quick(values, COUNT);
    for (int i = COUNT - 1; i > 0; i--) {
        int j = (int) ((unsigned long) i * 7919UL % (unsigned long) (i + 1));
        int t = values[i];

        values[i] = values[j];
        values[j] = t;
    }
    quick(values, COUNT);
    for (int i = 0; i < COUNT; i++)
        check = check * 31UL + (unsigned long) values[i];
    perf_stop();

    for (int i = 1; i < COUNT; i++)
        if (values[i - 1] > values[i])
            check = 0;
    perf_check(check);

    return 0;
}
