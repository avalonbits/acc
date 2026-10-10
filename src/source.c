/*
 * The source: the window each file is read through, the stack of files
 * and texts being read, the line splices, trigraphs and universal
 * character names that come before tokens, what the compile read (the
 * marks behind -c's up-to-date check), the include path, recorded tokens
 * to be read again, and where in a line a token is.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "ctype.h"
#include "lex_int.h"

/* ------------------------------------------------------------------ */
/* the source                                                          */

/* A window on the file rather than the whole of it.
 *
 * The reason is the preprocessor: `#include` means several files open at
 * once, and a header that is included from a header from a header is three
 * whole files held in memory at the same time as the one being compiled.
 * One buffer each, of a size that does not grow with the file, is what makes
 * that affordable on a machine with 448 KB for everything.
 *
 * What the buffer holds is always whole lines: a read is trimmed back to the
 * last newline in it, and the bytes after that newline are carried to the
 * front on the next refill. That is the invariant the rest of the lexer
 * rests on -- a token can point straight into the buffer, because a refill
 * only ever happens where a line ended and no token in C spans a line. Nor
 * does `*<slash>`, whose two characters cannot have a newline between them,
 * so even a comment that runs over many lines is found across a refill.
 *
 * The byte at the end of the handed-out part is replaced by a '\0' so that
 * scanning stops there without a bounds test on every character, and the
 * byte it hid is put back when the window moves. */
/* The window each file gets, the one named on the command line and every
 * header below it. A line longer than that grows it (window_grow), so the
 * size is a starting point and not a limit.
 *
 * Four kilobytes is still far longer than any line anybody writes, and eight
 * of them is 32 KB where eight of 16 KB would be 128 -- on a machine with
 * 448 KB for everything, that is the difference between a deep chain of
 * headers being affordable and not. zap sized its includes the same way and
 * for the same reason. The file on the command line had 16 KB, and the 12
 * more are what one of aed's files needed to compile on the Agon
 * (test/headers.sh); reading it in 4 KB pieces costs nothing measurable. */
#define SRC_CAP     4096
#define INCLUDE_CAP 2048

char *src;           /* src_cap + 1 bytes, the +1 for the sentinel */
static int   src_cap;       /* which of the two sizes this level has */
char *cursor;
char *src_end;       /* where the sentinel sits: the last line's end */
char *src_raw;       /* one past the last byte read into the buffer */
char  src_held;      /* the byte the sentinel replaced */
FILE *src_file;      /* null once the file has been read to its end */
const char *src_path;
/* The path it was opened under, which #line does not change: see once_add. */
const char *src_real;
int   line;

int     src_owned;
NameRef src_macro;

/* In a macro's text, or a kept text read again (lex_push_record), where in
 * the file under it the text was used: the name of the outermost macro, or
 * the token the kept text was read after. Null in a file. A token read from
 * the text has that for its column, as it has that place's line: the text
 * has no lines of its own, and the place in the file is what the reader
 * can go to. `nowhere` when the place was not known. */
static const char *src_use;
static const char  nowhere[1];

/* ------------------------------------------------------------------ */
/* what the compile read                                               */

/* Every file this compile opened: the source, and each header it reached
 * through an #include. An object records them so that a later build can ask
 * whether any of them has changed, and compile again only if one has.
 *
 * What is kept of each is its length and a checksum of its bytes, which is
 * the only honest answer available on this machine. The Agon has no clock of
 * its own -- MOS gets the time from the VDP, which starts from a fixed point
 * at every power-on -- so a file written in one session and one written in
 * the next carry timestamps that say nothing about which is newer. A
 * checksum does not care.
 *
 * The bytes are taken as they are read, in refill, so nothing is read twice
 * to work it out.
 *
 * The sum and the weighted sum together are 48 bits, and the weighting is
 * what makes the order of the bytes matter: without it, two lines swapped
 * would look unchanged. Both are kept to 24 bits explicitly, so that a host
 * whose int is wider works out the same number the Agon does. It is not a
 * cryptographic hash and does not need to be: it is here to notice an edit,
 * not to withstand one. */
typedef struct {
    char    *path;
    unsigned size, sum, weighted;
} Dep;

static Dep  *deps;
static int   ndeps, deps_cap;
static int   want_deps;         /* see dep_add */
static int src_dep = -1;        /* the file being read, or -1 for macro text */

/* The build's include directory, whose headers a dependency names as
 * <name>: see lex_add_include_default. */
static const char *include_builtin;
static int         include_builtin_len;
static const char *dep_name(const char *path, char *buf, int cap);

/* A dependency's name made, or made back into a path: one at a time, a
 * compile's own or an object's being checked, so one buffer for both. The
 * Agon's /lib/acc/include paths are a quarter of it; a longer path is cut,
 * and a header not found where it was is a file that changed -- the object
 * is made again, never kept wrongly. */
static char dep_path[128];

