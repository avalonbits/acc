/* The file functions of <stdio.h> against libagon's, in binary: what each
 * answers -- counts, places, flags, EOF -- around a small file. Text mode,
 * where the two libraries also agree, is test/agonlib/textmode.c. libagon
 * has no rename, and its fputs puts a newline after the string, which
 * acc's does not (test/agonlib/textmode.c). */
#include <stdio.h>
#include <string.h>

int main(void)
{
    FILE *f;
    char b[32];
    int c, n;

    printf("open missing %d\n", fopen("t_none.bin", "rb") == NULL);
    f = fopen("t_f.bin", "wb");
    printf("open w %d\n", f != NULL);
    printf("fwrite %d\n", (int) fwrite("abcdefghij", 1, 10, f));
    printf("fputc %d fwrite %d\n", fputc('K', f), (int) fwrite("LMN", 1, 3, f));
    printf("tell %ld\n", ftell(f));
    printf("fclose %d\n", fclose(f));

    f = fopen("t_f.bin", "rb");
    memset(b, 0, sizeof b);
    printf("fread %d [%s]\n", (int) fread(b, 1, 4, f), b);
    printf("tell %ld getc %c\n", ftell(f), fgetc(f));
    printf("ungetc %c getc %c\n", ungetc('Z', f), fgetc(f));
    printf("seek set %d getc %c\n", fseek(f, 8, SEEK_SET), fgetc(f));
    printf("seek cur %d getc %c\n", fseek(f, -3, SEEK_CUR), fgetc(f));
    printf("seek end %d tell %ld\n", fseek(f, -2, SEEK_END), ftell(f));
    memset(b, 0, sizeof b);
    printf("fread rest %d [%s] eof %d\n", (int) fread(b, 1, 10, f), b, feof(f) != 0);
    c = fgetc(f);
    printf("getc at end %d eof %d err %d\n", c, feof(f) != 0, ferror(f) != 0);
    clearerr(f);
    printf("cleared eof %d\n", feof(f) != 0);
    rewind(f);
    printf("rewound tell %ld\n", ftell(f));
    printf("fgets [%s]", fgets(b, 5, f));
    printf(" [%s]\n", fgets(b, sizeof b, f));
    printf("fgets at end %d\n", fgets(b, sizeof b, f) == NULL);
    fclose(f);

    f = fopen("t_f.bin", "ab");
    fputc('!', f);
    fclose(f);
    f = fopen("t_f.bin", "rb");
    n = (int) fread(b, 1, sizeof b, f);
    b[n] = 0;
    printf("appended %d [%s]\n", n, b);
    fclose(f);

    printf("remove %d again %d\n", remove("t_f.bin"), remove("t_f.bin") != 0);
    return 0;
}
