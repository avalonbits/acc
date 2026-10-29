#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int sign(int v) { return v < 0 ? -1 : v > 0; }
static void show(const char *what, const char *s) { printf("%s [%s]\n", what, s ? s : "(null)"); }
static void at(const char *what, const char *base, const char *p) { printf("%s %ld\n", what, p ? (long) (p - base) : -1L); }

int main(void)
{
    static const char *words[] = { "", "a", "ab", "abc", "abd", "ABC", "b", "\x80", "\xff", "a\x80" };
    char b[64], c[64];
    const char *h = "hello, world; hello again";
    static const char hi[] = "a\xe9z";
    unsigned i, j;
    char *t, *d;

    for (i = 0; i < 10; i++) {
        printf("cmp %u:", i);
        for (j = 0; j < 10; j++)
            printf(" %d/%d/%d/%d", sign(strcmp(words[i], words[j])), sign(strncmp(words[i], words[j], 1)),
                   sign(strcasecmp(words[i], words[j])), sign(strncasecmp(words[i], words[j], 2)));
        printf("\n");
    }
    printf("memcmp %d %d %d\n", sign(memcmp("ab\x80", "ab\x01", 3)), sign(memcmp("abc", "abc", 3)), sign(memcmp("a", "b", 0)));
    printf("len %u %u %u %u\n", (unsigned) strlen(""), (unsigned) strlen(h), (unsigned) strnlen(h, 5), (unsigned) strnlen("ab", 9));
    at("chr", h, strchr(h, 'o')); at("chr0", h, strchr(h, 0)); at("chrx", h, strchr(h, 'x'));
    at("rchr", h, strrchr(h, 'o')); at("rchr0", h, strrchr(h, 0)); at("chrnul", h, strchrnul(h, 'x'));
    at("chr hi", hi, strchr(hi, 0xe9)); at("chr 256+", h, strchr(h, 'o' + 256));
    at("str", h, strstr(h, "hello again")); at("str empty", h, strstr(h, "")); at("str none", h, strstr(h, "worlds"));
    at("pbrk", h, strpbrk(h, ";,")); at("pbrk none", h, strpbrk(h, "xyz"));
    printf("spn %u %u %u %u\n", (unsigned) strspn(h, "leho"), (unsigned) strcspn(h, ",;"), (unsigned) strspn(h, ""), (unsigned) strcspn(h, ""));
    at("memchr", h, memchr(h, ';', 25)); at("memchr short", h, memchr(h, ';', 5)); at("memchr hi", hi, memchr(hi, 0x1e9, 3));
    strcpy(b, "abc"); strcat(b, "def"); show("cat", b);
    strncat(b, "ghijk", 2); show("ncat", b);
    memset(c, 'x', 10); c[10] = 0; strncpy(c, "ab", 5); printf("ncpy %d %d %d %d [%s]\n", c[2], c[4], c[5], c[9], c);
    memset(c, 'x', 10); c[10] = 0; strncpy(c, "abcdefgh", 4); show("ncpy long", c);
    memset(c, 'y', 12); c[12] = 0; t = stpcpy(c, "hey"); at("stpcpy", c, t);
    memset(c, 'y', 12); c[12] = 0; t = stpncpy(c, "ab", 5); at("stpncpy", c, t); printf("stpncpy %d %d\n", c[2], c[5]);
    memset(c, 'y', 12); c[12] = 0; t = stpncpy(c, "abcdef", 3); at("stpncpy long", c, t);
    strcpy(b, "0123456789"); memmove(b + 2, b, 5); show("move up", b);
    strcpy(b, "0123456789"); memmove(b, b + 3, 5); show("move down", b);
    d = strdup("dup"); show("dup", d); free(d);
    d = strndup("duplicate", 3); show("ndup", d); free(d);
    d = strndup("du", 9); show("ndup short", d); free(d);
    strcpy(b, " a,b;;c ,"); t = strtok(b, " ,;");
    while (t) { show("tok", t); t = strtok(NULL, " ,;"); }
    strcpy(b, ";;;"); show("tok none", strtok(b, ";"));
    return 0;
}