static int dep_add(const char *path)
{
    char *keep;
    int i;

    /* Only an object records what it was made from, and only it pays for
     * the checksum: a compile straight to a program answers -1 here, and
     * every byte read after that goes through one compare. */
    if (!want_deps)
        return -1;
    path = dep_name(path, dep_path, (int) sizeof dep_path);

    /* A header that several others include is opened once for each of them,
     * and each time it reads the same bytes. One entry is enough, and it
     * keeps an object from growing with the shape of the include graph
     * rather than with the number of files in it.
     *
     * Its marks start again, because they are the marks of one reading of
     * the file: folding a second reading into the first would give a number
     * no reading of that file on its own could produce, and the build would
     * decide it had changed every time. */
    for (i = 0; i != ndeps; i++)
        if (strcmp(deps[i].path, path) == 0) {
            deps[i].size = deps[i].sum = deps[i].weighted = 0;

            return i;
        }

    if (ndeps == deps_cap) {
        deps_cap = deps_cap ? deps_cap * 2 : 16;
        deps = realloc(deps, (size_t) deps_cap * sizeof *deps);
        if (!deps)
            acc_error("out of memory for the list of files read");
    }
    keep = malloc(strlen(path) + 1);
    if (!keep)
        acc_error("out of memory for '%s'", path);
    strcpy(keep, path);
    deps[ndeps].path = keep;
    deps[ndeps].size = deps[ndeps].sum = deps[ndeps].weighted = 0;

    return ndeps++;
}

/* `n` bytes folded into the two running sums.
 *
 * The sums are kept to 24 bits, and masked once at the end rather than at
 * every byte: a sum of sums taken mod 2^24 is the same whenever the mask is
 * applied, and on the host, where unsigned is 32 bits, it wraps at a
 * multiple of 2^24.
 *
 * On the Agon this is src/marks.s instead. Every byte of every file read
 * passes through here, and agondev made this loop 93 cycles a byte, a
 * quarter of a compile -- and miscompiled the pointer form of it. Only
 * when agondev's clang is the compiler: acc builds these sources for the
 * Agon too, and takes no assembly, so it keeps the C (test/selfbuild.sh). */
#if defined(AGONDEV) && defined(__clang__)
void marks_fold(unsigned *sump, unsigned *weightedp, const char *bytes, int n);
#else
static void marks_fold(unsigned *sump, unsigned *weightedp, const char *bytes,
                       int n)
{
    const unsigned char *p = (const unsigned char *) bytes, *end = p + n;
    unsigned sum = *sump, weighted = *weightedp;

    while (p != end) {
        sum += *p++;
        weighted += sum;
    }
    *sump = sum & 0xffffffu;
    *weightedp = weighted & 0xffffffu;
}
#endif

/* The options that change what a compile reads: -D, -U and -I, in the
 * order they were given, folded into marks of their own as they arrive. An
 * object records them as one more dependency, after its files, with an
 * empty path -- a name no file can have -- so that the same source compiled
 * with another -D is compiled again rather than called up to date. */
static unsigned opt_size, opt_sum, opt_weighted;

void opt_fold(char kind, const char *arg)
{
    int n = (int) strlen(arg) + 1;      /* its NUL, between one and the next */

    marks_fold(&opt_sum, &opt_weighted, &kind, 1);
    marks_fold(&opt_sum, &opt_weighted, arg, n);
    opt_size += (unsigned) n + 1;
}

/* The bytes just read from the file at `which`, folded in. */
static void dep_bytes(int which, const char *bytes, int n)
{
    if (which < 0)
        return;
    marks_fold(&deps[which].sum, &deps[which].weighted, bytes, n);
    deps[which].size += (unsigned) n;
}

/* The marks for a file as it stands now, which is how a build asks whether
 * it is the same file an object was made from. Zero when it cannot be read,
 * which counts as changed -- a header that has gone is a reason to compile
 * again, not to assume nothing happened. */
int lex_file_marks(const char *path, unsigned *size, unsigned *sum,
                   unsigned *weighted)
{
    char buf[1024];
    FILE *f;
    size_t got;

    if (!*path) {
        *size = opt_size;
        *sum = opt_sum;
        *weighted = opt_weighted;

        return 1;
    }

    /* A header of the build's include directory, found there again. */
    if (*path == '<') {
        size_t len = strlen(path);

        if (!include_builtin || len < 3 || path[len - 1] != '>')
            return 0;
        snprintf(dep_path, sizeof dep_path, "%s/%.*s", include_builtin,
                 (int) len - 2, path + 1);
        path = dep_path;
    }
    f = fopen(path, "rb");
    if (!f)
        return 0;
    *size = *sum = *weighted = 0;
    while ((got = fread(buf, 1, sizeof buf, f)) > 0) {
        marks_fold(sum, weighted, buf, (int) got);
        *size += (unsigned) got;
    }
    fclose(f);

    return 1;
}

/* Said before the source is opened, by a compile that is going to write an
 * object. */
void lex_want_deps(void)
{
    want_deps = 1;
}

/* The files read, and after them the options, as the empty path. */
int lex_ndeps(void)
{
    return want_deps ? ndeps + 1 : 0;
}

const char *lex_dep_path(int i)
{
    return i == ndeps ? "" : deps[i].path;
}

void lex_dep_marks(int i, unsigned *size, unsigned *sum, unsigned *weighted)
{
    if (i == ndeps) {
        lex_file_marks("", size, sum, weighted);

        return;
    }
    *size = deps[i].size;
    *sum = deps[i].sum;
    *weighted = deps[i].weighted;
}

Source open_files[INCLUDE_MAX];
int    depth;
const char *include_dirs[INCLUDE_DIRS];
int         ninclude_dirs;

static void include_dir_add(const char *dir)
{
    if (ninclude_dirs == INCLUDE_DIRS)
        acc_error("more than %d -I directories", INCLUDE_DIRS);
    include_dirs[ninclude_dirs++] = dir;
}

void lex_add_include(const char *dir)
{
    include_dir_add(dir);
    opt_fold('I', dir);
}

/* The directory the build names rather than the command line -- the
 * repository's include/ on the host, /lib/acc/include on the Agon: looked
 * in the same way, and not an option, so that an object made where it is
 * looked in is the same file as one made where it is not. A header found
 * in it is recorded by its name there (see dep_add), for the same reason. */
