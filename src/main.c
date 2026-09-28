/*
 * The command line and what it asks for: compiling, linking, both at once,
 * or a library; the help; and the counts a build can report.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "acc.h"
#include "ctype.h"
#include "fmt.h"
#include "timing.h"
#include "version.h"
#include "parse_int.h"

/* ------------------------------------------------------------------ */

/* What -D and -U were given, in order. They cannot be acted on as they are
 * read, because the names table they go into is made after the options are.
 * Sixty-four is more than any command line here has wanted. */
#define CMDLINE_MAX 64

static struct {
    const char *arg;
    int         undef;
} cmdline[CMDLINE_MAX];

static int ncmdline;

/* Where the release puts the headers and the library on the Agon's SD card.
 * A build can name others with -DACC_INCLUDE_DIR=... and -DACC_LIBC=...; the
 * host build names none, and is given -I and the library by path. */
#if defined(AGONDEV) && !defined(ACC_INCLUDE_DIR)
#define ACC_INCLUDE_DIR "/lib/acc/include"
#endif

/* What acc prints about itself. Both fit a screen of 30 rows, which is what
 * the Agon has with an 8x16 font: text that scrolls off the top before it
 * can be read is no help. */
static void summary(FILE *f)
{
    fprintf(f,
        "acc " ACC_VERSION ", a C compiler for the Agon\r\n"
        "\r\n"
        "  acc prog.c                  compile and link prog.bin\r\n"
        "  acc -c prog.c               compile to prog.o\r\n"
        "  acc main.o util.o           link main.bin\r\n"
        "  acc main.c util.o lib.a     compile main.c, link all three\r\n"
        "  acc -a lib.a a.o b.o        put objects in a library\r\n"
        "  acc -h                      every option\r\n");
}

__attribute__((noreturn)) static void help(void)
{
    printf(
        "usage: acc [-c] <file.c> [<file.o|lib.a>]... [options]\r\n"
        "       acc <file.o|lib.a>... [options]\r\n"
        "       acc -a <lib.a> <file.o>...\r\n"
        "\r\n"
        "  -o <file>       what to write; else the input's name, .bin or .o\r\n"
        "  -c              compile to an object, to be linked later\r\n"
        "  -a <lib.a>      put the objects that follow in a library\r\n"
        "  -I <dir>        look in <dir> for #include, after the source's own\r\n"
        "  -D <n>[=<v>]    define a macro, as #define; -U <n> undefines one\r\n"
        "  -include <f>    read <f> before the source\r\n"
        "  -b <hex>        the address to load at; 40000 unless given\r\n"
        "  -r <file>       write the offsets inside the image -b moved\r\n"
        "  -map <file>     write where each function and variable went\r\n"
        "  -p              print what main returned, as six hex digits\r\n"
        "  -x              write what main returned to IO port 0\r\n"
        "  -trigraphs      read ?\?( and the eight others\r\n"
        "  -errors <file>  write an error to <file> too, and fail with 100\r\n"
        "  -v              print the version; -h prints this\r\n"
#if defined(ACC_INCLUDE_DIR) && defined(ACC_LIBC)
        "\r\n"
        "Headers from " ACC_INCLUDE_DIR ", the library " ACC_LIBC ".\r\n"
#endif
        );
    exit(0);
}

/* A command line acc does not take: the summary, and a failure. On the
 * Agon that is MOS's "Invalid parameter", which MOS then prints. */
__attribute__((noreturn)) static void usage(void)
{
    summary(stderr);
    exit(errors_asked ? ERRORS_EXIT : USAGE_EXIT);
}

#if defined(AGONDEV) && defined(ACC_CYCLES)
#include <ez80f92.h>

/* How many cycles the compile takes, counted by the eZ80 itself: timer 1,
 * which MOS leaves alone, counting down from 65535 once every 256 cycles.
 * The seconds MOS reports come from a clock another thread of the emulator
 * keeps, and wander by a few percent from one sitting to the next; the
 * timer is stepped by the instructions the emulator runs, so the same
 * compile counts the same, give or take the interrupts that arrive while it
 * runs.
 *
 * The emulator can count too, from 2037657 on: a write to IO port 0x40
 * starts its count and one to 0x41 prints it on the host, exactly and with
 * no pass to run out of. Both are written here, bench.sh takes the
 * emulator's figure when there is one and the timer's when there is not,
 * and an older emulator ignores the two ports.
 *
 * One pass of the timer is 16.7 million cycles, about 0.9 s, and two of the
 * benchmark's inputs take longer: matrix.c crossed it by a hair and was
 * silently read from the seconds instead, 17% high. So the timer runs
 * continuously, and each time it comes round it raises a flag that reading
 * the control register clears. cycles_poll counts those, once for every
 * declaration at file scope -- often enough, since none takes a pass of the
 * timer, and cheap: the build that is not counting has no call at all. */
