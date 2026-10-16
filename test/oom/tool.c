/* An Agon tool of the size and shape of the program a user reported: it
 * includes <agon/mos.h> and <agon/vdp.h> with the C headers, keeps a log
 * file, reads its options, prints its help, and talks to a UART. Built in
 * one step on the Agon by acc 0.5.1 it ran out of memory linking, with half
 * the heap free; compiled with -c and linked, it built. test/oom.sh builds
 * it in one step and runs it: `tool --help` prints its help. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <agon/mos.h>
#include <agon/vdp.h>

#define LINE_MAX_LEN 128
#define RECORDS 16
#define DEFAULT_PORT 1

static FILE *logfile;
static int verbose;

struct record {
    char     name[24];
    uint32_t value;
    int      kind;
};

static struct record records[RECORDS];
static int nrecords;

static void log_text(const char *text)
{
    if (!logfile)
        return;
    fprintf(logfile, "%s\n", text);
    fflush(logfile);
}

static void log_number(const char *label, long value)
{
    if (!logfile)
        return;
    fprintf(logfile, "%s: %ld (0x%lX)\n", label, value, value);
    fflush(logfile);
}

static void log_unsigned(const char *label, uint32_t value)
{
    if (logfile)
        fprintf(logfile, "%s: %lu\n", label, (unsigned long) value);
}

static void help(void)
{
    printf("tool - read records from a UART and keep them on the card\n");
    printf("Usage: tool [-v] [-p PORT] [-o FILE] [--help]\n");
    printf("  -v        say what it is doing\n");
    printf("  -p PORT   the UART, 0 or 1 (default %d)\n", DEFAULT_PORT);
    printf("  -o FILE   where the records go (default records.txt)\n");
    printf("  -r N=V    a record, typed in\n");
    printf("  --help    this, and nothing else\n");
}

static int parse_number(const char *text, long *value)
{
    char *end;
    long v = strtol(text, &end, 0);

    if (end == text || *end)
        return 0;
    *value = v;

    return 1;
}

static void trim(char *line)
{
    size_t n = strlen(line);

    while (n && isspace((unsigned char) line[n - 1]))
        line[--n] = 0;
    while (*line && isspace((unsigned char) *line))
        memmove(line, line + 1, strlen(line));
}

static int split(char *line, char **fields, int max)
{
    int n = 0;
    char *p = strtok(line, ",;");

    while (p && n < max) {
        trim(p);
        fields[n++] = p;
        p = strtok(NULL, ",;");
    }

    return n;
}

static int record_add(const char *name, uint32_t value, int kind)
{
    struct record *r;

    if (nrecords == RECORDS)
        return 0;
    r = &records[nrecords++];
    strncpy(r->name, name, sizeof r->name - 1);
    r->name[sizeof r->name - 1] = 0;
    r->value = value;
    r->kind = kind;
    log_unsigned(r->name, value);

    return 1;
}

static int parse_line(char *line)
{
    char *fields[4];
    long value, kind = 0;
    int n = split(line, fields, 4);

    if (n < 2 || !parse_number(fields[1], &value))
        return 0;
    if (n > 2 && !parse_number(fields[2], &kind))
        return 0;

    return record_add(fields[0], (uint32_t) value, (int) kind);
}

static int uart_open(int port)
{
    UART settings;

    memset(&settings, 0, sizeof settings);
    settings.baudRate = 115200;
    settings.dataBits = 8;
    settings.stopBits = 1;
    settings.parity = 0;
    if (verbose)
        printf("opening UART%d\n", port);
    log_number("uart", port);

    return mos_uopen(&settings);
}

static int uart_line(char *line, int room)
{
    int n = 0;

    while (n < room - 1) {
        int c = mos_ugetc();

        if (c < 0)
            return -1;
        if (c == '\r')
            continue;
        if (c == '\n')
            break;
        line[n++] = (char) c;
    }
    line[n] = 0;

    return n;
}

static int write_records(const char *path)
{
    FILE *f = fopen(path, "w");
    int i;

    if (!f) {
        printf("cannot write %s\n", path);
        return 0;
    }
    for (i = 0; i < nrecords; i++)
        fprintf(f, "%s,%lu,%d\n", records[i].name,
                (unsigned long) records[i].value, records[i].kind);
    fclose(f);
    log_text("records written");

    return 1;
}

static int by_value(const void *a, const void *b)
{
    const struct record *x = a, *y = b;

    return x->value < y->value ? -1 : x->value > y->value;
}

/* The records sorted, their mean and spread, and when: a report to the log. */
static void report(void)
{
    char stamp[32];
    time_t now = time(NULL);
    double sum = 0, squares = 0, mean, spread;
    int i;

    qsort(records, (size_t) nrecords, sizeof records[0], by_value);
    for (i = 0; i < nrecords; i++) {
        sum += (double) records[i].value;
        squares += (double) records[i].value * (double) records[i].value;
    }
    mean = nrecords ? sum / nrecords : 0;
    spread = nrecords ? sqrt(squares / nrecords - mean * mean) : 0;
    strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", gmtime(&now));
    if (logfile)
        fprintf(logfile, "%s: %d records, mean %.2f, spread %.3e\n", stamp,
                nrecords, mean, spread);
}

/* A record typed in by hand: name, then value, as "name=value". */
static int typed(const char *text)
{
    char name[24];
    unsigned long value;

    if (sscanf(text, "%23[^=]=%lu", name, &value) != 2)
        return 0;

    return record_add(name, (uint32_t) value, 0);
}

static void show(void)
{
    int i;

    vdp_clear_screen();
    vdp_set_text_colour(BRIGHT_WHITE);
    printf("%-24s %10s %4s\n", "name", "value", "kind");
    vdp_set_text_colour(WHITE);
    for (i = 0; i < nrecords; i++)
        printf("%-24s %10lu %4d\n", records[i].name,
               (unsigned long) records[i].value, records[i].kind);
}

int main(int argc, char **argv)
{
    char line[LINE_MAX_LEN];
    const char *out = "records.txt";
    long port = DEFAULT_PORT;
    int i;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            help();
            return 0;
        } else if (!strcmp(argv[i], "-v")) {
            verbose = 1;
        } else if (!strcmp(argv[i], "-p") && i + 1 < argc) {
            if (!parse_number(argv[++i], &port) || port < 0 || port > 1) {
                printf("bad port %s\n", argv[i]);
                return 1;
            }
        } else if (!strcmp(argv[i], "-o") && i + 1 < argc) {
            out = argv[++i];
        } else if (!strcmp(argv[i], "-r") && i + 1 < argc) {
            if (!typed(argv[++i]))
                printf("not name=value: %s\n", argv[i]);
        } else {
            help();
            return 1;
        }
    }

    logfile = fopen("/tool.log", "a");
    log_text("tool started");
    if (uart_open((int) port) != 0) {
        printf("no UART\n");
        return 1;
    }
    while (nrecords < RECORDS && uart_line(line, sizeof line) >= 0)
        if (!parse_line(line) && verbose)
            printf("skipped: %s\n", line);
    mos_uclose();
    report();
    show();
    write_records(out);
    if (logfile)
        fclose(logfile);

    return 0;
}