void lex_add_include_default(const char *dir)
{
    include_dir_add(dir);
    include_builtin = dir;
    include_builtin_len = (int) strlen(dir);
}

/* A path in the build's include directory as `<name>`, the way the source
 * asked for it, and any other as it is: <stdio.h> is at one path on the
 * host and another on the Agon, and an object records it the same on both. */
static const char *dep_name(const char *path, char *buf, int cap)
{
    if (!include_builtin || strncmp(path, include_builtin, (size_t) include_builtin_len)
        || path[include_builtin_len] != '/')
        return path;
    snprintf(buf, (size_t) cap, "<%s>", path + include_builtin_len + 1);

    return buf;
}

/* More of the file, with what has not been read yet kept.
 *
 * The unconsumed tail moves to the front, the rest of the buffer is read
 * into, and the window is trimmed back to the last newline so that it ends
 * where a line does. Returns whether there is anything to look at after it,
 * which is what the callers mean by asking: at the end of the file the
 * answer is no and the sentinel stays where it is.
 *
 * Out of line and off the hot path: it runs once per 16 KB of source, where
 * skip_space runs once per token. */
/* Backslash-newline and universal character names, out of the window
 * before anything reads it.
 *
 * C deletes a backslash-newline in the second of its translation phases --
 * before the file is a sequence of tokens at all -- which is what makes a
 * name split over two lines one name, a string that runs over two lines one
 * string, and a join between two tokens leave nothing behind.
 *
 * A universal character name, \u and four hex digits or \U and eight
 * (C99 6.4.3), is a character. It is written here as the UTF-8 for it, in
 * place, so that every scanner after this sees one spelling for it: \u00e9,
 * \U000000e9 and an e-acute typed as its UTF-8 are the same name, since bytes from 0x80
 * up are identifier characters (6.4.2.1 leaves those to the implementation),
 * and a string holds the UTF-8, as agondev's does. A `\\` is stepped over
 * whole, so that "\\u00e9" stays a backslash followed by text -- unless the
 * second backslash ends its line, which makes it a join, whatever is in
 * front of it.
 *
 * Doing it here rather than in each scanner is what keeps it free. A test
 * for a backslash in the loop that walks a name and in the one that walks
 * white space cost eleven percent of a compile between them, for something
 * almost no file has; a window with neither in it pays one search instead,
 * which is the instruction the chip has for exactly that.
 *
 * The newlines taken out are put back at the end of the logical line rather
 * than dropped, so that the count of lines is what it was and a diagnostic
 * still names the line the token was written on. There is always room: each
 * join frees two characters and gives back one, and UTF-8 is shorter than
 * the name it stands for.
 *
 * `end` is one past the last newline in the window, so a join is never
 * split across a refill -- its newline is what a window ends after. */
static int splice_at(const char *r)
{
    return r[0] == '\\' && (r[1] == '\n' || (r[1] == '\r' && r[2] == '\n'));
}

/* How long the universal character name at r is, or 0 when there is none. */
int ucn_at(const char *r)
{
    int n, i;

    if (r[0] != '\\' || (r[1] != 'u' && r[1] != 'U'))
        return 0;
    n = r[1] == 'u' ? 4 : 8;
    for (i = 0; i != n; i++)
        if (!is_digit(r[2 + i]) && (unsigned) ((r[2 + i] | 0x20) - 'a') >= 6u)
            return 0;                   /* escape() says what is wrong */

    return 2 + n;
}

/* The character the universal character name at r names, which ucn_at has
 * measured as len long -- or -1 when it is one 6.4.3p2 forbids: a character
 * the basic set already has a spelling for, bar the three it has none for,
 * or half of a surrogate pair. */
static long ucn_value(const char *r, int len)
{
    uint32_t v = 0;
    int i;

    for (i = 2; i < len; i++) {
        int d = (unsigned char) r[i];

        v = v * 16 + (uint32_t) (is_digit(d) ? d - '0' : (d | 0x20) - 'a' + 10);
    }
    if ((v < 0xa0 && v != 0x24 && v != 0x40 && v != 0x60)
        || (v >= 0xd800 && v <= 0xdfff) || v > 0x10ffff)
        return -1;

    return (long) v;
}

/* The UTF-8 for v into w, answering how many bytes that was. */
static int utf8_put(char *w, uint32_t v)
{
    if (v < 0x80) {
        w[0] = (char) v;

        return 1;
    }
    if (v < 0x800) {
        w[0] = (char) (0xc0 | (v >> 6));
        w[1] = (char) (0x80 | (v & 0x3f));

        return 2;
    }
    if (v < 0x10000) {
        w[0] = (char) (0xe0 | (v >> 12));
        w[1] = (char) (0x80 | ((v >> 6) & 0x3f));
        w[2] = (char) (0x80 | (v & 0x3f));

        return 3;
    }
    w[0] = (char) (0xf0 | (v >> 18));
    w[1] = (char) (0x80 | ((v >> 12) & 0x3f));
    w[2] = (char) (0x80 | ((v >> 6) & 0x3f));
    w[3] = (char) (0x80 | (v & 0x3f));

    return 4;
}

__attribute__((noinline))
/* A universal character name that may be written as UTF-8 here: one that
 * names a character C allows. One that does not is left as it is written,
 * since it may be in a comment, where it is nothing; read in a token it is
 * refused there, on the line it is on. */
static int ucn_ok_at(const char *r)
{
    int n = ucn_at(r);

    return n && ucn_value(r, n) >= 0 ? n : 0;
}