static unsigned long cycles_wraps;

#define CYCLES_PASS (0xffffUL * 256)

static void cycles_start(void)
{
    IO(TMR1_CTL) = 0;
    IO(TMR1_RR_L) = 0xff;
    IO(TMR1_RR_H) = 0xff;
    cycles_wraps = 0;
    (void) IO(TMR1_CTL);        /* any flag from before, cleared */
    IO(TMR1_CTL) = 0x1f;        /* on, reloaded now, clock / 256, continuous */
    IO(0x40) = 0;               /* and the emulator's count, from here */
}

void cycles_poll(void)
{
    if (IO(TMR1_CTL) & 0x80)
        cycles_wraps++;
}

/* The flag is read on both sides of the count. One that was up before it is
 * a pass that finished before the count was taken. One that went up after
 * the first read belongs before the count if the count is from the top of a
 * new pass, and after it if the count is still low in the old one. */
static void cycles_report(void)
{
    unsigned char before;

    IO(0x41) = 0;               /* the emulator's count, to here */
    before = IO(TMR1_CTL);
    unsigned lo = IO(TMR1_DR_L), hi = IO(TMR1_DR_H);
    unsigned char after = IO(TMR1_CTL);
    unsigned count = hi << 8 | lo;

    if (!(before & 0x01)) {
        printf("Cycles: the timer was stopped\r\n");

        return;
    }
    if (before & 0x80)
        cycles_wraps++;
    if ((after & 0x80) && count >= 0x8000)
        cycles_wraps++;
    printf("Cycles: %lu\r\n",
           cycles_wraps * CYCLES_PASS + (0xffffUL - count) * 256);
}
#else
#define cycles_start()
#define cycles_report()
#endif

#if defined(AGONDEV) && defined(ACC_STACK)
/* How deep the stack goes, for sizing the room the linker script leaves it
 * above the heap.
 *
 * The reserve -- everything from the heap's top to the stack's bottom -- is
 * painted before the compile and read back after it. The lowest byte still
 * holding the pattern is as far down as the stack reached, give or take a
 * frame that wrote nothing. Nothing else is in that region: the heap stops
 * at ___heaptop, which is where the painting starts.
 *
 * Built by `make -f Makefile.agon STACK=1`. Not in the ordinary build: the
 * painting is a pass over 8 KB, and the report would be noise in front of
 * every compile. */
/* The linker's own symbols, whose names gain an underscore on the way from
 * C to the assembler: ___heaptop and __stack. */
extern char __heaptop[], _stack[];

#define STACK_PAINT 0x5a

static void stack_paint(void)
{
    char here;
    char *p;

    /* Up to a little below this frame, which is live. */
    for (p = __heaptop; p < &here - 64; p++)
        *p = STACK_PAINT;
}

static void stack_report(void)
{
    char *p;

    for (p = __heaptop; p < _stack; p++)
        if (*p != STACK_PAINT)
            break;

    printf("Stack: %u bytes of %u reserved\r\n",
           (unsigned) (_stack - p), (unsigned) (_stack - __heaptop));
}
#else
#define stack_paint()
#define stack_report()
#endif

/* ------------------------------------------------------------------ */
/* linking                                                             */

/* An input that is already compiled. By its name, as every other toolchain
 * tells them apart, and not by looking inside: a file called x.c that turns
 * out to hold an object is a mistake worth a clear complaint rather than a
 * clever recovery. */
int ends_in(const char *path, char what)
{
    size_t n = strlen(path);

    return n > 2 && path[n - 2] == '.' && path[n - 1] == what;
}

#define is_object(path)  ends_in((path), 'o')

/* What is written when -o is not given: the first input's name with its
 * extension changed, beside it -- as zap names its output. */
