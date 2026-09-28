/* The string functions acc's runtime does with the block instructions --
 * strlen, strcmp, strncmp, memcmp, strchr, strrchr, strcpy and strcat --
 * at their edges: empty strings, a count of nothing, a count that ends
 * inside a string, bytes past 127 (compared as unsigned char), c as the
 * terminator, a character not there, and one called through a pointer.
 */
typedef unsigned int size_t;
size_t strlen(const char *);
int strcmp(const char *, const char *);
int strncmp(const char *, const char *, size_t);
int memcmp(const void *, const void *, size_t);
char *strchr(const char *, int);
char *strrchr(const char *, int);
char *strcpy(char *, const char *);
char *strcat(char *, const char *);

static int sign(int v) { return v < 0 ? -1 : v > 0; }

int main(void) {
    char buf[32];
    size_t (*len)(const char *) = strlen;
    const char *s = "abcabc";
    int r = 0;

    if (strlen("") == 0 && strlen("abc") == 3 && len("hello") == 5) r++;

    if (strcmp("", "") == 0 && strcmp("abc", "abc") == 0) r++;
    if (sign(strcmp("abc", "abd")) == -1 && sign(strcmp("abd", "abc")) == 1) r++;
    if (sign(strcmp("ab", "abc")) == -1 && sign(strcmp("abc", "ab")) == 1) r++;
    if (sign(strcmp("\x80", "\x7f")) == 1 && sign(strcmp("a", "b")) == -1) r++;
    if (sign(strcmp("\xff", "")) == 1 && sign(strcmp("", "\xff")) == -1) r++;

    if (strncmp("abc", "abd", 0) == 0 && strncmp("abc", "abd", 2) == 0) r++;
    if (sign(strncmp("abc", "abd", 3)) == -1 && strncmp("ab", "ab", 99) == 0) r++;
    if (sign(strncmp("ab", "abc", 99)) == -1 && sign(strncmp("\x90", "a", 1)) == 1) r++;

    if (memcmp("abc", "xyz", 0) == 0 && memcmp("a\0b", "a\0b", 3) == 0) r++;
    if (sign(memcmp("a\0b", "a\0c", 3)) == -1 && sign(memcmp("\xf0", "\x01", 1)) == 1) r++;

    if (strchr(s, 'b') == s + 1 && strchr(s, 'z') == 0 && strchr(s, 0) == s + 6) r++;
    if (strchr("", 0) != 0 && strchr("", 'a') == 0) r++;
    if (strrchr(s, 'b') == s + 4 && strrchr(s, 'z') == 0 && strrchr(s, 0) == s + 6) r++;
    if (strrchr(s, 'a') == s + 3 && strrchr("", 'a') == 0) r++;

    if (strcpy(buf, "hello") == buf && strcmp(buf, "hello") == 0) r++;
    if (strcpy(buf, "") == buf && buf[0] == 0) r++;
    buf[0] = 0;
    if (strcat(buf, "ab") == buf && strcat(buf, "") == buf && strcmp(buf, "ab") == 0) r++;
    if (strcat(buf, "cd") == buf && strlen(buf) == 4 && buf[4] == 0) r++;

    return r + 23;              /* 19 checks */
}