/* Trigraphs, C99 5.2.1.1: `??(` is `[`, `??/` a backslash, and so on for
 * nine. Replaced before anything else reads the text -- before lines are
 * joined, since `??/` at a line's end joins it -- and in strings as well as
 * out of them, as C says. pr18502-1 writes `int a??(2??);`.
 *
 * Only with -trigraphs, as gcc and clang have them outside their strict
 * modes: looking for them is a scan of every byte of every file, which cost
 * 0.57% of a compile, for a form nothing written since C89 uses and that
 * turns a `??!` in a string into something else. */
int lex_trigraphs;

static int trigraph(int c)
{
    const char *from = "=(/)'<!>-", *to = "#[\\]^{|}~";
    const char *at = c ? strchr(from, c) : NULL;

    return at ? to[at - from] : 0;
}

static void untrigraph(char **end)
{
    char *r, *w, *stop = *end;
    int c;

    for (r = src;; r++) {
        r = memchr(r, '?', (size_t) (stop - r));
        if (!r || r + 2 >= stop)
            return;                     /* nothing to replace */
        if (r[1] == '?' && trigraph(r[2]))
            break;
    }
    for (w = r; r < stop;) {
        if (r[0] == '?' && r + 2 < stop && r[1] == '?' && (c = trigraph(r[2]))) {
            *w++ = (char) c;
            r += 3;

            continue;
        }
        *w++ = *r++;
    }
    memmove(w, stop, (size_t) (src_raw - stop));
    src_raw -= stop - w;
    *end = w;
}

static void unsplice(char **end)
{
    char *r, *w, *q, *nl, *stop = *end;
    int extra = 0, n;

    for (r = src;;) {
        r = memchr(r, '\\', (size_t) (stop - r));
        if (!r)
            return;                     /* nothing to take out */
        if (splice_at(r) || ucn_ok_at(r))
            break;
        r += r[1] == '\\' && !splice_at(r + 1) ? 2 : 1;
    }

    for (w = r; r < stop;) {
        if (r[0] == '\\') {
            if (splice_at(r)) {
                r += r[1] == '\r' ? 3 : 2;
                extra++;

                continue;
            }
            if ((n = ucn_ok_at(r)) != 0) {
                w += utf8_put(w, (uint32_t) ucn_value(r, n));
                r += n;

                continue;
            }
            if (r[1] == '\\' && !splice_at(r + 1))
                *w++ = *r++;            /* and the one it escapes */
            *w++ = *r++;

            continue;
        }

        /* A run with no backslash in it goes down in one move, up to the
         * end of the line when lines were joined and the newlines they
         * took are owed. A byte at a time, this was the most expensive
         * thing acc did with a file that joins a line near its start. */
        q = memchr(r, '\\', (size_t) (stop - r));
        if (!q)
            q = stop;
        if (extra && (nl = memchr(r, '\n', (size_t) (q - r))) != NULL)
            q = nl + 1;
        memmove(w, r, (size_t) (q - r));
        w += q - r;
        r = q;
        if (w[-1] != '\n')
            continue;
        while (extra) {
            *w++ = '\n';
            extra--;
        }
    }

    /* What was past the window's end is still to be read, and moves down
     * with everything else. */
    memmove(w, stop, (size_t) (src_raw - stop));
    src_raw -= stop - w;
    *end = w;
}

/* A parameter's array size, kept as the text it was written as for a
 * function's definition to read again when it is entered: see vla_texts in
 * type.c. lex_record_from starts it after the current token, and the text
 * is copied out to lex_record when it is about to go -- the window refilled
 * -- and when it is taken. A macro's expansion is read from a window of its
 * own and leaves this one where it was, so what is kept is the macro's name
 * and arguments, which say it again. Only the refill has a test of
 * lex_record, and the path every token takes has none.
 *
 * lex_record is NULL when nothing is being kept, or when this could not
 * be: it did not fit, or the size ended at another level than it began. */
char *lex_record, *lex_record_end;
static char       *record_first;
static const char *record_start;
static int         record_depth;

__attribute__((noinline))
static void record_flush(void)
{
    size_t n = (size_t) (cursor - record_start);

    if (depth != record_depth)
        return;                         /* in an expansion: not kept */
    if (n >= (size_t) (lex_record_end - lex_record)) {
        lex_record = NULL;

        return;
    }
    memcpy(lex_record, record_start, n);
    lex_record += n;
    record_start = cursor;
}

void lex_record_from(char *text, char *end)
{
    lex_record = record_first = text;
    lex_record_end = end;
    record_start = cursor;
    record_depth = depth;
}

#ifdef OPT_ACC
/* A function's whole body, kept by opt-acc for a call to read again in
 * place (inline.c) -- beside lex_record, which a for loop in the body uses
 * for its step, and grown as it goes: the host has the room. */
static char       *body_rec;
static size_t      body_len, body_cap;
static const char *body_start;
static int         body_depth, body_on;

static const char no_room[] = "out of memory for an inline function";

__attribute__((noinline))
static void body_flush(void)
{
    size_t n = (size_t) (cursor - body_start);

    if (depth != body_depth)
        return;
    if (body_len + n + 1 > body_cap) {
        body_cap = (body_len + n + 1) * 2;
        body_rec = realloc(body_rec, body_cap);
        if (!body_rec)
            acc_error(no_room);
    }
    memcpy(body_rec + body_len, body_start, n);
    body_len += n;
    body_start = cursor;
}

void lex_body_record_from(void)
{
    body_len = 0;
    body_start = cursor;
    body_depth = depth;
    body_on = 1;
}