static const char *output_named(const char *from, const char *ext)
{
    static char name[256];
    int n = 0, dot = -1;

    for (; from[n]; n++) {
        if (from[n] == '.')
            dot = n;
        else if (from[n] == '/' || from[n] == '\\' || from[n] == ':')
            dot = -1;
    }
    if (dot >= 0)
        n = dot;
    if (n + (int) strlen(ext) >= (int) sizeof name)
        acc_error("'%s' is too long a name to write the output beside", from);
    memcpy(name, from, (size_t) n);
    strcpy(name + n, ext);

    return name;
}

#if defined(AGONDEV) && defined(__clang__)
/* The command line, split by acc with no limit on how many words it has.
 *
 * agondev's startup has a splitter of its own (__arg_processing), which puts
 * the words in an array of 16 -- the program's name and 15 words -- and
 * drops every word past that without a word: a link of 23 objects lost the
 * last eight, and said a function in one of them was never defined. So the
 * startup's call to split the line (parse_option) only keeps the line, and
 * main splits it here, with the quotes and the `>`, `>>` and `<` that
 * agondev's understands. */
static char *agon_line;                 /* what MOS passed */

char parse_option(char *line, char **argv)
{
    (void) argv;
    agon_line = line;

    return 1;                           /* argv[0], which crt0 has put there */
}

/* A redirection: `>` or `>>` sends what acc prints to the file, through
 * src/fmt.c, which is where all of it goes -- agondev's own stdout is the
 * console whatever it is reopened as. acc reads nothing, so `<` is taken
 * and has nothing to do. */
static void agon_close_redirect(void)
{
    fclose(fmt_console);
}

static void agon_redirect(const char *how, char *file)
{
    if (how[0] == '<')
        return;
    fmt_console = fopen(file, how[1] == '>' ? "a" : "w");
    if (!fmt_console)
        acc_error("cannot write '%s'", file);
    atexit(agon_close_redirect);
}

/* The words of agon_line, after `name`: a word in double quotes is the
 * words inside them, and a redirection -- `>`, `>>` or `<`, with its file
 * joined to it or the next word -- is made, and is not a word. */
__attribute__((noinline))
static char **agon_split(char *name, int *argcp)
{
    char *p = agon_line, **argv, **put;
    size_t room = strlen(p) + 2;        /* words, at most: a word and a space */
    int n = 1;

    /* Walked by a pointer, and sized by calloc: a scale by an entry's three
     * bytes is a multiply, which is a call into the runtime at every site. */
    argv = calloc(room, sizeof *argv);
    if (!argv)
        acc_error("out of memory for the command line");
    put = argv;
    *put++ = name;
    for (;;) {
        char *word;
        int redirect;

        while (*p == ' ')
            p++;
        if (!*p)
            break;
        redirect = *p == '>' || *p == '<';
        if (*p == '"') {
            word = ++p;
            while (*p && *p != '"')
                p++;
        } else {
            word = p;
            while (*p && *p != ' ')
                p++;
        }
        if (*p)
            *p++ = '\0';
        if (!redirect) {
            *put++ = word;
            n++;
            continue;
        }
        /* `>` alone, or `>>`, or `<`: the file is the next word. */
        if (!word[1] || (word[1] == '>' && !word[2])) {
            char *file;

            while (*p == ' ')
                p++;
            file = p;
            while (*p && *p != ' ')
                p++;
            if (*p)
                *p++ = '\0';
            agon_redirect(word, file);
        } else {
            agon_redirect(word, word + (word[1] == '>' ? 2 : 1));
        }
    }
    *put = NULL;
    *argcp = n;

    return argv;
}
#endif

