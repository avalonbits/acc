/* <time.h> against libagon's: a broken-down time from seconds and back,
 * asctime and ctime's text, and difftime.
 *
 * Where libagon departs from C99 acc does not follow, and those are held
 * to glibc in test/hosted/: libagon's mktime is a day out on 2000-03-01
 * and 2004-02-29, and does not put fields out of their range right (C99
 * 7.23.2.3); its gmtime has tm_isdst -1, where acc's, as glibc's, has 0:
 * there is no summer time in UTC. */
#include <stdio.h>
#include <string.h>
#include <time.h>

static void tm_show(const char *what, const struct tm *t)
{
    printf("%s %d-%02d-%02d %02d:%02d:%02d wday %d yday %d\n", what, t->tm_year + 1900, t->tm_mon + 1,
           t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, t->tm_wday, t->tm_yday);
}

int main(void)
{
    static const long ts[] = { 0L, 1L, 86399L, 86400L, 951782400L, 951868800L, 1700000000L, 2147483647L,
                               946684799L, 1078012800L };
    unsigned i;
    struct tm t;
    time_t back;
    char text[40];

    for (i = 0; i < sizeof ts / sizeof *ts; i++) {
        time_t v = (time_t) ts[i];

        tm_show("gm", gmtime(&v));
        tm_show("local", localtime(&v));
        if (ts[i] != 951868800L && ts[i] != 1078012800L) {    /* see the top */
            t = *gmtime(&v);
            back = mktime(&t);
            printf("mktime %ld\n", (long) back);
        }
        strcpy(text, asctime(gmtime(&v)));
        printf("asctime %s", text);
        printf("ctime %s", ctime(&v));
    }
    printf("difftime %ld %ld\n", (long) difftime((time_t) 100, (time_t) 40), (long) difftime((time_t) 40, (time_t) 100));
    printf("CLOCKS_PER_SEC %ld\n", (long) CLOCKS_PER_SEC);
    /* The emulator's clock was never set: 1980-01-01 and on, in hours so
     * that the seconds between the two runs do not show. */
    printf("time %ld\n", (long) (time(NULL) / 3600));
    return 0;
}