/* The body kept, to the `}` that is the current token, which is left out:
 * a string of its own, or NULL where it ended at another level. */
char *lex_body_record_take(void)
{
    char *text;

    if (!body_on)
        return NULL;
    body_on = 0;
    if (depth != body_depth)
        return NULL;
    body_flush();
    if (!body_len || body_rec[body_len - 1] != '}')
        return NULL;
    text = malloc(body_len);
    if (!text)
        acc_error(no_room);
    memcpy(text, body_rec, body_len - 1);
    text[body_len - 1] = '\0';

    return text;
}
#endif

/* The text kept, from lex_record_from to the `]` that is the current
 * token, which is left out: its end, or NULL if it could not be kept. */
char *lex_record_take(void)
{
    char *end;

    if (!lex_record)
        return NULL;
    if (depth == record_depth)
        record_flush();
    else
        lex_record = NULL;
    end = lex_record;
    lex_record = NULL;
    if (!end)
        return NULL;

    if (end > record_first && end[-1] == ']')
        return end - 1;
    if (end - 1 > record_first && end[-1] == '>' && end[-2] == ':')
        return end - 2;

    return NULL;
}

/* The same, to the `;` that is the current token: a function's return
 * expression, kept to be read again where the function is called (see
 * inline_keep in inline.c). The end, or NULL. */
char *lex_record_take_semi(void)
{
    char *start = record_first, *end;

    if (!lex_record)
        return NULL;
    if (depth == record_depth)
        record_flush();
    else
        lex_record = NULL;
    end = lex_record;
    lex_record = NULL;

    return end && end > start && end[-1] == ';' ? end - 1 : NULL;
}

/* The same, to the `)` that is the current token: a for loop's step, kept
 * to be read again after the body (see for_statement in stmt.c). */
char *lex_record_take_paren(void)
{
    char *start = record_first, *end;

    if (!lex_record)
        return NULL;
    if (depth == record_depth)
        record_flush();
    else
        lex_record = NULL;
    end = lex_record;
    lex_record = NULL;

    return end && end > start && end[-1] == ')' ? end - 1 : NULL;
}

/* The current token, set aside and put back: a for loop reads its step
 * again after the body, when the token after the body is the current one,
 * and has it back afterwards. Every field a token has is here, so that one
 * added to it is added in one place. */
void lex_token_save(LexToken *t)
{
    t->tok = tok;
    t->val = tok_val;
    t->val_hi = tok_val_hi;
    t->fval = tok_fval;
    t->name = tok_name;
    t->line = tok_line;
    t->type = tok_type;
    t->prev_line = tok_prev_line;
    t->str = tok_str;
    t->str_len = tok_str_len;
    t->str_wide = tok_str_wide;
    t->str_escaped = tok_str_escaped;
    t->at = tok_at;
}

void lex_token_restore(const LexToken *t)
{
    tok = t->tok;
    tok_val = t->val;
    tok_val_hi = t->val_hi;
    tok_fval = t->fval;
    tok_name = t->name;
    tok_line = t->line;
    tok_type = t->type;
    tok_prev_line = t->prev_line;
    tok_str = t->str;
    tok_str_len = t->str_len;
    tok_str_wide = t->str_wide;
    tok_str_escaped = t->str_escaped;
    tok_at = t->at;
}

/* The kept text read as source, from after the current token: when it
 * runs out, what follows the current token is read as ever. */
void lex_push_record(char *text, int len)
{
    push_text(NAME_NONE, text, len);
}

/* The same over text this level frees when it is done with it, which is
 * not known to whoever pushed it: a for loop's step, read again after the
 * body, can be under the next loop's when that loop is the body. */
void lex_push_record_owned(char *text)
{
    push_owned_text(NAME_NONE, text);
}

void lex_pop_record(void)
{
    if (cursor == src_end)
        pop_source();
}

/* A window doubled, for a line longer than it: up to SRC_LINE_MAX, which is
 * far past the 4095 characters C99 asks a compiler to take in a line. */
#define SRC_LINE_MAX 65536

__attribute__((noinline))
static void window_grow(void)
{
    size_t used = (size_t) (src_raw - src);
    char *grown;

    if (src_cap >= SRC_LINE_MAX)
        acc_error_at(line, "a line longer than %d characters", SRC_LINE_MAX);
    grown = realloc(src, (size_t) src_cap * 2 + 1);
    if (!grown)
        acc_error_at(line, "out of memory for a line longer than %d "
                           "characters", src_cap);
    src = grown;
    src_cap *= 2;
    cursor = src;
    record_start = src;
    src_raw = src + used;
}