int main(int argc, char **argv)
{
    const char *in = NULL, *out = NULL, *relocs = NULL;
    const char **objs;
    int nobjs = 0, to_object = 0, to_archive = 0;
    int ending = END_RETURN;
    int i;
    clock_t begin;
    unsigned cs;

#if defined(AGONDEV) && defined(__clang__)
    argv = agon_split(argv[0], &argc);
#endif
    objs = malloc((size_t) argc * sizeof *objs);
    if (!objs)
        acc_error("out of memory for the inputs");

    if (argc < 2) {
        summary(stdout);

        return 0;
    }
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            help();
        } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version")) {
            obj_print_version();

            return 0;
        } else if (argv[i][0] == '-' && argv[i][1] == 'o') {
            if (argv[i][2])
                out = argv[i] + 2;
            else if (++i < argc)
                out = argv[i];
            else
                usage();
        } else if (argv[i][0] == '-'
                   && (argv[i][1] == 'D' || argv[i][1] == 'U')) {
            /* Kept as they were given, in the order they were given: -D of
             * a name and then -U of it leaves it undefined, and the other
             * way round leaves it defined. Acted on below, once lex_init
             * has made a names table to put them in. */
            int undef = argv[i][1] == 'U';
            const char *arg = argv[i][2] ? argv[i] + 2
                            : ++i < argc  ? argv[i] : NULL;

            if (!arg)
                usage();
            if (ncmdline == CMDLINE_MAX)
                acc_error("more than %d -D and -U options", CMDLINE_MAX);
            cmdline[ncmdline].arg = arg;
            cmdline[ncmdline].undef = undef;
            ncmdline++;
        } else if (argv[i][0] == '-' && argv[i][1] == 'I') {
            if (argv[i][2])
                lex_add_include(argv[i] + 2);
            else if (++i < argc)
                lex_add_include(argv[i]);
            else
                usage();
        } else if (argv[i][0] == '-' && argv[i][1] == 'b') {
            const char *arg = argv[i][2] ? argv[i] + 2
                            : ++i < argc  ? argv[i] : NULL;
            char *end;
            long value;

            if (!arg)
                usage();
            value = strtol(arg, &end, 16);
            if (*end || value < 0 || value > 0xfe0000)
                acc_error("-b wants an address in hexadecimal, and '%s' is "
                          "not one", arg);
            out_base = (int) value;
        } else if (argv[i][0] == '-' && argv[i][1] == 'r') {
            if (argv[i][2])
                relocs = argv[i] + 2;
            else if (++i < argc)
                relocs = argv[i];
            else
                usage();
        } else if (argv[i][0] == '-' && argv[i][1] == 'c' && !argv[i][2]) {
            to_object = 1;
        } else if (argv[i][0] == '-' && argv[i][1] == 'a') {
            /* The library to make, named here rather than with -o, so that
             * what is being made is one word and reads left to right. */
            if (argv[i][2])
                out = argv[i] + 2;
            else if (++i < argc)
                out = argv[i];
            else
                usage();
            to_archive = 1;
        } else if (argv[i][0] == '-' && argv[i][1] == 'x' && !argv[i][2]) {
            ending = END_EXIT;
        } else if (argv[i][0] == '-' && argv[i][1] == 'p' && !argv[i][2]) {
            ending = END_PRINT;
        } else if (!strcmp(argv[i], "-trigraphs")) {
            lex_trigraphs = 1;
        } else if (!strcmp(argv[i], "-include")) {
            if (++i == argc)
                usage();
            lex_prelude = argv[i];
        } else if (!strcmp(argv[i], "-map")) {
            if (++i == argc)
                usage();
            obj_map_path = argv[i];
        } else if (!strcmp(argv[i], "-errors")) {
            errors_asked = 1;
            if (++i == argc)
                usage();
            errors_path = argv[i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "acc: '%s' is not an option\r\n\r\n", argv[i]);
            usage();
        } else if (is_object(argv[i]) || is_archive(argv[i])) {
            objs[nobjs++] = argv[i];
        } else if (!in) {
            in = argv[i];
        } else {
            usage();
        }
    }
    if (!out && !to_archive && (in || nobjs))
        out = output_named(in ? in : objs[0], to_object ? ".o" : ".bin");
    if (!out || (!in && !nobjs) || (in && nobjs && (to_object || to_archive)))
        usage();
    if (to_object && !in)
        usage();
    if (to_archive && (!nobjs || to_object))
        usage();
#ifdef ACC_INCLUDE_DIR
    lex_add_include_default(ACC_INCLUDE_DIR); /* after every -I */
