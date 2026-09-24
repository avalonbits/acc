/* What the VDP functions send, caught on its way out.
 *
 * Every byte for the VDP goes through putch or mos_puts, in libagon and in
 * acc's library alike, so a program that defines both catches all of them.
 * libagon's printf goes through putch too, so what the tests say is not
 * printed: each call's bytes are written as a line of hex to vdp.txt on
 * the card, through MOS's own file calls, and test/agonlib.sh reads the
 * file back. A call that should send nothing writes an empty line. */
#include <stdio.h>
#include <string.h>

#include <agon/mos.h>

static unsigned char caught[1024];
static int ncaught;
static uint8_t out_fh;

int putch(int c)
{
    if (ncaught < (int) sizeof caught)
        caught[ncaught++] = (unsigned char) c;

    return c;
}

void mos_puts(const char *buffer, uint24_t size, char delimiter)
{
    if (!size) {
        while (*buffer != delimiter)
            putch((unsigned char) *buffer++);
        return;
    }
    while (size--)
        putch((unsigned char) *buffer++);
}

static void out(const char *s)
{
    if (!out_fh)
        out_fh = mos_fopen("vdp.txt", FA_WRITE | FA_CREATE_ALWAYS);
    mos_fwrite(out_fh, (char *) s, (uint24_t) strlen(s));
}

/* The bytes since the last one, under a name. */
static void sent(const char *name)
{
    static char line[3200];
    int i, n;

    n = sprintf(line, "%s:", name);
    for (i = 0; i < ncaught; i++)
        n += sprintf(line + n, " %02x", caught[i]);
    line[n++] = '\n';
    line[n] = 0;
    out(line);
    ncaught = 0;
}

#define CALL(x) do { ncaught = 0; x; sent(#x); } while (0)

/* A call test/vdpsync.sh should not play into the VDP, because of what it
 * does there -- terminal mode stops the VDU commands altogether -- written
 * with a ! in front, which the replay passes over. Its bytes are still held
 * to libagon's. */
#define CALL_UNREPLAYED(x) do { ncaught = 0; x; sent("!" #x); } while (0)

/* A call libagon does not have: acc's build makes it and writes what it
 * sent, and libagon's build writes the bytes the VDP's own source says the
 * command is -- worked out by hand, which is the point: the two lines have
 * to agree. test/vdpsync.sh then plays acc's into the VDP. */
static void expect(const char *name, const char *hex)
{
    static char line[3200];

    sprintf(line, "%s: %s\n", name, hex);
    out(line);
}

#ifdef AGONDEV
#define NEW(x, hex) expect(#x, hex)
#define NEW_UNREPLAYED(x, hex) expect("!" #x, hex)
#else
#define NEW(x, hex) CALL(x)
#define NEW_UNREPLAYED(x, hex) CALL_UNREPLAYED(x)
#endif

static void done(void)
{
    if (out_fh)
        mos_fclose(out_fh);
}
