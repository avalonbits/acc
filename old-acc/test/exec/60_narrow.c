/* expect:
sc -5 127 -128
uc 250 0 255
ss -300 32767 -32768
us 60000 0 65535
str AB hello
idx 65 66 67
rev cba
sum 6
trunc 66 -56 4660
*/
int printf(const char *, ...);
char buf[8];
short sarr[4];

/* Written through pointers, so the narrow store paths are exercised too. */
void setc(char *p, int v) { *p = v; }
void sets(short *p, int v) { *p = v; }
int getc_(char *p) { return *p; }

int strlen_(char *s) { int n = 0; while (*s) { n = n + 1; s = s + 1; } return n; }

int main(void) {
    signed char sc = -5, scmax = 127, scmin = -128;
    unsigned char uc = 250, ucmin = 0, ucmax = 255;
    short ss = -300, ssmax = 32767, ssmin = -32768;
    unsigned short us = 60000, usmin = 0, usmax = 65535;
    int i;

    printf("sc %d %d %d\r\n", sc, scmax, scmin);
    printf("uc %d %d %d\r\n", uc, ucmin, ucmax);
    printf("ss %d %d %d\r\n", ss, ssmax, ssmin);
    printf("us %d %d %d\r\n", us, usmin, usmax);

    buf[0] = 'A'; buf[1] = 'B'; buf[2] = 0;
    printf("str %s %s\r\n", buf, "hello");

    setc(&buf[0], 'A'); setc(&buf[1], 'B'); setc(&buf[2], 'C'); setc(&buf[3], 0);
    printf("idx %d %d %d\r\n", getc_(&buf[0]), getc_(&buf[1]), getc_(&buf[2]));

    /* reverse in place, which reads and writes bytes through pointers */
    buf[0] = 'a'; buf[1] = 'b'; buf[2] = 'c'; buf[3] = 0;
    {
        char t = buf[0]; buf[0] = buf[2]; buf[2] = t;
    }
    printf("rev %s\r\n", buf);

    for (i = 0; i < 4; i++) sets(&sarr[i], i);
    { int s = 0; for (i = 0; i < 4; i++) s = s + sarr[i]; printf("sum %d\r\n", s); }

    /* truncation on the way into a narrow object */
    { char c = 322; signed char d = 200; short e = 0x11234; 
      printf("trunc %d %d %d\r\n", c, d, e); }
    return 0;
}