int refill(void)
{
    size_t keep, room, got;
    char *nl;

    if (!src_file)
        return 0;                       /* the whole file has been read */

    /* What is behind the cursor goes, and the tokens there with it: their
     * columns are no longer known. */
    tok_at = NULL;
    if (lex_record)
        record_flush();
#ifdef OPT_ACC
    if (body_on)
        body_flush();
#endif
    *src_end = src_held;                /* put back what the sentinel hid */
    keep = (size_t) (src_raw - cursor);
    if (keep && cursor != src)
        memmove(src, cursor, keep);
    cursor = src;
    record_start = cursor;              /* where a size being kept goes on */
#ifdef OPT_ACC
    body_start = cursor;
#endif
    src_raw = src + keep;

    /* Until the window is full or the file has no more. Reading until it is
     * full rather than taking one read's worth is what lets the trim below
     * stand: a short read that stopped mid-line would otherwise hand out
     * half a line and break the invariant the whole scheme rests on. */
    for (;;) {
        room = (size_t) src_cap - (size_t) (src_raw - src);
        while (room && (got = fread(src_raw, 1, room, src_file)) > 0) {
            dep_bytes(src_dep, src_raw, (int) got);
            src_raw += got;
            room -= got;
        }
        if (room) {                     /* that was the end of the file */
            fclose(src_file);
            src_file = NULL;
            nl = src_raw;
            break;
        }

        /* Whole lines only. A full window with no newline in it is a line
         * longer than the window, and the window grows to hold it: a file
         * with lines that long pays for them, and no other does. */
        for (nl = src_raw; nl > src && nl[-1] != '\n'; nl--)
            ;
        if (nl != src)
            break;
        window_grow();
    }

    /* Joined lines go before the end is settled: taking one out moves
     * everything behind it down, the end with it, and the byte the
     * sentinel is about to hide is whichever one ends up there. */
    if (lex_trigraphs)
        untrigraph(&nl);
    unsplice(&nl);
    src_held = src_file ? *nl : '\0';
    src_end = nl;
    *src_end = '\0';

    return *cursor != '\0';
}

/* A file's window started, with the one under it kept. The path is copied,
 * since it outlives whatever built it and every diagnostic from inside the
 * file names it. */
void push_source(const char *path)
{
    FILE *f;
    char *keep;

    if (depth == INCLUDE_MAX)
        acc_error_at(line, "includes nested more than %d deep", INCLUDE_MAX);

    f = fopen(path, "rb");
    if (!f)
        acc_error_at(line, "cannot open '%s'", path);

    open_files[depth].src = src;
    open_files[depth].cursor = cursor;
    open_files[depth].src_end = src_end;
    open_files[depth].src_raw = src_raw;
    open_files[depth].cap = src_cap;
    open_files[depth].src_held = src_held;
    open_files[depth].src_file = src_file;
    open_files[depth].src_path = src_path;
    open_files[depth].src_real = src_real;
    open_files[depth].line = line;
    open_files[depth].owned = src_owned;
    open_files[depth].macro = src_macro;
    open_files[depth].conds = nconds;
    open_files[depth].dep = src_dep;
    open_files[depth].use = src_use;

    /* The handle this level was reading through is closed while the file
     * below it runs, and opened again on the way back at the byte it had
     * reached. MOS gives a program few handles, and a chain of headers
     * would otherwise hold one for every file in it -- zap does the same,
     * for the same reason.
     *
     * Where to start again is what the handle had already read, which is
     * exactly what the window holds: everything from there on is still to
     * come. */
    if (src_file) {
        open_files[depth].at = ftell(src_file);
        fclose(src_file);
        src_file = NULL;
    } else {
        open_files[depth].at = -1;      /* read to its end already */
    }
    depth++;

    src = malloc(INCLUDE_CAP + 1);
    keep = malloc(strlen(path) + 1);
    if (!src || !keep)
        acc_error("out of memory for '%s'", path);
    strcpy(keep, path);

    src_file = f;
    src_path = keep;
    src_real = keep;
    src_dep = dep_add(path);
    src_cap = INCLUDE_CAP;
    src_owned = OWN_BUF | OWN_PATH;
    src_macro = NAME_NONE;
    src_use = NULL;
    cursor = src_end = src_raw = src;
    src_held = '\0';
    *src = '\0';
    line = 1;
    refill();
}

/* A window over text that is already in memory, which is what a macro's
 * expansion is. Nothing is allocated and nothing is read: the text is the
 * definition itself, and running off its end pops back to where the name
 * was used. The file and the line do not change, so a diagnostic from
 * inside an expansion points at the line that used the macro. */
void push_text(NameRef macro, char *text, int len)
{
    if (depth == INCLUDE_MAX)
        acc_error_at(line, "macros expanded more than %d deep", INCLUDE_MAX);

    open_files[depth].src = src;
    open_files[depth].cursor = cursor;
    open_files[depth].src_end = src_end;
    open_files[depth].src_raw = src_raw;
    open_files[depth].cap = src_cap;
    open_files[depth].src_held = src_held;
    open_files[depth].src_file = src_file;
    open_files[depth].src_path = src_path;
    open_files[depth].src_real = src_real;
    open_files[depth].line = line;
    open_files[depth].owned = src_owned;
    open_files[depth].macro = src_macro;
    open_files[depth].conds = nconds;
    open_files[depth].at = -1;          /* no handle of its own to set aside */
    open_files[depth].dep = src_dep;
    open_files[depth].use = src_use;
    depth++;
    src_dep = -1;                       /* text in memory, not a file */
    if (!src_use)
        src_use = tok_at ? tok_at : nowhere;

    src = NULL;
    src_file = NULL;
    src_owned = 0;
    src_macro = macro;
    cursor = text;
    src_end = src_raw = text + len;
    src_held = '\0';
}

/* The same, over text this level has to free when it ends: what a macro
 * with parameters builds, which is made for the one call and nothing
 * else. */
void push_owned_text(NameRef macro, char *text)
{
    push_text(macro, text, (int) strlen(text));
    src = text;                 /* what pop_source will free */
    src_owned = OWN_BUF;
}

