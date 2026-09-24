/* <time.h>'s calendar against glibc's in UTC: gmtime and localtime over
 * times either side of 1970, leap days, centuries and the far future;
 * mktime putting fields back in range; every strftime conversion over
 * dates chosen for the ISO week's edges; asctime and ctime; difftime; and
 * wcsftime. time() is only asked to be consistent, since the Agon's clock
 * may not be set. */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

static void show(const struct tm *t)
{
    printf("%d-%02d-%02d %02d:%02d:%02d wday %d yday %d dst %d\n", t->tm_year + 1900,
           t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, t->tm_wday,
           t->tm_yday, t->tm_isdst);
}

int main(void)
{
    static const long long times[] = {
        0, 1, 86399, 86400, -1, -86400, 951782400LL, 951868800LL, 978307199LL,
        1234567890LL, 2147483647LL, 2147483648LL, 4102444800LL, 1790000000LL,
        -2208988800LL, 253402300799LL,
    };
    static const char *const fmts[] = {
        "%a %A %b %B %h", "%c", "%C %y %Y", "%d %e %j %m", "%D %F %x", "%H %I %M %S %p",
        "%r %R %T %X", "%u %w", "%U %W %V %g %G", "%n%t%%", "%Ec %EC %Ex %EX %Ey %EY",
        "%Od %Oe %OH %OI %Om %OM %OS %Ou %OU %OV %Ow %OW %Oy", "%z %Z",
    };
    struct tm t, *p;
    char buf[128];
    wchar_t wbuf[64];
    time_t now, then;
    int i, j;

    for (i = 0; i < (int) (sizeof times / sizeof *times); i++) {
        time_t v = (time_t) times[i];

        p = gmtime(&v);
        printf("%lld: ", times[i]);
        show(p);
        t = *p;
        printf("  mktime back %d, ctime %s", mktime(&t) == v, ctime(&v));
        for (j = 0; j < (int) (sizeof fmts / sizeof *fmts); j++) {
            char *q;

            strftime(buf, sizeof buf, fmts[j], p);
            printf("  [");
            for (q = buf; *q; q++)
                printf(*q == '\t' ? "\\t" : *q == '\n' ? "\\n" : "%c", *q);
            printf("]\n");
        }
    }

    /* ISO weeks where the year they belong to is not the calendar's. */
    for (i = 0; i < 14; i++) {
        memset(&t, 0, sizeof t);
        t.tm_year = 104 + i / 2;
        t.tm_mon = i % 2 ? 11 : 0;
        t.tm_mday = i % 2 ? 29 + i % 3 : 1 + i % 4;
        t.tm_hour = 12;
        mktime(&t);
        strftime(buf, sizeof buf, "%F %a: %V %G %g, %U %W", &t);
        printf("%s\n", buf);
    }

    /* Fields out of range, put back. */
    memset(&t, 0, sizeof t);
    t.tm_year = 99;
    t.tm_mon = 13;
    t.tm_mday = 31;
    t.tm_hour = 25;
    t.tm_min = -1;
    t.tm_sec = 3600;
    t.tm_isdst = -1;
    printf("mktime %lld: ", (long long) mktime(&t));
    show(&t);
    t.tm_mday = 0;
    t.tm_mon = 2;
    t.tm_year = 100;
    mktime(&t);
    show(&t);

    printf("asctime [%s]", asctime(gmtime(&(time_t) { 1234567890 })));
    printf("difftime %g\n", difftime(100, 40));
    printf("strftime short %d\n", (int) strftime(buf, 5, "%Y-%m", gmtime(&(time_t) { 0 })));
    printf("strftime exact %d\n", (int) strftime(buf, 5, "%Y", gmtime(&(time_t) { 0 })));
    printf("wcsftime %d", (int) wcsftime(wbuf, 64, L"%A %d %B %Y", gmtime(&(time_t) { 0 })));
    for (i = 0; wbuf[i]; i++)
        putchar((int) wbuf[i]);
    putchar('\n');

    now = time(&then);
    printf("time %d %d\n", now == then, now == (time_t) -1 || now > 1600000000);
    printf("clock %d\n", clock() != (clock_t) -1);

    return 0;
}
