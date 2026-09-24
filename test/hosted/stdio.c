/* <stdio.h>'s files, against glibc's: every mode, reading and writing a
 * character, a line and a block at a time, pushing back, seeking and
 * telling in the middle of buffered input and output, the end-of-file and
 * error indicators, removing, renaming, temporary files, stdin from a
 * file, and atexit, whose functions run after main has returned. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void dump(const char *path)
{
    FILE *f = fopen(path, "rb");
    int c, n = 0;

    printf("[%s:", path);
    if (!f) {
        printf(" none]\n");
        return;
    }
    while ((c = fgetc(f)) != EOF) {
        putchar(c == '\n' ? '|' : c);
        n++;
    }
    printf("] %d eof %d err %d\n", n, feof(f) != 0, ferror(f) != 0);
    fclose(f);
}

static void last(void)  { printf("atexit: registered first, run last\n"); }
static void first(void) { printf("atexit: registered last, run first\n"); }

static void leave(void)
{
    exit(0);
}

int main(void)
{
    FILE *f;
    char line[64], big[2000], back[2100];
    fpos_t pos;
    long at;
    int i, c;

    atexit(last);
    atexit(first);

    /* Lines, and reading them back. */
    f = fopen("t_lines.txt", "w");
    fputs("alpha\nbeta\n", f);
    fprintf(f, "gamma %d\n", 3);
    fputc('d', f);
    fclose(f);
    dump("t_lines.txt");
    f = fopen("t_lines.txt", "r");
    while (fgets(line, sizeof line, f))
        printf("fgets [%s]\n", strchr(line, '\n') ? "line" : line);
    printf("eof %d err %d\n", feof(f) != 0, ferror(f) != 0);
    clearerr(f);
    printf("cleared %d\n", feof(f) != 0);
    rewind(f);
    printf("fgets short [%s]\n", fgets(line, 4, f));
    c = fgetc(f);
    printf("then %c, ungetc %c, again %c\n", c, ungetc('X', f), fgetc(f));
    printf("and %c at %ld\n", fgetc(f), ftell(f));
    ungetc('Y', f);
    printf("tell after ungetc %ld\n", ftell(f));
    fclose(f);

    /* Appending, which goes at the end whatever the seek. */
    f = fopen("t_lines.txt", "a");
    fputs("E\n", f);
    fseek(f, 0, SEEK_SET);
    fputs("F\n", f);
    fclose(f);
    dump("t_lines.txt");
    f = fopen("t_lines.txt", "a+");
    printf("a+ reads %c\n", fgetc(f));
    fputs("G", f);
    fclose(f);
    dump("t_lines.txt");

    /* Reading and writing one stream, with a seek between. */
    f = fopen("t_lines.txt", "r+");
    fgets(line, sizeof line, f);
    fseek(f, 0, SEEK_CUR);
    fputs("BETA", f);
    fseek(f, 0, SEEK_SET);
    fgets(line, sizeof line, f);
    printf("r+ [%s]", line);
    fgets(line, sizeof line, f);
    printf(" [%s]", line);
    fclose(f);
    dump("t_lines.txt");

    /* w+: written, rewound and read. */
    f = fopen("t_wplus.bin", "w+b");
    for (i = 0; i < 2000; i++)
        big[i] = (char) (i * 7 + i / 256);
    printf("fwrite %d\n", (int) fwrite(big, 1, 2000, f));
    printf("tell %ld\n", ftell(f));
    rewind(f);
    memset(back, 0, sizeof back);
    printf("fread %d\n", (int) fread(back, 1, 2100, f));
    printf("same %d eof %d\n", memcmp(big, back, 2000) == 0, feof(f) != 0);
    fseek(f, 700, SEEK_SET);
    fgetpos(f, &pos);
    printf("at 700: %d", fgetc(f) == (unsigned char) big[700]);
    for (i = 0; i < 600; i++)
        fgetc(f);
    printf(" 1301: %d", fgetc(f) == (unsigned char) big[1301]);
    fsetpos(f, &pos);
    printf(" back: %d tell %ld\n", fgetc(f) == (unsigned char) big[700], ftell(f));
    fseek(f, -10, SEEK_END);
    printf("from end %ld %d\n", ftell(f), fgetc(f) == (unsigned char) big[1990]);
    fseek(f, 5, SEEK_SET);
    fputc('Q', f);
    fseek(f, -1, SEEK_CUR);
    printf("wrote %c at %ld\n", fgetc(f), ftell(f) - 1);
    printf("fread 3 of 4-byte records: %d\n", (int) fread(back, 4, 3, f));
    fseek(f, 1996, SEEK_SET);
    printf("fread past the end: %d eof %d\n", (int) fread(back, 3, 3, f),
           feof(f) != 0);
    fclose(f);

    /* Unbuffered, line buffered: what is in the file before the close. */
    f = fopen("t_nbf.txt", "w");
    setvbuf(f, NULL, _IONBF, 0);
    fputs("one", f);
    dump("t_nbf.txt");
    fclose(f);
    f = fopen("t_nbf.txt", "w");
    setvbuf(f, NULL, _IOLBF, BUFSIZ);
    fputs("two\nthr", f);
    dump("t_nbf.txt");
    fclose(f);
    dump("t_nbf.txt");

    /* Modes that are not modes, and files that are not there. */
    printf("bad mode %d\n", fopen("t_nbf.txt", "x") == NULL);
    printf("missing %d\n", fopen("t_missing.txt", "r") == NULL);
    printf("write to input: %d", fputc('a', f = fopen("t_nbf.txt", "r")) == EOF);
    printf(" error %d\n", ferror(f) != 0);
    fclose(f);

    /* Renaming and removing. */
    printf("rename %d\n", rename("t_nbf.txt", "t_renamed.txt"));
    dump("t_nbf.txt");
    dump("t_renamed.txt");
    printf("remove %d %d\n", remove("t_renamed.txt"), remove("t_lines.txt"));
    printf("remove again %d\n", remove("t_renamed.txt") != 0);
    remove("t_wplus.bin");

    /* Temporary files. */
    {
        char a[L_tmpnam], b[L_tmpnam];

        printf("tmpnam %d %d\n", tmpnam(a) != NULL, tmpnam(b) != NULL);
        printf("differ %d\n", strcmp(a, b) != 0);
        f = tmpfile();
        fputs("temporary", f);
        rewind(f);
        fgets(line, sizeof line, f);
        printf("tmpfile [%s]\n", line);
        fclose(f);
    }

    /* stdin, from a file. */
    f = fopen("t_in.txt", "w");
    fputs("12 apples\nand pears\n", f);
    fclose(f);
    freopen("t_in.txt", "r", stdin);
    printf("getchar %c%c", getchar(), getchar());
    gets(line);
    printf(" gets [%s]", line);
    gets(line);
    printf(" [%s] then %d\n", line, getchar() == EOF);
    remove("t_in.txt");

    printf("fflush(NULL) %d\n", fflush(NULL));

    /* Out through exit from below main, which runs atexit's functions as
     * returning from main does. */
    leave();

    return 1;
}
