/* Text: words made up from a seed into a 6 KB buffer, split on spaces,
 * hashed, and counted in an open-addressed table compared with a strcmp
 * of its own -- char pointers, byte loops and short functions called
 * often.
 *
 * expect: 2228348029
 *
 * Checked against a model of the program in Python, with an int of 24 bits
 * and a long of 32: agondev's builds give 976538813. Its -Oz reads the byte
 * after the one `*p++` names in hash() -- 't' for the 'e' of "etao" -- and
 * its -O2 comes to the same wrong table. */
#include "perf.h"

#define TEXT 6144
#define SLOTS 512

static char text[TEXT];
static const char *slot_word[SLOTS];
static unsigned char slot_len[SLOTS];
static unsigned short slot_count[SLOTS];

static int same(const char *a, const char *b, int n)
{
    while (n-- > 0)
        if (*a++ != *b++)
            return 0;

    return 1;
}

static unsigned hash(const char *p, int n)
{
    unsigned h = 5381;

    while (n-- > 0)
        h = (h << 5) + h + (unsigned char) *p++;

    return h;
}

static void count(const char *word, int n)
{
    unsigned at = hash(word, n) & (SLOTS - 1);

    while (slot_word[at]) {
        if (slot_len[at] == n && same(slot_word[at], word, n)) {
            slot_count[at]++;
            return;
        }
        at = (at + 1) & (SLOTS - 1);
    }
    slot_word[at] = word;
    slot_len[at] = (unsigned char) n;
    slot_count[at] = 1;
}

int main(void)
{
    static const char letters[] = "etaoinshrdlu";
    unsigned long state = perf_seed, check = 0;
    int at = 0;

    while (at < TEXT - 8) {
        int n;

        state = state * 1103515245UL + 12345UL;
        n = 1 + (int) (state >> 16) % 3;   /* 258 words at most, in 512 slots */
        for (int i = 0; i < n; i++) {
            state = state * 1103515245UL + 12345UL;
            text[at++] = letters[(state >> 16) % 6];
        }
        text[at++] = ' ';
    }
    text[at] = 0;

    perf_start();
    for (const char *p = text; *p;) {
        const char *start;

        while (*p == ' ')
            p++;
        start = p;
        while (*p && *p != ' ')
            p++;
        if (p > start)
            count(start, (int) (p - start));
    }
    for (int i = 0; i < SLOTS; i++)
        if (slot_word[i])
            check = check * 33 + slot_count[i] * slot_len[i];
    perf_stop();

    perf_check(check);

    return 0;
}