#endif

    begin = clock();
    stack_paint();
    cycles_start();

    name_init();
    lex_init();
    sym_init();
    gen_init();

    /* The command line's macros, before a source line is read. */
    for (i = 0; i != ncmdline; i++) {
        if (cmdline[i].undef)
            lex_undefine(cmdline[i].arg);
        else
            lex_define(cmdline[i].arg);
    }

    if (to_archive) {
        /* Nothing is compiled and nothing is linked: the objects are put
         * together with a list of what each of them defines in front. */
        ar_write(out, objs, nobjs);
    } else if (!in) {
        /* Linking. The entry stub goes in first, as it does for a program
         * compiled in one piece, and its call to main is a fixup like any
         * other -- which is what makes the objects' own symbols do the work
         * of finding it. Nothing in it is moved once it is written, so what
         * is done goes to the file as the link goes. */
        out_open(out, 1);
        out_may_flush = OUT_FLUSH_IN_PLACE;
        if (obj_map_path)
            obj_link_map_open();
        gen_startup(ending, out);
        link_inputs(objs, nobjs);
        gen_finish();
#ifdef ACC_TABLE_STATS
        {
            int out_npatches(void), out_nadds(void);

            fprintf(stderr, "fixups %d patches %d adds %d\n", gen_nfixups(),
                    out_npatches(), out_nadds());
        }
#endif
        obj_link_map_close();
        out_close();
    } else if (to_object && obj_current(out, in)) {
        /* Nothing to do: the object is there and every file it was made
         * from is unchanged. This is the whole point of an object knowing
         * what it was made from. */
        printf("%s is up to date\r\n", out);
    } else if (to_object) {
        /* Compiling to an object. No header and no entry stub: those belong
         * to a program, and this is a piece of one. Based at zero, so that
         * every address in it is an offset from its own first byte and
         * placing it is one addition. */
        gen_objects = 1;
        lex_want_deps();
        out_base = 0;
        out_open(out, 0);
        out_may_flush = OUT_FLUSH_COPY;
        lex_open(in);
        translation_unit();
        lex_end();
        bss_end();
        gen_finish();
#ifdef ACC_TABLE_STATS
        {
            unsigned gen_table_bytes(void);
            extern int out_nflushes, out_nspill_cuts, out_nresidents;

            fprintf(stderr, "tables %u\n", gen_table_bytes());
            fprintf(stderr, "spill %d flushes %d cuts %d reads\n", out_nflushes,
                    out_nspill_cuts, out_nresidents);
        }
#endif
        lex_close();
        sym_members_free();             /* room for writing the object */
        name_table_free();
        obj_write(out);
        if (relocs)
            out_relocs_write(relocs);
        out_free();
    } else {
        /* A program from a source, and whatever it calls from the objects
         * and libraries named with it -- and from the default library --
         * linked in after it, as a link of its object would. */
        out_open(out, 1);
        out_may_flush = OUT_FLUSH_COPY;
        gen_startup(ending, out);
        lex_open(in);
        translation_unit();
        lex_end();
        bss_end();
        lex_close();                    /* its window is room for the link */
        sym_members_free();
        link_inputs(objs, nobjs);
        gen_finish();
#ifdef ACC_TABLE_STATS
        {
            extern int out_nflushes, out_nspill_cuts, out_nresidents;

            fprintf(stderr, "spill %d flushes %d cuts %d reads\n", out_nflushes,
                    out_nspill_cuts, out_nresidents);
        }
#endif
        if (relocs)
            out_relocs_write(relocs);
        out_close();
    }

    /* Reported the way zap reports it, down to the wording, so that the two
     * halves of a build can be read as one number. Measured from after the
     * arguments are checked to after the file is written: everything a
     * "how long did that take" is asking about, and nothing else. */
    cs = elapsed_cs(begin, clock());
    cycles_report();
    stack_report();
    printf("Done in %u.%02u seconds\r\n", cs / 100, cs % 100);

    /* The one thing this process gives back.
     *
     * acc does not free what it allocates: it runs once and exits, and the
     * code to unwind a symbol table costs bytes on a machine where bytes
     * are the scarce thing. That reasoning holds for everything whose size
     * is fixed by the program being compiled -- and it is why every suite
     * but one turns leak checking off.
     *
     * The one is macro.sh, which leaves it on because a preprocessor that
     * leaks leaks once per expansion, and that grows without bound where a
     * symbol table does not. This array was the only allocation standing
     * between that check and meaning something: it made acc exit non-zero
     * under the sanitizer, so every comparison in the file failed and the
     * leak it was watching for could not have been seen. */
    free(objs);

    /* It worked: no error to read, and none left from a run before. */
    if (errors_path)
        remove(errors_path);

    return 0;
}