/* Back to the file that included this one. Returns whether there was one. */
int pop_source(void)
{
    if (!depth)
        return 0;

    /* A conditional belongs to the file it is written in: one left open at
     * the end of a file is a mistake, and so is an `#endif` that would
     * close the caller's.
     *
     * Asked before anything is freed, because what is reported names this
     * file and reads the path this level is about to give up. */
    if (nconds > open_files[depth - 1].conds)
        acc_error_at(conds[open_files[depth - 1].conds].line,
                     "#if without #endif before the end of this file");
    if (nconds < open_files[depth - 1].conds)
        acc_error_at(line, "an #endif here would close an #if in the file "
                           "that included this one");

    if (src_file)
        fclose(src_file);
    if (src_owned & OWN_BUF)
        free(src);
    if (src_owned & OWN_PATH)
        free((char *) src_path);

    depth--;

    src = open_files[depth].src;
    cursor = open_files[depth].cursor;
    src_end = open_files[depth].src_end;
    src_raw = open_files[depth].src_raw;
    src_cap = open_files[depth].cap;
    src_held = open_files[depth].src_held;
    src_file = open_files[depth].src_file;
    src_path = open_files[depth].src_path;
    src_real = open_files[depth].src_real;
    line = open_files[depth].line;
    src_owned = open_files[depth].owned;
    src_macro = open_files[depth].macro;
    src_dep = open_files[depth].dep;
    src_use = open_files[depth].use;

    /* Out of the level a size being kept began at: it ended somewhere its
     * text cannot be had. */
    if (lex_record && depth < record_depth)
        lex_record = NULL;
#ifdef OPT_ACC
    if (body_on && depth < body_depth)
        body_on = 0;
#endif

    /* The handle set aside when this level was pushed, back where it was. */
    if (open_files[depth].at >= 0) {
        src_file = fopen(src_path, "rb");
        if (!src_file)
            acc_error_at(line, "cannot open '%s' again", src_path);
        if (fseek(src_file, open_files[depth].at, SEEK_SET) != 0)
            acc_error_at(line, "cannot go back to where '%s' was", src_path);
    }

    return 1;
}

const char *lex_prelude;

void lex_open(const char *path)
{
    src_file = fopen(path, "rb");
    if (!src_file)
        acc_error("cannot open '%s'", path);
    if (!src) {
        src = malloc(SRC_CAP + 1);
        if (!src)
            acc_error("out of memory for the source buffer");
    }

    src_path = src_real = path;
    src_dep = dep_add(path);
    src_cap = SRC_CAP;
    cursor = src_end = src_raw = src;
    src_held = '\0';
    *src = '\0';
    line = 1;
    depth = 0;
    refill();

    /* -include: a file read before the source, as if its first line were
     * an #include of it -- which is what gcc and clang mean by it. */
    if (lex_prelude)
        push_source(lex_prelude);

    /* GNU C's builtins that are functions, declared before the file as
     * gcc has them declared, and in the library: a declaration nothing
     * calls costs an object nothing. */
    {
        static char builtins[] = "int __builtin_ffs(int);";

        push_text(NAME_NONE, builtins, (int) sizeof builtins - 1);
    }
    next();
}

void lex_close(void)
{
    unsigned i;

    if (src_file) {
        fclose(src_file);
        src_file = NULL;
    }
    free(src);
    src = NULL;
    cursor = src_end = src_raw = NULL;

    /* The macros too: nothing after the source asks about them, and a link
     * that follows in the same run -- `acc prog.c` -- has the room. */
    for (i = 0; i != nmacro_slots; i++)
        if (macros[i]) {
            free(macros[i]->text);
            free(macros[i]->params);
        }
    free(macros);
    macros = NULL;
    nmacro_slots = nmacros = 0;
    while (macro_chunks) {
        void *chunk = macro_chunks;

        macro_chunks = *(void **) chunk;
        free(chunk);
    }
    macro_at = macro_end = NULL;
    macro_spare = NULL;
}

const char *lex_path(void) { return src_path; }
int lex_line(void)         { return line; }

/* Columns, for diagnostics.
 *
 * Nothing is counted as the source is read: a column is worked out only
 * when it is asked for, by walking back from the token to the start of its
 * line. The walk is always inside the window, because a window only ever
 * begins where a line does -- the start of the line is a newline behind the
 * token or the window's own start. Keeping a count instead would cost a
 * store at every newline, or every token, of a compile that has no error.
 *
 * A column counts bytes from 1, so a tab is one column, as are each of the
 * bytes of a UTF-8 character: it is the offset an editor needs to put its
 * cursor on the character. AED, which reads the column to move there, keeps
 * a tab as one character in its buffer and widens it only on the screen, so
 * a tab counted as one is the right place in the buffer. It is gcc's
 * -fdiagnostics-column-unit=byte, which was its default before gcc 11; gcc
 * now counts a tab to the next multiple of 8 unless told otherwise. 0 is a
 * column that is not known.
 *
 * Lines joined by a backslash are one line by the time they are read, so a
 * token on the second half of one is counted from the start of the first,
 * which is also the line it is reported on. */
static int column_in(const char *p, const char *start, const char *end)
{
    const char *q = p;

    if (!p || !start || p < start || p > end)
        return 0;                       /* not in this window, or gone */
    while (q > start && q[-1] != '\n')
        q--;

    return (int) (p - q) + 1;
}

/* The column of p, which is in the window being read or, in a macro's
 * text, in the file the macro was used in; in a macro's text the answer is
 * where the macro was used, the place the line is also taken from. p in
 * text that has since been popped is 0. So is the current token once the
 * window has moved past it; a token the parser kept for longer, across the
 * move -- once per 16 KB of source -- can come out in the wrong place, which
 * is what keeping a pointer rather than a count costs. */
