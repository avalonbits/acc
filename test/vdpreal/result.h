/* How a test/vdpreal program says what it found: a line at a time into
 * res.txt on the card, through MOS's own file calls, since the screen goes
 * nowhere. See test/vdpreal.sh. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <agon/mos.h>

static uint8_t result_fh;

static void say(const char *fmt, ...)
{
    static char line[256];
    va_list ap;

    if (!result_fh)
        result_fh = mos_fopen("res.txt", FA_WRITE | FA_CREATE_ALWAYS);
    va_start(ap, fmt);
    vsprintf(line, fmt, ap);
    va_end(ap);
    mos_fwrite(result_fh, line, (uint24_t) strlen(line));
}

static int finish(void)
{
    mos_fclose(result_fh);

    return 42;
}
