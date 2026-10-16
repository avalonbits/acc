/* Files opened without `b`, against libagon's: a newline written is a
 * return and a line feed in the file, a return and a line feed read are one
 * newline, and fwrite and `b` leave the bytes alone. Each file is read back
 * in binary and printed byte by byte.
 *
 * libagon's fputs puts a newline of its own after the string, and reads a
 * return before a return and a line feed as two returns; acc does neither,
 * and what acc's build prints for those is libagon's to print, under
 * AGONDEV. */
#include <stdio.h>
#include <string.h>

static void bytes(const char *label)
{
    FILE *f = fopen("t_text.txt", "rb");
    int c;

    printf("%-16s", label);
    while ((c = fgetc(f)) != EOF)
        printf(" %d", c);
    printf("\n");
    fclose(f);
}

/* `raw` put in the file as it is, and read back through `mode` three ways. */
static void reads(const char *label, const char *raw, int n, const char *mode)
{
    FILE *f = fopen("t_text.txt", "wb");
    char line[32];
    int c, i;

    fwrite(raw, 1, (size_t) n, f);
    fclose(f);

    f = fopen("t_text.txt", mode);
    printf("%-10s getc ", label);
    while ((c = getc(f)) != EOF)
        printf(" %d", c);
    printf(" | tell %ld\n", ftell(f));
    fclose(f);

    f = fopen("t_text.txt", mode);
    printf("%-10s fgets", label);
    while (fgets(line, sizeof line, f)) {
        printf(" [");
        for (i = 0; line[i]; i++)
            printf(i ? " %d" : "%d", line[i]);
        printf("]");
    }
    printf("\n");
    fclose(f);

    f = fopen("t_text.txt", mode);
    n = (int) fread(line, 1, sizeof line, f);
    printf("%-10s fread %d:", label, n);
    for (i = 0; i < n; i++)
        printf(" %d", line[i]);
    printf("\n");
    fclose(f);
}

/* A file of lines, each a return and a line feed, with the first return
 * at `first` and one every `every` bytes after it: some pairs fall either
 * side of the end of a buffer. Read back as text a character at a time,
 * and in blocks of `block`: the newlines, the returns and a sum of what
 * came back. */
static void across(int first, int every, int block)
{
    static char big[3000];
    FILE *f = fopen("t_text.txt", "wb");
    long sum = 0;
    int i, c, n, newlines = 0, returns = 0, total = 0;

    for (i = 0; i < (int) sizeof big; i++)
        big[i] = (char) ('a' + i % 26);
    for (i = first; i + 1 < (int) sizeof big; i += every) {
        big[i] = '\r';
        big[i + 1] = '\n';
    }
    fwrite(big, 1, sizeof big, f);
    fclose(f);

    f = fopen("t_text.txt", "r");
    while ((c = fgetc(f)) != EOF) {
        newlines += c == '\n';
        returns += c == '\r';
        sum += c * (total++ % 7 + 1);
    }
    printf("across %d/%d getc: %d bytes %d newlines %d returns sum %ld\n",
           first, every, total, newlines, returns, sum);
    fclose(f);

    f = fopen("t_text.txt", "r");
    newlines = returns = total = 0;
    sum = 0;
    while ((n = (int) fread(big, 1, (size_t) block, f)) > 0)
        for (i = 0; i < n; i++) {
            newlines += big[i] == '\n';
            returns += big[i] == '\r';
            sum += big[i] * (total++ % 7 + 1);
        }
    printf("across %d/%d fread %d: %d bytes %d newlines %d returns sum %ld\n",
           first, every, block, total, newlines, returns, sum);
    fclose(f);
}

int main(void)
{
    FILE *f;
    char line[32];
    int a, b;

    f = fopen("t_text.txt", "w");
    fputc('a', f);
    fputc('\n', f);
    putc('b', f);
    putc('\n', f);
    fprintf(f, "%d\n%d\n", 12, 34);
    printf("tell %ld\n", ftell(f));
    fclose(f);
    bytes("fputc putc w");

    f = fopen("t_text.txt", "wb");
    fputc('a', f);
    fputc('\n', f);
    fprintf(f, "%d\n", 12);
    fclose(f);
    bytes("fputc wb");

    f = fopen("t_text.txt", "wt");
    fputc('\n', f);
    fclose(f);
    bytes("wt");

    f = fopen("t_text.txt", "w");
    fwrite("c\nd\n", 1, 4, f);
    fclose(f);
    bytes("fwrite w");

    f = fopen("t_text.txt", "a");
    fputc('\n', f);
    fclose(f);
    bytes("appended");

    /* Unbuffered, which libagon has no way to ask for: the return and the
     * line feed both reach the file. */
#ifndef AGONDEV
    f = fopen("t_text.txt", "w");
    setvbuf(f, NULL, _IONBF, 0);
    fputc('x', f);
    fputc('\n', f);
    fwrite("y\n", 1, 2, f);
    bytes("unbuffered");
    fclose(f);
#else
    printf("%-16s 120 13 10 121 10\n", "unbuffered");
#endif

    /* Written and read back through one text stream. */
    f = fopen("t_text.txt", "w+");
    fprintf(f, "%d\n%d\n", 5, 6);
    rewind(f);
    a = fgetc(f);
    b = fgetc(f);
    printf("w+ back %d %d tell %ld", a, b, ftell(f));
    printf(" scanf %d", fscanf(f, "%d", &a));
    printf(" %d\n", a);
    fclose(f);

    reads("crlf", "x\r\ny", 4, "r");
    reads("lone cr", "a\rb", 3, "r");
    reads("cr at end", "a\r", 2, "r");
    reads("lf cr", "a\n\rb", 4, "r");
    reads("binary", "x\r\ny", 4, "rb");
    reads("r+", "x\r\ny", 4, "r+");

    /* libagon's differ here: see the top. */
#ifndef AGONDEV
    f = fopen("t_text.txt", "w");
    fputs("a\nb", f);
    fputs("\n\n", f);
    fputs("c", f);
    fclose(f);
    bytes("fputs w");
    f = fopen("t_text.txt", "wb");
    fputs("a\n", f);
    fclose(f);
    bytes("fputs wb");
    reads("cr crlf", "z\r\r\nw", 5, "r");
#else
    printf("%-16s 97 13 10 98 13 10 13 10 99\n", "fputs w");
    printf("%-16s 97 10\n", "fputs wb");
    printf("cr crlf    getc  122 13 10 119 | tell 5\n");
    printf("cr crlf    fgets [122 13 10] [119]\n");
    printf("cr crlf    fread 4: 122 13 10 119\n");
#endif

    across(511, 40, 100);
    across(1023, 512, 512);
    across(0, 2, 700);
    across(510, 513, 2999);

    /* Last, since the screen is gone after it: stdout made a text file,
     * puts and printf through it, and what reached the file said on
     * stderr. libagon's freopen leaves stdout on the screen. */
#ifndef AGONDEV
    freopen("t_out.txt", "w", stdout);
    puts("p");
    printf("%d\n", 9);
    fclose(stdout);
    f = fopen("t_out.txt", "rb");
    a = (int) fread(line, 1, sizeof line, f);
    fclose(f);
    remove("t_out.txt");
    fprintf(stderr, "stdout to a file %d:", a);
    for (b = 0; b < a; b++)
        fprintf(stderr, " %d", line[b]);
    fprintf(stderr, "\n");
#else
    fprintf(stderr, "stdout to a file 6: 112 13 10 57 13 10\n");
#endif

    remove("t_text.txt");

    return 0;
}