int column_of(const char *p)
{
    const Source *f;
    int i;

    if (!src_use)
        return column_in(p, src, src_end);

    /* The file under the text: the level it was pushed over, which is the
     * first one down that was not text itself. */
    for (i = depth - 1; open_files[i].use; i--)
        ;
    f = &open_files[i];
    if (p >= f->src && p <= f->src_end)
        return column_in(p, f->src, f->src_end);

    return column_in(src_use, f->src, f->src_end);
}

/* The column the current token starts at. */
int lex_col(void)
{
    return column_of(tok_at);
}

/* The column of a token the parser kept tok_at of, to report an error
 * about it once it has read on past it. Kept is all that is needed: what it
 * costs is a copy of a pointer, and the column is only worked out if there
 * is an error. */
int lex_col_at(const char *at)
{
    return column_of(at);
}

/* The token before the one at `at`, which is in the window being read:
 * just past its end, with *lines the lines between the two. Null when that
 * cannot be read back.
 *
 * The lexer keeps where the current token starts and nothing about the one
 * before: keeping that was two more stores on the path every token takes,
 * and 1% of a compile, for the sake of an error. So it is found here by
 * walking back from the current token over what is not a token -- blanks,
 * comments, lines that are blank or a directive -- counting the lines it
 * crosses, which the caller holds against the line it knows the token
 * before is on. What it cannot read back over, such as a group an #if
 * skipped, comes out on the wrong line and is caught there. */
static const char *token_before(const char *at, int *lines)
{
    const char *p = at, *start = src, *q, *cut;
    int n = 0;

    if (!column_in(p, start, src_end))
        return NULL;

    for (;;) {
        while (p > start && p[-1] != '\n' && is_space(p[-1]))
            p--;
        if (p <= start + 1) {
            if (p == start)
                return NULL;
        } else if (p[-1] == '/' && p[-2] == '*') {
            /* A block comment ends here: back to where it opened. */
            for (p -= 2;; p--) {
                if (p < start + 2)
                    return NULL;
                if (p[-2] == '/' && p[-1] == '*')
                    break;
                if (p[-1] == '\n')
                    n++;
            }
            p -= 2;
            continue;
        }
        if (p[-1] != '\n')
            break;                      /* just past a token */

        /* The line before: its end, or where a `//` comment on it starts,
         * and nothing of it at all if it is a directive. */
        n++;
        cut = --p;
        for (q = p; q > start && q[-1] != '\n'; q--)
            ;
        while (q < cut && is_space(*q))
            q++;
        if (*q == '#') {
            p = q;
            continue;
        }
        for (; q < cut; q++) {
            if (*q == '"' || *q == '\'') {
                char quote = *q;

                while (++q < cut && *q != quote)
                    if (*q == '\\')
                        q++;
            } else if (*q == '/' && q[1] == '/') {
                cut = q;
            }
        }
        p = cut;
    }
    *lines = n;

    return p;
}

/* The column just past the token before the current one, where something
 * missing from the end of it would have gone: a ';' left off a line is
 * missing at the end of that line, not at the '}' on the next that shows
 * it. In a macro's text, where the macro was used. */
int lex_prev_col(void)
{
    const char *p;
    int lines;

    if (src_use)
        return column_of(tok_at);
    p = token_before(tok_at, &lines);
    if (!p || tok_line - lines != tok_prev_line)
        return 0;

    return column_in(p, src, src_end);
}

/* The column of the name just before the token at `at`, which was on
 * *line: the one a parser that has stepped past a name to see what follows
 * it is talking about. *line becomes the name's. 0, and *line left alone,
 * when the token before is not a name. In a macro's text it is where the
 * macro was used. */
int lex_name_col(const char *at, int *line)
{
    const char *p, *q;
    int lines;

    if (src_use)
        return column_of(at);
    p = token_before(at, &lines);
    if (!p)
        return 0;
    for (q = p; q > src && is_alnum((unsigned char) q[-1]); q--)
        ;
    if (q == p || !is_alpha((unsigned char) *q))
        return 0;
    *line -= lines;

    return column_in(q, src, src_end);
}

/* Whether the next character that is not white space is a colon. That is all
 * that tells a label from an expression statement -- `done:` and `done = 1;`
 * start with the same token -- and one character is enough to look at: at
 * the start of a statement a name followed by a colon can only be a label.
 * Nothing is consumed. */
/* The window ended in white space, so what follows has not been read yet.
 * Out of line, because this runs once per 16 KB and next_char runs at every
 * statement: a call in the middle of it costs the peek its registers. */
__attribute__((noinline))
static char next_char_refilled(void)
{
    while (refill()) {
        const char *p = cursor;

        while (is_space(*p))
            p++;
        if (*p)
            return *p;
    }

    return '\0';
}

static char next_char(void)
{
    const char *p = cursor;

    while (is_space(*p))
        p++;
    if (*p)
        return *p;

    return next_char_refilled();
}

int lex_colon_follows(void)
{
    return next_char() == ':';
}

int lex_rparen_follows(void)
{
    return next_char() == ')';
}

int lex_string_follows(void)
{
    return next_char() == '"';
}

int lex_rbrace_follows(void)
{
    return next_char() == '}';
}

int lex_rbracket_follows(void)
{
    return next_char() == ']';
}

/* Whether a name comes after the current token, which is what tells a
 * designator's `.` from a floating literal's: `.x` names a member and `.5`
 * is a number, and the difference is one character away. */
int lex_ident_follows(void)
{
    return is_alpha(next_char()) != 0;
}

__attribute__((noinline))
void window_more(void)
{
    for (;;) {
        if (!refill() && !pop_source())
            return;                     /* the outermost file has ended */
        skip_space();
        if (*cursor)
            return;
    }
}
