/*
 * Names and tokens.
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

/* ------------------------------------------------------------------ */
/* the name arena                                                      */

/* One block of characters holding every identifier in the program, each one
 * exactly once, NUL terminated. Identifiers are referred to by their offset
 * into it, so a name costs three bytes wherever it is mentioned. In front
 * of each name's text are four bytes: the file-scope symbol it stands for,
 * in the three next to the text (see name_global), and before those whether
 * it is a macro (see name_is_macro).
 *
 * Nothing is ever freed. A compiler runs once and exits; a free list would be
 * bytes spent to give memory back to a process that is about to end.
 *
 * Offset zero is never a name, so it can mean "none". */
char         *name_arena;       /* name_global reads it directly */
static size_t names_len, names_cap;

/* Open addressing over the arena: each slot is an offset, and a collision
 * walks forward. Sized as a power of two so the modulo is a mask, and grown by
 * doubling so the transient peak stays near 1.5x rather than 2x -- the
 * difference matters on a machine with no virtual memory. */
/* A slot is four bytes and not three, so that a slot's offset is a byte
 * pattern that can be assembled without arithmetic: see home, below. A
 * three-byte slot would make it a multiply, `call __imulu`. The probe runs for
 * every identifier in the program, which is what the compiler does most.
 *
 * An empty slot holds NAME_NONE. */
typedef union {
    NameRef       ref;
    unsigned char size[4];      /* four bytes on the host too, so the hash
                                 * test measures the table the Agon has */
} Bucket;

static Bucket  *buckets, *buckets_end;
static unsigned nbuckets, nnames;
static unsigned names_room;     /* names that fit before the table must grow */

/* Where a name's probe starts, as a byte offset into the table: a slot
 * number times the slot's size, masked to the table's size. It is assembled a byte at a
 * time, in memory, because each way of computing it in a register is a helper
 * call -- `high << 8` is `call __ishl`, masking a 24-bit value is `call
 * __iand`, and the slot times four is `call __ishl` again -- and those three
 * ran for every identifier in the program. Byte-wise they are an 8-bit `and`
 * and a store each, and the load that reads the offset back is one 24-bit
 * load. It has to go through memory: assembled in a local, the compiler
 * recognises the pattern and puts the shifts back.
 *
 * The bytes above the second stay zero, which caps the table at 64 KB: 16384
 * slots, and so 8191 distinct names in a program on the Agon. */
static unsigned char home[sizeof(unsigned)];
static unsigned char mask_lo, mask_hi;

#ifdef ACC_HASH_STATS
/* Counts probes so a test can tell a hash that spreads from one that does
 * not -- both give correct answers, and only one of them is fast. Compiled
 * only when the test asks for it: in the compiler this would be an increment
 * on the hottest loop there is. */
unsigned long name_probes, name_rehashes;
#define PROBE() (name_probes++)
#define REHASHED() (name_rehashes++)
#else
#define PROBE() ((void) 0)
#define REHASHED() ((void) 0)
#endif

/* Pearson hashing: one table lookup per character, and nothing else.
 *
 * FNV-1a was here, and on this target it was the most expensive thing in the
 * compiler. `h * 16777619` in a 32-bit accumulator is `call __lmulu`, and the
 * xor before it is `call __lxor` -- two calls into the long-arithmetic library
 * for every character of every identifier, including every `int` and every
 * `return`. The eZ80 has no multiplier wider than the 8-bit MLT and no 32-bit
 * anything, so there was no instruction for either half of it.
 *
 * Pearson replaces the arithmetic with `t[h ^ c]`, which is an 8-bit xor and
 * an indexed load from 256 bytes -- both real instructions. Two lanes, seeded
 * differently, give the 16 bits the bucket index needs; one lane's 8 would cap
 * the table at 256 names.
 *
 * The table is a permutation of 0..255, which is what makes the lanes mix at
 * all: any repeated value would collapse the two inputs that map to it. */
static const unsigned char pearson[256] = {
    203,  49,  73,  37, 250,  58, 130, 246,  54, 244,  89, 107,
    253, 227, 167,  57,   3, 231, 230,  80,   9, 235, 197, 113,
    251,  78,  22,  47, 193, 238, 176, 132, 109, 217, 225, 190,
     53, 236,  72, 120,  75, 242,  39, 205,  70,  20,  64,   5,
    166,  69,  15,  86, 100, 212, 177, 232,  41,  60,  23, 171,
      8, 108, 125,  82, 219, 165,  26,  28, 180,  25, 218,  74,
    209, 133, 102,  51, 207, 118,  92, 237, 153,  18, 211,  81,
     36, 127,  43, 175,  85,  52, 189, 131, 183,  17,  95, 192,
    213,  88,  16,   1, 110,  99, 119, 240, 186, 172, 170, 228,
    137, 162, 158, 145, 255, 214, 199,  65, 157,   0, 160, 245,
     91,  35, 181,  10,  59, 243, 194, 123, 134,  79, 249, 115,
    178,  34, 101, 144, 248, 239,  84, 141,  67, 185, 215, 140,
    184, 198, 149, 173, 187,  45, 223, 210, 117,  38, 161, 252,
     50, 229, 216, 139, 247,  98,  96,  90,  14,   2, 168, 196,
     42,  30,  56,  87, 201,  24, 142,   4,  33, 126, 148,  71,
     55, 159, 147,  13,  29, 234,  31, 179,  77, 150, 233, 114,
    200, 155, 104,  66, 224,  83,   6,  93,  12,  19,   7,  48,
    122, 182, 220, 254,  76, 152, 121,  46, 202, 206,  11, 124,
    191,  32, 103, 208, 135, 106, 226, 105, 204, 116, 163, 138,
     68, 111,  44,  63,  62, 136, 156, 143, 188, 128, 154, 146,
    169, 112, 151, 195, 221,  61, 129,  94,  27, 164, 222,  40,
    241,  97, 174,  21
};

/* Returns the bucket a name's probe starts at. */
static inline __attribute__((always_inline))
Bucket *name_home(const char *text, unsigned n)
{
    /* Sixteen bits from one table lookup per character rather than two.
     *
     * Two independent Pearson lanes is the obvious way to widen the hash and
     * it costs a lookup per character per lane. Only one lane has to be
     * that good. The other only has to carry what the first loses, and
     * doubling and adding the character does that for two adds against a
     * second indexed load; one more lookup at the end, once per name, mixes
     * it in.
     *
     * Cheaper and better, which was not the expected result. Measured over
     * the two thousand names the hash test interns, the two-lane version
     * needed 1.54 probes for a lookup and this one 1.46 when the table took
     * its index from the low sixteen bits -- the shift carries position
     * information that a second Pearson lane, being the same function of the
     * same bytes, largely repeats.
     */
    unsigned char low = (unsigned char) n;     /* so "ab" and "ba" differ */
    unsigned char high = 0;
    const unsigned char *at = (const unsigned char *) text, *end = at + n;
    unsigned offset;

    /* A pointer walked to the end rather than an index: the index was an add
     * to the base on every character, and the base and the count were read
     * back from the frame on every one -- a fifth of a compile went through
     * this loop. */
    while (at != end) {
        unsigned char c = *at++;

        low  = pearson[low ^ c];
        high = (unsigned char) (high + high + c);
    }

    /* The offset's low byte is a multiple of the slot's size, so whatever
     * goes there loses its bottom two bits, and neither byte can be `high`
     * as it stands: its low bits are the last character's and not much else.
     * Both bytes are therefore mixed ones, and together they still tell
     * apart every pair of bytes the loop could have ended on. Over the hash
     * test's names this probes 1.50 times a lookup; `low` and `high` as
     * they were, low byte first, probe 1.96, and a uniform hash at this load
     * 1.48. */
    high = (unsigned char) (pearson[high] ^ low);
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    offset = ((unsigned) (low & mask_hi) << 8) | (high & mask_lo);
#else
    home[0] = high & mask_lo;
    home[1] = low & mask_hi;
    memcpy(&offset, home, sizeof offset);
#endif

    return (Bucket *) ((char *) buckets + offset);
}

/* Split so that the test can be inlined into name_intern and the growth
 * cannot: the test is once per identifier, the growth is a dozen times in a
 * compile, and inlining both put a realloc's frame on the hot path. */
__attribute__((noinline))
static void names_realloc(size_t need)
{
    while (names_cap < names_len + need)
        names_cap = names_cap ? names_cap * 2 : 1024;
    name_arena = realloc(name_arena, names_cap);
    if (!name_arena)
        acc_error("out of memory for names");
}

static void names_grow(size_t need)
{
    if (names_len + need > names_cap)
        names_realloc(need);
}

static void buckets_rehash(unsigned newn)
{
    Bucket *old = buckets, *end = buckets_end, *from;
    unsigned mask = newn * sizeof *buckets - 1;

    if (newn * sizeof *buckets > 65536)
        acc_error("too many names: the limit is %u", 65536 / sizeof *buckets / 2 - 1);
    REHASHED();
    buckets = calloc(newn, sizeof *buckets);
    if (!buckets)
        acc_error("out of memory for the name table");
    buckets_end = buckets + newn;
    nbuckets = newn;
    names_room = newn / 2 - 1 - nnames;
    mask_lo = (unsigned char) (mask & ~(sizeof *buckets - 1));
    mask_hi = (unsigned char) (mask >> 8);
    for (from = old; from != end; from++) {
        NameRef ref = from->ref;
        Bucket *b;

        if (ref == NAME_NONE)
            continue;
        b = name_home(name_arena + ref, (unsigned) strlen(name_arena + ref));
        while (b->ref != NAME_NONE)
            if (++b == buckets_end)
                b = buckets;
        b->ref = ref;
    }
    free(old);
}

void name_init(void)
{
    names_cap = 1024;
    name_arena = malloc(names_cap);
    if (!name_arena)
        acc_error("out of memory for names");
    name_arena[0] = '\0';     /* so offset 0 is never a real name */
    names_len = 1;

    /* 1024 slots, which hold 511 names before the table grows: enough for a
     * program of a few hundred lines without a rehash, each of which moves
     * every name. Starting at 256, the two a benchmark input needed were 2%
     * of its compile. Four KB. */
    buckets_rehash(1024);
}

NameRef name_intern(const char *text, int len)
{
    Bucket *b = name_home(text, (unsigned) len);
    NameRef ref;

    PROBE();
    while ((ref = b->ref) != NAME_NONE) {
        /* Compare, then check the terminator, rather than measure first.
         * strlen walks the stored name to its end before the compare walks it
         * again, and it walked it even when the first character already said
         * the two were different. This runs 1.5 times per identifier in the
         * program.
         *
         * strncmp and not memcmp, which reads all len bytes of the stored
         * name whether or not it ends first -- off the end of the arena when
         * it is the last name there. strncmp stops at its terminator, so the
         * check after it only runs on a name at least len long. */
        if (strncmp(name_arena + ref, text, len) == 0
            && name_arena[ref + len] == '\0')
            return ref;
        if (++b == buckets_end)
            b = buckets;
        PROBE();
    }

    /* Kept under half full, since past that a linear probe starts walking.
     * Checked here, where the name is new, rather than on every lookup: the
     * growth moves every name, so the probe starts again. */
    if (names_room == 0) {
        buckets_rehash(nbuckets * 2);

        return name_intern(text, len);
    }

    /* Not a macro and no file-scope symbol yet, then the text. */
    names_grow(4 + len + 1);
    memset(name_arena + names_len, 0, 4);
    ref = (NameRef) names_len + 4;
    memcpy(name_arena + ref, text, len);
    name_arena[ref + len] = '\0';
    names_len = ref + len + 1;
    b->ref = ref;
    nnames++;
    names_room--;

    return ref;
}

const char *name_text(NameRef ref)
{
    return name_arena + ref;
}

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
/* The window the file named on the command line gets, which is what sets
 * the longest line a source may have.
 *
 * A file below it gets less. Four kilobytes is still far longer than any
 * line anybody writes, and eight of them is 32 KB where eight of the big
 * one would be 128 -- on a machine with 448 KB for everything, that is the
 * difference between a deep chain of headers being affordable and not.
 * zap sized its includes the same way and for the same reason. */
#define SRC_CAP     16384
#define INCLUDE_CAP 4096

static char *src;           /* src_cap + 1 bytes, the +1 for the sentinel */
static int   src_cap;       /* which of the two sizes this level has */
static char *cursor;
static char *src_end;       /* where the sentinel sits: the last line's end */
static char *src_raw;       /* one past the last byte read into the buffer */
static char  src_held;      /* the byte the sentinel replaced */
static FILE *src_file;      /* null once the file has been read to its end */
static const char *src_path;
/* The path it was opened under, which #line does not change: see once_add. */
static const char *src_real;
static int   line;

/* The files an `#include` is inside, innermost last.
 *
 * The window's state stays in the plain statics above rather than in a
 * field of whatever is on top of this stack: the lexer's inner loops read
 * `cursor` on every character, and reaching it through a pointer costs that
 * loop a load it does not have. Pushing a file puts the statics here and
 * starts new ones; popping puts them back.
 *
 * Eight deep, because each level holds a window of its own and the memory
 * is what there is least of: a program 128 KB of buffer deep in headers has
 * other problems. */
#define INCLUDE_MAX 8

typedef struct {
    char       *src, *cursor, *src_end, *src_raw;
    int         cap;
    long        at;             /* where the handle was when it was closed */
    char        src_held;
    FILE       *src_file;
    const char *src_path;
    const char *src_real;
    int         line;
    int         owned;          /* OWN_BUF and OWN_PATH, below */
    NameRef     macro;          /* the macro whose text this level is */
    int         conds;          /* conditionals open when this was pushed */
    int         dep;            /* which file of dep_files this is, or -1 */
} Source;

/* What this level allocated, and which macro it is the expansion of.
 *
 * A file owns both its window and the copy of its name; a macro with
 * parameters owns the text its expansion was built into, and has no name of
 * its own; a macro without them owns neither, since its text is the
 * definition in the table. The two are separate because a file's handle is
 * closed as soon as it has been read to its end, which is before the level
 * is popped -- so whether there is still a handle says nothing about
 * whether there is a name to free. */
#define OWN_BUF   1
#define OWN_PATH  2

static int     src_owned;
static NameRef src_macro;

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

static int dep_add(const char *path)
{
    char *keep;
    int i;

    /* Only an object records what it was made from, and only it pays for
     * the checksum: a compile straight to a program answers -1 here, and
     * every byte read after that goes through one compare. */
    if (!want_deps)
        return -1;

    /* A header that several others include is opened once for each of them,
     * and each time it reads the same bytes. One entry is enough, and it
     * keeps an object from growing with the shape of the include graph
     * rather than with the number of files in it.
     *
     * Its marks start again, because they are the marks of one reading of
     * the file: folding a second reading into the first would give a number
     * no reading of that file on its own could produce, and the build would
     * decide it had changed every time. */
    for (i = 0; i < ndeps; i++)
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

/* `n` bytes folded into the two running sums. */
static void marks_fold(unsigned *sump, unsigned *weightedp, const char *bytes,
                       int n)
{
    unsigned sum = *sump, weighted = *weightedp;
    int i;

    for (i = 0; i < n; i++) {
        sum = (sum + (unsigned char) bytes[i]) & 0xffffffu;
        weighted = (weighted + sum) & 0xffffffu;
    }
    *sump = sum;
    *weightedp = weighted;
}

/* The options that change what a compile reads: -D, -U and -I, in the
 * order they were given, folded into marks of their own as they arrive. An
 * object records them as one more dependency, after its files, with an
 * empty path -- a name no file can have -- so that the same source compiled
 * with another -D is compiled again rather than called up to date. */
static unsigned opt_size, opt_sum, opt_weighted;

static void opt_fold(char kind, const char *arg)
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

static Source open_files[INCLUDE_MAX];
static int    depth;

/* Where a `<...>` include is looked for, and a `"..."` one after the
 * directory of the file that asked for it. From -I, in the order given. */
#define INCLUDE_DIRS 8
static const char *include_dirs[INCLUDE_DIRS];
static int         ninclude_dirs;

void lex_add_include(const char *dir)
{
    if (ninclude_dirs == INCLUDE_DIRS)
        acc_error("more than %d -I directories", INCLUDE_DIRS);
    include_dirs[ninclude_dirs++] = dir;
    opt_fold('I', dir);
}

int      tok;
long     tok_val;
uint32_t tok_val_hi;
float    tok_fval;
typedef char float_is_four_bytes[sizeof(float) == 4 ? 1 : -1];
NameRef  tok_name;
int      tok_line;
Type     tok_type;
int      tok_prev_line;

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
static int ucn_at(const char *r)
{
    int n, i;

    if (r[0] != '\\' || (r[1] != 'u' && r[1] != 'U'))
        return 0;
    n = r[1] == 'u' ? 4 : 8;
    for (i = 0; i < n; i++)
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

static void push_text(NameRef macro, char *text, int len);

/* A parameter's array size, kept as the text it was written as for a
 * function's definition to read again when it is entered: see vla_texts in
 * parse.c. lex_record_from starts it after the current token, and the text
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

/* The kept text read as source, from after the current token: when it
 * runs out, what follows the current token is read as ever. */
void lex_push_record(char *text, int len)
{
    push_text(NAME_NONE, text, len);
}

static int refill(void)
{
    size_t keep, room, got;
    char *nl;

    if (!src_file)
        return 0;                       /* the whole file has been read */

    if (lex_record)
        record_flush();
    *src_end = src_held;                /* put back what the sentinel hid */
    keep = (size_t) (src_raw - cursor);
    if (keep && cursor != src)
        memmove(src, cursor, keep);
    cursor = src;
    record_start = cursor;              /* where a size being kept goes on */
    src_raw = src + keep;

    /* Until the window is full or the file has no more. Reading until it is
     * full rather than taking one read's worth is what lets the trim below
     * stand: a short read that stopped mid-line would otherwise hand out
     * half a line and break the invariant the whole scheme rests on. */
    room = (size_t) src_cap - keep;
    while (room && (got = fread(src_raw, 1, room, src_file)) > 0) {
        dep_bytes(src_dep, src_raw, (int) got);
        src_raw += got;
        room -= got;
    }
    if (room) {                         /* that was the end of the file */
        fclose(src_file);
        src_file = NULL;
        nl = src_raw;
    } else {
        /* Whole lines only. A full window with no newline in it is a line
         * longer than the window, which there is nowhere to put. */
        for (nl = src_raw; nl > src && nl[-1] != '\n'; nl--)
            ;
        if (nl == src)
            acc_error_at(line, "a line longer than %d characters", src_cap);
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

/* ------------------------------------------------------------------ */
/* macros                                                              */

/* What a name stands for, and the text it stands for. The text is kept as
 * it was written rather than as tokens: expanding a macro then costs no
 * more than pushing that text as another window and reading it, which is
 * the machinery `#include` already needed. Rescanning falls out of it --
 * the lexer simply carries on -- and so does a macro that uses a macro.
 *
 * Open addressing on the name's offset, which is already a good spread: the
 * names came out of a hash table. Every identifier in the program asks this
 * question, so the first thing asked is whether there are any macros at
 * all. */
/* The most parameters a macro may take, and so the most arguments a call
 * may pass. */
#define PARAMS_MAX 127          /* what C99 5.2.4.1 asks a compiler to take */

typedef struct {
    NameRef  name;              /* NAME_NONE in an empty slot */
    char    *text;
    NameRef *params;            /* null when the macro takes none */
    short    nparams;           /* -1 when it is a name and not a call */
    short    variadic;          /* whether the last parameter is `...` */
} Macro;

/* A buffer that grows, for the text an expansion is built into. On the
 * heap rather than in a frame: the Agon's stack and heap grow towards each
 * other out of one region, and a few hundred bytes of local buffer in
 * something that can call itself is how that ends badly. */
typedef struct {
    char *text;
    int   len, cap;
} Buf;

static void buf_put(Buf *b, const char *s, int n)
{
    if (b->len + n + 1 > b->cap) {
        int want = b->cap ? b->cap * 2 : 128;

        while (want < b->len + n + 1)
            want *= 2;
        b->text = realloc(b->text, (size_t) want);
        if (!b->text)
            acc_error("out of memory expanding a macro");
        b->cap = want;
    }
    memcpy(b->text + b->len, s, (size_t) n);
    b->len += n;
    b->text[b->len] = '\0';
}

static void buf_putc(Buf *b, int c)
{
    char ch = (char) c;

    buf_put(b, &ch, 1);
}

static Macro   *macros;
static unsigned nmacro_slots, nmacros;

/* Whether a name is a macro, kept in the byte four in front of its text.
 *
 * Every identifier in the program asks this, and for nearly all of them the
 * answer is no. It used to be asked of the macro table, whenever the program
 * had defined anything at all: a hash, a probe and a frame for every
 * identifier -- and the hash was a 24-bit AND and the probe an index into
 * entries that are not a power of two wide, a runtime call each. That was 4%
 * of compiling big.c, for names that were almost never macros. A byte in
 * front of the name is one load.
 *
 * The byte is two bits. NAME_MACRO is whether the name is a macro;
 * NAME_WIDE is on L alone, and says to look at what follows it, since L
 * and a quote are a wide literal and not a name at all. Either sends the
 * name to expand(), out of line, which is where the question is answered;
 * the lexer's own path is the one load either way. */
#define name_is_macro(ref)  (name_arena[(ref) - 4])
#define NAME_MACRO  1
#define NAME_WIDE   2
#define NAME_WEAK   4       /* #pragma weak, or a weak reference a link read */
#define NAME_STRONG 8       /* and a reference that was not weak, which ends it */

int name_weak(NameRef ref)
{
    return (name_is_macro(ref) & NAME_WEAK) != 0;
}

/* Weak is a name every reference to which is weak, whichever order the
 * references come in: once one that is not has been seen, it never is
 * again. `weak` 1 is a weak reference, 0 one that is not. */
void name_set_weak(NameRef ref, int weak)
{
    if (!weak)
        name_is_macro(ref) = (char) ((name_is_macro(ref) & ~NAME_WEAK)
                                     | NAME_STRONG);
    else if (!(name_is_macro(ref) & NAME_STRONG))
        name_is_macro(ref) |= NAME_WEAK;
}

static void macros_grow(void);

/* The slot a name belongs in, empty or not. */
static Macro *macro_slot(NameRef name)
{
    unsigned i = (name >> 2) & (nmacro_slots - 1);

    while (macros[i].name != NAME_NONE && macros[i].name != name)
        i = (i + 1) & (nmacro_slots - 1);

    return &macros[i];
}

/* The definition of a name, or null. */
static Macro *macro_find(NameRef name)
{
    Macro *m;

    if (!nmacros)
        return NULL;
    m = macro_slot(name);

    return m->name == NAME_NONE ? NULL : m;
}

static void macros_grow(void)
{
    Macro   *old = macros;
    unsigned n = nmacro_slots, i;

    nmacro_slots = n ? n * 2 : 64;
    macros = calloc(nmacro_slots, sizeof *macros);
    if (!macros)
        acc_error("out of memory for macros");
    for (i = 0; i < n; i++)
        if (old[i].name != NAME_NONE)
            *macro_slot(old[i].name) = old[i];
    free(old);
}

static void macro_define(NameRef name, const char *text, int len,
                         NameRef *params, int nparams, int variadic)
{
    Macro *m;
    char  *keep;

    /* Kept under half full: past that a linear probe starts walking. */
    if (!nmacro_slots || nmacros * 2 >= nmacro_slots)
        macros_grow();

    keep = malloc((size_t) len + 1);
    if (!keep)
        acc_error("out of memory for a macro");
    /* Asked, because `#define X` with nothing after it is a macro of no
     * bytes and there is nothing to point at for it -- and memcpy from a
     * null pointer is undefined even when it is told to copy nothing. */
    if (len)
        memcpy(keep, text, (size_t) len);
    keep[len] = '\0';

    m = macro_slot(name);
    if (m->name != NAME_NONE) {
        free(m->text);          /* defined again; C allows it if it matches */
        free(m->params);
    } else {
        nmacros++;
    }
    m->name = name;
    m->text = keep;
    m->params = params;
    m->nparams = (short) nparams;
    m->variadic = (short) variadic;
    name_is_macro(name) |= NAME_MACRO;
}

/* Forgotten, and the slot left usable. A tombstone is not needed: the run of
 * names that probed past this one is put back through the table. */
static void macro_undef(NameRef name)
{
    Macro   *m = macro_find(name);
    unsigned i;

    if (!m)
        return;
    free(m->text);
    free(m->params);
    m->name = NAME_NONE;
    m->text = NULL;
    m->params = NULL;
    nmacros--;
    name_is_macro(name) &= ~NAME_MACRO;

    /* Whatever follows in this run may have probed past the hole. */
    i = (unsigned) (m - macros);
    for (;;) {
        Macro moved;

        i = (i + 1) & (nmacro_slots - 1);
        if (macros[i].name == NAME_NONE)
            return;
        moved = macros[i];
        macros[i].name = NAME_NONE;
        macros[i].text = NULL;
        *macro_slot(moved.name) = moved;
    }
}

/* Whether a macro is already being expanded, which is what stops `#define A
 * A` from going on for ever. The standard paints the tokens rather than the
 * region, so a name that comes back out of its own expansion and is then
 * used again further along is not expanded here where it would be; the
 * difference needs a macro that expands to its own name, which no program
 * that means anything contains. */
static int expanding(NameRef name)
{
    int i;

    if (src_macro == name)
        return 1;
    for (i = 0; i < depth; i++)
        if (open_files[i].macro == name)
            return 1;

    return 0;
}

/* The conditionals open at this moment, and where each of them began. A
 * file has to close its own, so pushing and popping a source looks at
 * these; they are defined with the rest of the conditional machinery. */
typedef struct {
    unsigned char taken;
    unsigned char seen_else;
    int           line;
} Cond;

#define COND_MAX 16
static Cond conds[COND_MAX];
static int  nconds;

/* A file's window started, with the one under it kept. The path is copied,
 * since it outlives whatever built it and every diagnostic from inside the
 * file names it. */
static void push_source(const char *path)
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
static void push_text(NameRef macro, char *text, int len)
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
    depth++;
    src_dep = -1;                       /* text in memory, not a file */

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
static void push_owned_text(NameRef macro, char *text)
{
    push_text(macro, text, (int) strlen(text));
    src = text;                 /* what pop_source will free */
    src_owned = OWN_BUF;
}

/* Back to the file that included this one. Returns whether there was one. */
static int pop_source(void)
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

    /* Out of the level a size being kept began at: it ended somewhere its
     * text cannot be had. */
    if (lex_record && depth < record_depth)
        lex_record = NULL;

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
    next();
}

void lex_close(void)
{
    if (src_file) {
        fclose(src_file);
        src_file = NULL;
    }
    free(src);
    src = NULL;
    cursor = src_end = src_raw = NULL;
}

const char *lex_path(void) { return src_path; }
int lex_line(void)         { return line; }

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

/* A comment, with the cursor on its opening `/`. Out of line so that
 * skip_space, which is inlined into next(), carries nothing but the cursor
 * and the line count: the comment scan's own state, the line a block comment
 * opened on, was enough to give next() a stack frame -- on every token,
 * where comments are one call each. */
__attribute__((noinline))
static void skip_comment(void)
{
    if (cursor[1] == '/') {
        while (*cursor && *cursor != '\n')
            cursor++;

        return;
    }

    /* Where it opened, which is where the mistake is. Reporting the line the
     * scan gave up on points at the end of the file, which is the one place
     * the reader already knows is not the problem. */
    int opened = line;

    cursor += 2;
    for (;;) {
        while (*cursor && !(cursor[0] == '*' && cursor[1] == '/')) {
            if (*cursor == '\n')
                line++;
            cursor++;
        }
        if (*cursor)
            break;

        /* The window ended inside the comment. The two characters that close
         * one cannot have a newline between them and a window always ends
         * after a newline, so a `*<slash>` is never split by this. */
        if (!refill())
            acc_error_at(opened, "unterminated comment");
    }
    cursor += 2;
}

static void skip_space(void);

/* More of the file, and whatever white space and comments begin it. At the
 * end of the file the cursor is left on the sentinel, which is what next()
 * reads as the end.
 *
 * Out of line and off next()'s path: it runs once per 16 KB, and next() is
 * where a compile spends its time. */
/* ------------------------------------------------------------------ */
/* macros with parameters                                              */

/* Whether a '(' comes next, over space and over the end of whatever window
 * we are in: `f` and its `(` may come from different places, and a name
 * that is a function-like macro with no '(' after it is not a use of it at
 * all but an ordinary identifier.
 *
 * Only space is stepped over, which is never wrong to step over. */
static int paren_follows(void)
{
    for (;;) {
        const char *p = cursor;

        while (is_space(*p))
            p++;
        if (*p)
            return *p == '(';

        /* The window ended. Reading more is safe here: whatever comes back
         * is still ahead of the cursor, and nothing has been consumed. */
        if (!refill() && !pop_source())
            return 0;
    }
}

/* One character of the argument list, with the window moved on when it
 * runs out. Arguments can run over lines and, when one macro's expansion
 * ends inside another's argument list, over the end of a window. */
static int args_char(void)
{
    while (!*cursor) {
        if (!refill() && !pop_source())
            acc_error_at(line, "a macro's arguments are not closed");
    }
    if (*cursor == '\n')
        line++;

    return (unsigned char) *cursor++;
}

/* A call's arguments start with room for this many and the null after them,
 * and grow when a macro takes more: most take a few, and C99 asks for 127,
 * which as the first room would be zeroed on every call. */
#define ARGS_FIRST 17

/* Room for argument `argc` and the null after it. */
static char **args_room(char **argv, int argc, int *cap)
{
    int more;

    if (argc + 1 < *cap)
        return argv;
    more = *cap * 2;
    argv = realloc(argv, (size_t) more * sizeof *argv);
    if (!argv)
        acc_error("out of memory for a macro's arguments");
    memset(argv + *cap, 0, (size_t) (more - *cap) * sizeof *argv);
    *cap = more;

    return argv;
}

/* The arguments of a call, each as the text between the commas. Nesting is
 * counted so that a comma inside parentheses or brackets belongs to what it
 * is inside, and strings and character constants go over whole. */
static char **collect_args(Macro *m, int *out_argc)
{
    char **argv;
    Buf    arg;
    int    argc = 0, depth = 0, i, cap = ARGS_FIRST;

    argv = calloc(ARGS_FIRST, sizeof *argv);
    if (!argv)
        acc_error("out of memory for a macro's arguments");
    arg.text = NULL; arg.len = 0; arg.cap = 0;

    while (is_space(*cursor) || !*cursor) {
        if (!*cursor) {
            if (!refill() && !pop_source())
                acc_error_at(line, "a macro's arguments are not closed");

            continue;
        }
        if (*cursor == '\n')
            line++;
        cursor++;
    }
    cursor++;                           /* the '(' */

    for (;;) {
        int c = args_char();

        if (c == '(' || c == '[') {
            depth++;
        } else if (c == ')' && depth == 0) {
            break;
        } else if (c == ')' || c == ']') {
            depth--;
        } else if (c == ',' && depth == 0
                   && !(m->variadic && argc == m->nparams)) {
            /* The comma that ends one argument -- unless the variadic one
             * has started, where the commas are part of it. */
            if (argc == PARAMS_MAX)
                acc_error_at(line, "a macro takes at most %d arguments",
                             PARAMS_MAX);
            if (argc + 1 >= cap)
                argv = args_room(argv, argc, &cap);
            argv[argc++] = arg.text ? arg.text : strdup("");
            arg.text = NULL; arg.len = 0; arg.cap = 0;

            continue;
        } else if (c == '"' || c == '\'') {
            int quote = c;

            buf_putc(&arg, c);
            for (;;) {
                c = args_char();
                buf_putc(&arg, c);
                if (c == '\\') {
                    buf_putc(&arg, args_char());

                    continue;
                }
                if (c == quote)
                    break;
            }

            continue;
        }

        /* Runs of space become one, so that what a `#` makes of an argument
         * is the same however it was written. */
        if (is_space(c)) {
            if (arg.len && !is_space((unsigned char) arg.text[arg.len - 1]))
                buf_putc(&arg, ' ');

            continue;
        }
        buf_putc(&arg, c);
    }

    if (argc + 1 >= cap)
        argv = args_room(argv, argc, &cap);
    argv[argc++] = arg.text ? arg.text : strdup("");

    /* `f()` on a macro that takes nothing passes nothing, not one argument
     * that happens to be empty. On one that takes a parameter the same text
     * does pass an empty argument, which is why the macro decides. */
    if (m->nparams == 0 && argc == 1 && !*argv[0]) {
        free(argv[0]);
        argv[0] = NULL;
        argc = 0;
    }

    /* Blanks at either end are not part of an argument. */
    for (i = 0; i < argc; i++) {
        char *a = argv[i];
        int   n = (int) strlen(a);

        while (n && a[n - 1] == ' ')
            a[--n] = '\0';
        if (*a == ' ')
            memmove(a, a + 1, strlen(a));
    }

    *out_argc = argc;

    return argv;
}

static void free_args(char **argv, int argc)
{
    int i;

    for (i = 0; i < argc; i++)
        free(argv[i]);
    free(argv);
}

/* Which parameter a name is, or -1. */
static int param_index(Macro *m, const char *at, int len)
{
    int i;

    for (i = 0; i < m->nparams; i++) {
        const char *p = name_text(m->params[i]);

        if ((int) strlen(p) == len && !memcmp(p, at, (size_t) len))
            return i;
    }
    if (m->variadic && len == 11 && !memcmp(at, "__VA_ARGS__", 11))
        return m->nparams;

    return -1;
}

/* An argument with the macros in it expanded, which is what goes in
 * wherever the parameter is used plainly. The operands of `#` and `##` skip
 * this and go in as they were written.
 *
 * The expanding itself is the same walk an `#if` does over its condition,
 * and lives with it further down. */
static void expand_text_into(Buf *out, const char *text);

static void put_expanded(Buf *out, const char *text)
{
    expand_text_into(out, text);
}

/* An argument as the string it was written as, for `#`. */
static void put_stringified(Buf *out, const char *text)
{
    buf_putc(out, '"');
    while (*text) {
        if (*text == '"' || *text == '\\')
            buf_putc(out, '\\');
        buf_putc(out, *text++);
    }
    buf_putc(out, '"');
}

/* The body with the arguments put in: what the call becomes.
 *
 * `#p` is the argument as it was written, quoted. `a ## b` joins what is on
 * either side of it with nothing between, and the result is read as tokens
 * like anything else, since the buffer this builds is lexed from. */
/* '#' and '##', or their digraphs '%:' and '%:%:' (C99 6.4.6): how many
 * characters the one at p is spelled with, or 0. `#%:` is not a '##' but
 * two '#', as C tokenizes it. */
static int hash_at(const char *p)
{
    if (*p == '#')
        return 1;

    return p[0] == '%' && p[1] == ':' ? 2 : 0;
}

static int hashhash_at(const char *p)
{
    if (p[0] == '#' && p[1] == '#')
        return 2;

    return p[0] == '%' && p[1] == ':' && p[2] == '%' && p[3] == ':' ? 4 : 0;
}

/* Whether two characters side by side could be one token that they were
 * not: both punctuation that joins -- `-` and `-`, `<` and `=`. */
static int punct_pair(int a, int b)
{
    return a && b && strchr("+-*/%<>=!&|^.#:", a) && strchr("+-*/%<>=!&|^.#:", b);
}

static void buf_insert_space(Buf *b, int at)
{
    buf_putc(b, ' ');
    memmove(b->text + at + 1, b->text + at, (size_t) (b->len - at - 1));
    b->text[at] = ' ';
}

static char *build_expansion(Macro *m, char **argv, int argc)
{
    const char *p = m->text;
    Buf out;

    out.text = NULL; out.len = 0; out.cap = 0;

    while (*p) {
        const char *start;
        int idx, paste_before = 0, n;

        if (*p == '"' || *p == '\'') {          /* whole, names and all */
            int quote = *p;

            start = p++;
            while (*p && *p != quote) {
                if (*p == '\\' && p[1])
                    p++;
                p++;
            }
            if (*p)
                p++;
            buf_put(&out, start, (int) (p - start));

            continue;
        }

        if ((n = hashhash_at(p)) != 0) {
            /* Joined: whatever was put last stays, and the space before it
             * goes, so that the two sides end up next to each other. */
            while (out.len && out.text[out.len - 1] == ' ')
                out.text[--out.len] = '\0';
            p += n;
            while (*p == ' ' || *p == '\t')
                p++;
            paste_before = 1;
        }

        if (!paste_before && (n = hash_at(p)) != 0) {
            const char *q = p + n;

            while (*q == ' ' || *q == '\t')
                q++;
            start = q;
            while (is_alnum((unsigned char) *q))
                q++;
            idx = start == q ? -1 : param_index(m, start, (int) (q - start));
            if (idx < 0)
                acc_error_at(line, "'#' in a macro needs one of its "
                                   "parameters after it");
            put_stringified(&out, idx < argc ? argv[idx] : "");
            p = q;

            continue;
        }

        if (!is_alpha((unsigned char) *p)) {
            buf_putc(&out, *p++);

            continue;
        }

        start = p;
        while (is_alnum((unsigned char) *p))
            p++;

        /* `L'1'` and `L"x"` are one token, a wide literal, and its L is not
         * a name: a parameter called L is not put in its place. The quote
         * is copied whole on the next turn. */
        if (p - start == 1 && *start == 'L' && (*p == '\'' || *p == '"')) {
            buf_putc(&out, 'L');

            continue;
        }
        idx = param_index(m, start, (int) (p - start));
        if (idx < 0) {
            buf_put(&out, start, (int) (p - start));

            continue;
        }

        /* A parameter. Next to a `##` it goes in as it was written; on its
         * own it goes in expanded. */
        {
            const char *arg = idx < argc ? argv[idx] : "";
            const char *q = p;
            int at = out.len, paste_after;

            while (*q == ' ' || *q == '\t')
                q++;
            paste_after = hashhash_at(q) != 0;
            if (paste_before || paste_after)
                buf_put(&out, arg, (int) strlen(arg));
            else
                put_expanded(&out, arg);

            /* The argument's tokens and the body's are separate tokens,
             * and the text they are kept as must not let them run
             * together where they meet: `#define NEG(x) -x` with -1 is
             * `- -1` and not `--1`, and c-testsuite's 00202 pastes a `+`
             * to nothing just before the body's own `+`. A `##` joins on
             * purpose, so not there. */
            if (!paste_before && at > 0 && out.len > at
                && punct_pair(out.text[at - 1], out.text[at]))
                buf_insert_space(&out, at);
            if (!paste_after && out.len > 0 && punct_pair(out.text[out.len - 1], *p))
                buf_putc(&out, ' ');
        }
    }

    if (!out.text)
        out.text = strdup("");

    return out.text;
}

/* __DATE__ and __TIME__.
 *
 * C says both stand for when the translation unit was translated, and lets
 * an implementation that cannot find that out supply a valid date of its
 * own instead. What acc supplies is when acc itself was built, which is
 * the date the compiler that built it gave it -- and, when acc is built by
 * acc, the date this reports.
 *
 * Not the clock, on purpose. Reading it is not free: on the Agon it is a
 * call into MOS, and doing it up front cost more than compiling a small
 * file and made every compile take a different number of cycles from the
 * last, which is a compiler that cannot be measured. It also makes a
 * compile unrepeatable -- a program that names either would compile to
 * different bytes every second, where acc's own tests rest on the same
 * source giving the same image twice. A date that is fixed for a given acc
 * is worth more here than one that is right to the second.
 */
static const char date_text[] = __DATE__;
static const char time_text[] = __TIME__;

/* A name the compiler defines as what it stands for: the file being read,
 * the line the token is on, when the compile happened, or the 1 that says
 * this is a C compiler.
 *
 * The line is the one the name appears on, and inside a macro's expansion
 * that is the line the macro was used on, since an expansion does not move
 * the count. Out of line because it runs for a handful of names in a
 * program and next() runs for every one. */
static void pragma_operator(void);

__attribute__((noinline))
static void predefined(void)
{
    if (tok == TK_PRAGMA_OP) {
        pragma_operator();

        return;
    }

    /* C99 6.10.8. __STDC_HOSTED__ is 0: a hosted implementation is one with
     * all of the library, and until acc's has every header it is not one.
     * agondev says 1. */
    if (tok == TK_STDC_VERSION) {
        tok_val = 199901L;
        tok = TK_INT;
        tok_type = TY_LONG;
        tok_val_hi = 0;

        return;
    }
    if (tok == TK_LINE || tok == TK_STDC || tok == TK_STDC_HOSTED) {
        tok_val = tok == TK_LINE ? line : tok == TK_STDC ? 1 : 0;
        tok = TK_INT;
        tok_type = TY_INT;
        tok_val_hi = 0;

        return;
    }

    tok_str = tok == TK_FILE ? (src_path ? src_path : "")
            : tok == TK_DATE ? date_text : time_text;
    tok = TK_STRING;
    tok_str_len = (int) strlen(tok_str);
    tok_str_wide = 0;
}

/* A name put back as the text it stands for. Returns whether it was one:
 * when it is, the caller reads again and gets the first token of the
 * expansion.
 *
 * Out of line, since a name that is not a macro never reaches it and a name
 * that is pays a call either way. */
static void wide_literal(void);

__attribute__((noinline))
static int expand(NameRef name)
{
    Macro *m;

    /* L, and a quote after it: the literal is the token, and the answer is
     * that nothing was expanded -- next() has set tok to a name already,
     * and returns what this leaves there instead. Before the macro is
     * looked for, as L"x" is a string even where L is a macro. */
    if ((name_is_macro(name) & NAME_WIDE)
        && (*cursor == '"' || *cursor == '\'')) {
        wide_literal();

        return 0;
    }
    m = macro_find(name);

    if (!m || expanding(name))
        return 0;

    /* A macro with parameters is only used where a '(' follows it. Written
     * on its own the name is an ordinary identifier, which is what lets a
     * function and a macro over it share a name. */
    if (m->nparams >= 0) {
        char **argv;
        char  *text;
        int    argc;

        if (!paren_follows())
            return 0;

        argv = collect_args(m, &argc);
        if (argc < m->nparams || (argc > m->nparams && !m->variadic))
            acc_error_at(line, "'%s' takes %d argument%s, not %d",
                         name_text(name), m->nparams,
                         m->nparams == 1 ? "" : "s", argc);

        text = build_expansion(m, argv, argc);
        free_args(argv, argc);

        if (!*text) {
            free(text);

            return 1;           /* it came to nothing */
        }
        push_owned_text(name, text);

        return 1;
    }

    /* An empty definition expands to nothing at all, and a window over no
     * text is not worth pushing: saying it was expanded is enough, and the
     * caller reads straight past it. `#define EMPTY` and then `EMPTY;` is
     * a `;` on its own. */
    if (*m->text)
        push_text(name, m->text, (int) strlen(m->text));

    return 1;
}

/* ------------------------------------------------------------------ */
/* directives                                                          */

static void directive(void);
static void window_more(void);
static void predefined(void);

/* Declared here as well as where it is set: the two names the compiler
 * defines are keywords, and what asks whether a name is one of them comes
 * before the keywords do. A second tentative definition of the same object
 * is what C has for exactly this. */
static NameRef kw_limit;

/* Whether a name is one the compiler defines rather than the program:
 * `defined(__LINE__)` is true, and neither may be defined or undefined. */
static int predefined_name(NameRef name)
{
    return name < kw_limit
           && (unsigned char) name_arena[name - 3] >= TK_FILE
           && (unsigned char) name_arena[name - 3] <= TK_STDC_HOSTED;
}
static int  directive_name(char *buf, int cap);
static NameRef directive_target(const char *what);
static void skip_blanks(void);
static void rest_of_line(void);

/* Whether `at` is the first thing on its line, blanks aside. Only asked of
 * a '#', so the walk is off every path but that one.
 *
 * Always inlined. next() asks it, and when lex_digraph came to ask it too
 * clang made it a function of its own, and next() -- where it had been
 * inlined -- was given different registers for the loop that reads a name:
 * 1% of every compile, spent in a loop that does not call this at all. */
static inline __attribute__((always_inline))
int at_line_start(const char *at)
{
    while (at > src && (at[-1] == ' ' || at[-1] == '\t' || at[-1] == '\r'))
        at--;

    return at == src || at[-1] == '\n';
}

/* Space that is not a line's end: a directive lives on one line, so the
 * newline that ends it is what stops the scan rather than something to be
 * skipped over. */
/* Past blanks and the comments among them, to what really follows on the
 * line: a comment is one space by the time directives are read (C99
 * 5.1.1.2), so `#include <stdio.h> // printf` has nothing after its name.
 * A block comment that runs on past the line's end is left where it is. */
static const char *blanks_and_comments(const char *p)
{
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\r')
            p++;
        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n')
                p++;

            return p;
        }
        if (p[0] == '/' && p[1] == '*') {
            const char *end = p + 2;

            while (*end && *end != '\n' && !(end[0] == '*' && end[1] == '/'))
                end++;
            if (end[0] != '*')
                return p;
            p = end + 2;

            continue;
        }

        return p;
    }
}

static void skip_blanks(void)
{
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r')
        cursor++;
}

/* What is left of a directive's line, gathered rather than pointed at: a
 * macro's replacement text, or the condition of an #if.
 *
 * Two reasons it is copied. A line ending in a backslash is joined to the
 * one after it, which is how a macro of more than one line is written, and
 * the join has to take the backslash and the newline out of the text. And
 * the window may be refilled in the middle of a long one, which moves what
 * is left of it to the front of the buffer -- so a pointer to where the text
 * started would no longer be pointing at it.
 *
 * The buffer is grown and kept, since a file full of macros would otherwise
 * ask for one per macro. What is in it lasts until the next directive, so
 * macro_define copies what it is given. */
static char *body;
static int   body_cap;

static int logical_line(void)
{
    int len = 0;

    for (;;) {
        while (*cursor && *cursor != '\n') {
            int comment = cursor[0] == '/'
                          && (cursor[1] == '/' || cursor[1] == '*');

            if (len == body_cap) {
                body_cap = body_cap ? body_cap * 2 : 256;
                body = realloc(body, (size_t) body_cap);
                if (!body)
                    acc_error("out of memory for a macro");
            }

            /* A comment is one space. C takes them out before it reads
             * directives, so one that opens on a directive's line and closes
             * on a later one is still part of that directive -- and what
             * follows the close is still part of it too. Read as text, the
             * body kept the comment's opening and the lines under it were
             * compiled as though they were code, which is what a define with
             * a comment laid out over two lines did to acc's own gen.c. */
            if (comment) {
                skip_comment();
                body[len++] = ' ';

                continue;
            }
            body[len++] = *cursor++;
        }

        /* A backslash last on the line, bar the carriage return a file
         * written on another machine leaves there, joins this line to the
         * next.
         *
         * Both characters go, rather than becoming a space: C deletes them
         * before anything is tokenised, so a name split over two lines is
         * one name. `#define AB\<newline>CD 42` defines ABCD, and a space
         * in the join would have defined AB and left CD after it. */
        if (*cursor == '\n') {
            int end = len;

            if (end && body[end - 1] == '\r')
                end--;
            if (end && body[end - 1] == '\\') {
                len = end - 1;
                cursor++;
                line++;

                continue;
            }
        }
        if (*cursor || !refill())
            return len;
    }
}

/* The rest of the directive's line, thrown away. The newline is left for
 * skip_space, which is what counts the lines. A block comment is one space,
 * so one that runs past the line's end takes the directive with it to where
 * it closes (pragma-pack-3); a string is passed over whole, so that what
 * opens a comment inside one is not taken for one. */
static void rest_of_line(void)
{
    int in_comment = 0, quote = 0;

    for (;;) {
        while (*cursor && (*cursor != '\n' || in_comment)) {
            if (in_comment) {
                if (cursor[0] == '*' && cursor[1] == '/') {
                    in_comment = 0;
                    cursor++;
                } else if (*cursor == '\n') {
                    line++;
                }
            } else if (quote) {
                if (*cursor == '\\' && cursor[1] && cursor[1] != '\n')
                    cursor++;
                else if (*cursor == quote)
                    quote = 0;
            } else if (*cursor == '"' || *cursor == '\'') {
                quote = *cursor;
            } else if (cursor[0] == '/' && cursor[1] == '*') {
                in_comment = 1;
                cursor++;
            } else if (cursor[0] == '/' && cursor[1] == '/') {
                while (*cursor && *cursor != '\n')
                    cursor++;
                break;
            }
            cursor++;
        }
        if (*cursor || !refill())
            return;
    }
}

/* The name after the '#', into `buf`. Returns its length, which is zero for
 * a '#' on a line of its own -- which C allows and which does nothing. */
static int directive_name(char *buf, int cap)
{
    int n = 0;

    while (is_alnum((unsigned char) *cursor) && n < cap - 1)
        buf[n++] = *cursor++;
    buf[n] = '\0';

    return n;
}

/* The file named by an `#include`, into `buf`. Returns the quote character,
 * '"' or '<', so the caller knows where to look for it. */
static int include_name(char *buf, int cap)
{
    int close, n = 0;
    int open_ch = (unsigned char) *cursor;

    if (open_ch != '"' && open_ch != '<')
        acc_error_at(line, "an #include needs \"a file\" or <a file>");
    close = open_ch == '"' ? '"' : '>';
    cursor++;

    while (*cursor && *cursor != close && *cursor != '\n') {
        if (n == cap - 1)
            acc_error_at(line, "the file name in an #include is too long");
        buf[n++] = *cursor++;
    }
    if (*cursor != close)
        acc_error_at(line, "the file name in an #include is not closed");
    cursor++;
    buf[n] = '\0';
    if (!n)
        acc_error_at(line, "an #include needs a file name");

    return open_ch;
}

/* dir + "/" + name, or just name when there is no directory. Returns 0 when
 * the two together do not fit. */
static int join_path(char *out, int cap, const char *dir, int dirlen,
                     const char *name)
{
    int n = (int) strlen(name);

    if (dirlen + (dirlen ? 1 : 0) + n >= cap)
        return 0;
    if (dirlen) {
        memcpy(out, dir, (size_t) dirlen);
        out[dirlen] = '/';
        strcpy(out + dirlen + 1, name);
    } else {
        strcpy(out, name);
    }

    return 1;
}

static int readable(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (!f)
        return 0;
    fclose(f);

    return 1;
}

/* Where a named file is, or null. A quoted name is looked for beside the
 * file that asked for it first, which is what makes a header next to its
 * source findable without an -I; an angled one is not. Both then go through
 * the -I directories in the order they were given. */
static const char *find_include(int how, const char *name, char *buf, int cap)
{
    int i;

    /* An absolute path, or one the working directory already answers. */
    if (name[0] == '/')
        return readable(name) ? name : NULL;

    if (how == '"') {
        const char *slash = NULL, *p;

        for (p = src_path; *p; p++)
            if (*p == '/')
                slash = p;
        if (join_path(buf, cap, src_path, slash ? (int) (slash - src_path) : 0,
                      name)
            && readable(buf))
            return buf;
    }

    for (i = 0; i < ninclude_dirs; i++)
        if (join_path(buf, cap, include_dirs[i],
                      (int) strlen(include_dirs[i]), name)
            && readable(buf))
            return buf;

    return NULL;
}

/* The files a `#pragma once` has been seen in, by the path they were opened
 * under.
 *
 * Not C, but every compiler has it and headers are written expecting it: a
 * header with neither this nor a guard is read again on every include, and
 * what that does to a program is redefine everything in it. The path is the
 * one the include resolved to rather than whatever `#line` has since called
 * the file, so that a generated header cannot rename itself out of the set.
 *
 * Two names for the same file are two entries, which is the answer every
 * compiler that compares paths gives. A header found down two different -I
 * directories is read twice, and a guard is what covers that; the two
 * together are the usual belt and braces. */
static char **once_seen;
static int    nonce, once_cap;

static int once_has(const char *path)
{
    int i;

    for (i = 0; i < nonce; i++)
        if (strcmp(once_seen[i], path) == 0)
            return 1;

    return 0;
}

static void once_add(const char *path)
{
    char *keep;

    if (!path || once_has(path))
        return;
    if (nonce == once_cap) {
        once_cap = once_cap ? once_cap * 2 : 8;
        once_seen = realloc(once_seen, (size_t) once_cap * sizeof *once_seen);
        if (!once_seen)
            acc_error("out of memory for the files read once");
    }
    keep = malloc(strlen(path) + 1);
    if (!keep)
        acc_error("out of memory for the files read once");
    strcpy(keep, path);
    once_seen[nonce++] = keep;
}

static void do_include(void)
{
    char name[128], buf[256];
    const char *found;
    int how;

    skip_blanks();
    how = include_name(name, (int) sizeof name);
    cursor = (char *) blanks_and_comments(cursor);
    if (*cursor && *cursor != '\n')
        acc_error_at(line, "an #include takes one file name and nothing else");

    found = find_include(how, name, buf, (int) sizeof buf);
    if (!found)
        acc_error_at(line, "cannot find '%s'", name);

    /* The rest of this file's line goes first: the push swaps the window
     * out, and what is left of the line has to be behind the cursor when it
     * comes back. */
    rest_of_line();
    if (once_has(found))
        return;                         /* it said `#pragma once` already */
    push_source(found);
}

/* ------------------------------------------------------------------ */
/* conditionals                                                        */

/* ------------------------------------------------------------------ */
/* #if                                                                 */

/* The line an `#if` is given, with the macros in it put back as their text
 * and `defined X` answered, built into a buffer and then read as an
 * expression.
 *
 * Done on the text rather than through the lexer because the lexer is in
 * the middle of a directive: asking it for the next token would run it off
 * the end of the line, and putting it back afterwards is more machinery
 * than copying the line is. */
static char *if_text;
static int   if_len, if_cap;

static void if_put(const char *text, int len)
{
    if (if_len + len > if_cap) {
        int want = if_cap ? if_cap * 2 : 512;

        while (want < if_len + len)
            want *= 2;
        if_text = realloc(if_text, (size_t) want);
        if (!if_text)
            acc_error("out of memory expanding a macro");
        if_cap = want;
    }
    memcpy(if_text + if_len, text, (size_t) len);
    if_len += len;
}

/* The names being expanded, so that one does not expand inside itself. */
static NameRef if_active[INCLUDE_MAX];
static int     if_depth;

static void if_expand(const char *text, int len);

/* The arguments of a call written in text rather than read from the file:
 * what a macro's expansion holds, and what an #if's condition holds. The
 * nesting rules are the same as at a call the lexer reads. */
static char **collect_args_text(Macro *m, const char **at, const char *end,
                                int *out_argc)
{
    const char *p = *at;
    char **argv;
    Buf    arg;
    int    argc = 0, depth = 0, i, cap = ARGS_FIRST;

    argv = calloc(ARGS_FIRST, sizeof *argv);
    if (!argv)
        acc_error("out of memory for a macro's arguments");
    arg.text = NULL; arg.len = 0; arg.cap = 0;

    while (p < end && is_space((unsigned char) *p))
        p++;
    p++;                                /* the '(' */

    for (;;) {
        int c;

        if (p == end)
            acc_error_at(line, "a macro's arguments are not closed");
        c = (unsigned char) *p++;

        if (c == '(' || c == '[') {
            depth++;
        } else if (c == ')' && depth == 0) {
            break;
        } else if (c == ')' || c == ']') {
            depth--;
        } else if (c == ',' && depth == 0
                   && !(m->variadic && argc == m->nparams)) {
            if (argc == PARAMS_MAX)
                acc_error_at(line, "a macro takes at most %d arguments",
                             PARAMS_MAX);
            if (argc + 1 >= cap)
                argv = args_room(argv, argc, &cap);
            argv[argc++] = arg.text ? arg.text : strdup("");
            arg.text = NULL; arg.len = 0; arg.cap = 0;

            continue;
        } else if (c == '"' || c == '\'') {
            int quote = c;

            buf_putc(&arg, c);
            while (p < end) {
                c = (unsigned char) *p++;
                buf_putc(&arg, c);
                if (c == '\\' && p < end) {
                    buf_putc(&arg, (unsigned char) *p++);

                    continue;
                }
                if (c == quote)
                    break;
            }

            continue;
        }

        if (is_space(c)) {
            if (arg.len && !is_space((unsigned char) arg.text[arg.len - 1]))
                buf_putc(&arg, ' ');

            continue;
        }
        buf_putc(&arg, c);
    }

    if (argc + 1 >= cap)
        argv = args_room(argv, argc, &cap);
    argv[argc++] = arg.text ? arg.text : strdup("");
    if (m->nparams == 0 && argc == 1 && !*argv[0]) {
        free(argv[0]);                  /* as above: nothing, not one empty */
        argv[0] = NULL;
        argc = 0;
    }
    for (i = 0; i < argc; i++) {
        char *a = argv[i];
        int   n = (int) strlen(a);

        while (n && a[n - 1] == ' ')
            a[--n] = '\0';
        if (*a == ' ')
            memmove(a, a + 1, strlen(a));
    }

    *at = p;
    *out_argc = argc;

    return argv;
}

/* A name in text being expanded: put back as what it stands for, or left
 * alone. `at` is the name; `after` is what follows it, which is what says
 * whether a macro with parameters is being used or merely mentioned.
 * Returns where to carry on from. */
static const char *if_name(const char *at, int len, const char *after,
                           const char *end)
{
    NameRef name = name_intern(at, len);
    Macro  *m;
    int     i;

    /* One of the names the compiler defines. They are keywords rather than
     * macros, so the table below has never heard of them, and without this
     * `#if __STDC__` would read as the zero an undefined name stands for.
     * The two that are strings are put back as strings, which is not
     * something an #if can do anything with -- and saying so where the
     * expression is read is a better answer than a silent zero. */
    if (predefined_name(name)) {
        char num[24];

        switch ((unsigned char) name_arena[name - 3]) {
        case TK_LINE:
            snprintf(num, sizeof num, "%d", line);
            if_put(num, (int) strlen(num));
            break;
        case TK_STDC:
            if_put("1", 1);
            break;
        case TK_STDC_VERSION:
            if_put("199901L", 7);
            break;
        case TK_STDC_HOSTED:
            if_put("0", 1);
            break;
        case TK_DATE: if_put("\"", 1);
                      if_put(date_text, (int) strlen(date_text));
                      if_put("\"", 1); break;
        case TK_TIME: if_put("\"", 1);
                      if_put(time_text, (int) strlen(time_text));
                      if_put("\"", 1); break;
        default: {
            const char *f = src_path ? src_path : "";

            if_put("\"", 1);
            if_put(f, (int) strlen(f));
            if_put("\"", 1);
            break;
        }
        }

        return after;
    }

    for (i = 0; i < if_depth; i++)
        if (if_active[i] == name) {
            if_put(at, len);            /* already being expanded */

            return after;
        }

    m = macro_find(name);
    if (!m) {
        if_put(at, len);

        return after;
    }
    if (if_depth == INCLUDE_MAX)
        acc_error_at(line, "macros expanded more than %d deep", INCLUDE_MAX);

    if (m->nparams >= 0) {
        const char *p = after;
        char **argv;
        char  *built;
        int    argc;

        while (p < end && is_space((unsigned char) *p))
            p++;
        if (p == end || *p != '(') {
            if_put(at, len);            /* mentioned, not used */

            return after;
        }

        p = after;
        argv = collect_args_text(m, &p, end, &argc);
        if (argc < m->nparams || (argc > m->nparams && !m->variadic))
            acc_error_at(line, "'%s' takes %d argument%s, not %d",
                         name_text(name), m->nparams,
                         m->nparams == 1 ? "" : "s", argc);
        built = build_expansion(m, argv, argc);
        free_args(argv, argc);

        if_active[if_depth++] = name;
        if_expand(built, (int) strlen(built));
        if_depth--;
        free(built);

        return p;
    }

    if_active[if_depth++] = name;
    if_expand(m->text, (int) strlen(m->text));
    if_depth--;

    return after;
}

/* `defined X` and `defined(X)`, which are answered before anything is
 * expanded: the name is the one that was written, not what it stands for. */
static const char *if_defined(const char *p, const char *end)
{
    const char *start;
    int parens = 0;

    while (p < end && (*p == ' ' || *p == '\t'))
        p++;
    if (p < end && *p == '(') {
        parens = 1;
        p++;
        while (p < end && (*p == ' ' || *p == '\t'))
            p++;
    }
    start = p;
    while (p < end && is_alnum((unsigned char) *p))
        p++;
    if (p == start)
        acc_error_at(line, "'defined' needs a name");

    {
        NameRef name = name_intern(start, (int) (p - start));

        if_put(macro_find(name) || predefined_name(name) ? "1" : "0", 1);
    }

    if (parens) {
        while (p < end && (*p == ' ' || *p == '\t'))
            p++;
        if (p == end || *p != ')')
            acc_error_at(line, "'defined(' needs a ')'");
        p++;
    }

    return p;
}

/* One stretch of text into the buffer, with its names dealt with. */
static void if_expand(const char *text, int len)
{
    const char *p = text, *end = text + len;

    while (p < end) {
        const char *start;

        if (!is_alpha((unsigned char) *p)) {
            /* A string or a character constant goes over whole, so that a
             * name inside one is not a name. */
            if (*p == '"' || *p == '\'') {
                int quote = *p;

                start = p++;
                while (p < end && *p != quote) {
                    if (*p == '\\' && p + 1 < end)
                        p++;
                    p++;
                }
                if (p < end)
                    p++;
                if_put(start, (int) (p - start));

                continue;
            }
            if_put(p, 1);
            p++;

            continue;
        }


        start = p;
        while (p < end && is_alnum((unsigned char) *p))
            p++;

        if (p - start == 7 && !memcmp(start, "defined", 7)) {
            p = if_defined(p, end);

            continue;
        }
        p = if_name(start, (int) (p - start), p, end);
    }
}

/* ------------------------------------------------------------------ */

/* The expression, read from the buffer the expansion built.
 *
 * C says an #if is worked out in the widest integers there are, which here
 * is a long long, and that a name left over is zero -- `#if FOO` on a name
 * nobody defined is false rather than a mistake. */
static const char *ep;

static long long if_ternary(void);

/* A hex digit's value, or -1. float.c has one of these, but it is static
 * there and this is the only other place that wants one. */
static int if_digit(int c, int base)
{
    int v;

    if (c >= '0' && c <= '9')
        v = c - '0';
    else if (c >= 'a' && c <= 'f')
        v = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
        v = c - 'A' + 10;
    else
        return -1;

    return v < base ? v : -1;
}

static void ep_blanks(void)
{
    while (*ep == ' ' || *ep == '\t' || *ep == '\r')
        ep++;
}

/* Whether the operator `op` is next, stepping over it if so. Two-character
 * operators are asked for before the one-character ones they start with. */
static int ep_op(const char *op)
{
    int n = (int) strlen(op);

    ep_blanks();
    if (memcmp(ep, op, (size_t) n) != 0)
        return 0;

    /* `&` must not match the front of `&&`, nor `<` that of `<<` or `<=`. */
    if (n == 1 && (ep[1] == ep[0] || ep[1] == '=')
        && (*op == '&' || *op == '|' || *op == '<' || *op == '>'
            || *op == '=' || *op == '!'))
        return 0;
    ep += n;

    return 1;
}

/* How deep inside an operand C says is not evaluated: the right of a `&&`
 * whose left was false, of a `||` whose left was true, and the branch of a
 * `?:` that was not taken. The text still has to be read, because it has to
 * be stepped over, but nothing in it may be complained about -- `#if 0 &&
 * 1/0` is a well-formed condition that is false, and a program that guards
 * a division that way is entitled to have it not diagnosed. */
static int if_dead;

static long long if_primary(void)
{
    long long v;

    ep_blanks();
    if (*ep == '(') {
        ep++;
        v = if_ternary();
        ep_blanks();
        if (*ep != ')')
            acc_error_at(line, "the expression in an #if needs a ')'");
        ep++;

        return v;
    }
    if (*ep == '\'' || (ep[0] == 'L' && ep[1] == '\'')) {
        /* A character constant, whose value is what the byte is. The
         * escapes are the ones the lexer proper takes, numbers and all:
         * `#if '\\x41' == 'A'` is a condition a program may reasonably
         * write, and one that reads only `\\0` cannot answer it.
         *
         * An L in front makes it wide, as in the program: a wchar_t, which
         * is a short, an escape as wide as that, and a character of the
         * source read as the UTF-8 it is. */
        int wide = *ep == 'L';

        ep += wide + 1;
        if (*ep == '\\') {
            ep++;
            switch (*ep) {
            case 'n':  v = '\n'; ep++; break;
            case 't':  v = '\t'; ep++; break;
            case 'r':  v = '\r'; ep++; break;
            case 'a':  v = '\a'; ep++; break;
            case 'b':  v = '\b'; ep++; break;
            case 'f':  v = '\f'; ep++; break;
            case 'v':  v = '\v'; ep++; break;
            case 'e':  v = 27;   ep++; break;
            case '\\': v = '\\'; ep++; break;
            case '\'': v = '\''; ep++; break;
            case '"':  v = '"';  ep++; break;
            case '?':  v = '?';  ep++; break;
            case 'x': {
                int d;

                ep++;
                if (if_digit((unsigned char) *ep, 16) < 0)
                    acc_error_at(line, "'\\x' in an #if needs a hex digit");
                v = 0;
                while ((d = if_digit((unsigned char) *ep, 16)) >= 0) {
                    v = v * 16 + d;
                    ep++;
                }
                v = wide ? (short) v : (signed char) v;
                break;
            }
            default:
                if (if_digit((unsigned char) *ep, 8) >= 0) {
                    int n = 0, d;

                    v = 0;
                    while (n < 3
                           && (d = if_digit((unsigned char) *ep, 8)) >= 0) {
                        v = v * 8 + d;
                        ep++;
                        n++;
                    }
                    v = wide ? (short) v : (signed char) v;
                } else {
                    v = (unsigned char) *ep;
                    ep++;
                }
                break;
            }
        } else if (wide && (unsigned char) *ep >= 0xc0) {
            int c = (unsigned char) *ep++;
            int more = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : 1;

            v = c & (0x3f >> more);
            while (more-- && ((unsigned char) *ep & 0xc0) == 0x80)
                v = v << 6 | (*ep++ & 0x3f);
            if (v > 0xffff)
                acc_error_at(line, "a wide character constant past 0xffff "
                                   "does not fit in a wchar_t");
            v = (short) v;
        } else {
            v = wide ? (unsigned char) *ep++ : (signed char) *ep++;
        }
        if (*ep != '\'')
            acc_error_at(line, "a character constant in an #if is not closed");
        ep++;

        return v;
    }
    if (is_digit((unsigned char) *ep)) {
        int base = 10;

        if (ep[0] == '0' && (ep[1] == 'x' || ep[1] == 'X')) {
            base = 16;
            ep += 2;
        } else if (ep[0] == '0') {
            base = 8;
        }
        v = 0;
        for (;;) {
            int d = if_digit((unsigned char) *ep, base);

            if (d < 0)
                break;
            v = v * base + d;
            ep++;
        }
        while (*ep == 'u' || *ep == 'U' || *ep == 'l' || *ep == 'L')
            ep++;

        return v;
    }
    if (is_alpha((unsigned char) *ep)) {
        /* A name nothing defined, which C says is zero. */
        while (is_alnum((unsigned char) *ep))
            ep++;

        return 0;
    }
    if (!*ep)
        acc_error_at(line, "the expression in an #if stops too soon");

    acc_error_at(line, "'%c' has no meaning in an #if", *ep);

    return 0;
}

static long long if_unary(void)
{
    ep_blanks();
    if (ep_op("!"))
        return !if_unary();
    if (ep_op("~"))
        return ~if_unary();
    if (ep_op("-"))
        return -if_unary();
    if (ep_op("+"))
        return if_unary();

    return if_primary();
}

static long long if_mul(void)
{
    long long v = if_unary();

    for (;;) {
        long long r;

        if (ep_op("*")) {
            v = v * if_unary();
        } else if (ep_op("/")) {
            r = if_unary();
            if (!r) {
                if (!if_dead)
                    acc_error_at(line, "a division by zero in an #if");
                r = 1;
            }
            v = v / r;
        } else if (ep_op("%")) {
            r = if_unary();
            if (!r) {
                if (!if_dead)
                    acc_error_at(line, "a division by zero in an #if");
                r = 1;
            }
            v = v % r;
        } else {
            return v;
        }
    }
}

static long long if_add(void)
{
    long long v = if_mul();

    for (;;) {
        if (ep_op("+"))
            v = v + if_mul();
        else if (ep_op("-"))
            v = v - if_mul();
        else
            return v;
    }
}

static long long if_shift(void)
{
    long long v = if_add();

    for (;;) {
        if (ep_op("<<"))
            v = v << if_add();
        else if (ep_op(">>"))
            v = v >> if_add();
        else
            return v;
    }
}

static long long if_relational(void)
{
    long long v = if_shift();

    for (;;) {
        if (ep_op("<="))
            v = v <= if_shift();
        else if (ep_op(">="))
            v = v >= if_shift();
        else if (ep_op("<"))
            v = v < if_shift();
        else if (ep_op(">"))
            v = v > if_shift();
        else
            return v;
    }
}

static long long if_equality(void)
{
    long long v = if_relational();

    for (;;) {
        if (ep_op("=="))
            v = v == if_relational();
        else if (ep_op("!="))
            v = v != if_relational();
        else
            return v;
    }
}

static long long if_bitand(void)
{
    long long v = if_equality();

    while (ep_op("&"))
        v = v & if_equality();

    return v;
}

static long long if_bitxor(void)
{
    long long v = if_bitand();

    while (ep_op("^"))
        v = v ^ if_bitand();

    return v;
}

static long long if_bitor(void)
{
    long long v = if_bitxor();

    while (ep_op("|"))
        v = v | if_bitxor();

    return v;
}

static long long if_and(void)
{
    long long v = if_bitor();

    /* The right is read whether or not the left decided it -- it is text
     * that has to be stepped over either way -- but it is not evaluated
     * when the left settled the answer. See if_dead. */
    while (ep_op("&&")) {
        long long r;

        if_dead += !v;
        r = if_bitor();
        if_dead -= !v;
        v = v && r;
    }

    return v;
}

static long long if_or(void)
{
    long long v = if_and();

    while (ep_op("||")) {
        long long r;

        if_dead += !!v;
        r = if_and();
        if_dead -= !!v;
        v = v || r;
    }

    return v;
}

static long long if_ternary(void)
{
    long long v = if_or();

    if (!ep_op("?"))
        return v;
    {
        long long yes, no;

        if_dead += !v;
        yes = if_ternary();
        if_dead -= !v;
        if (!ep_op(":"))
            acc_error_at(line, "the '?' in an #if needs a ':'");
        if_dead += !!v;
        no = if_ternary();
        if_dead -= !!v;

        return v ? yes : no;
    }
}

/* Text with the macros in it put back, into a buffer. The same walk an
 * #if's condition goes through, and what a macro's argument goes through
 * before it is put in where the parameter was.
 *
 * The buffer it builds in is one shared static, so what is in it is saved
 * and put back: expanding an argument can reach a macro whose own argument
 * has to be expanded, and the inner one would otherwise write over the
 * outer one's work. */
static void expand_text_into(Buf *out, const char *text)
{
    char *saved = NULL;
    int   saved_len = if_len, saved_depth = if_depth, i;
    NameRef saved_active[INCLUDE_MAX];

    if (saved_len) {
        saved = malloc((size_t) saved_len);
        if (!saved)
            acc_error("out of memory expanding a macro");
        memcpy(saved, if_text, (size_t) saved_len);
    }
    for (i = 0; i < saved_depth; i++)
        saved_active[i] = if_active[i];

    if_len = 0;
    if_depth = 0;
    if_expand(text, (int) strlen(text));
    if_put("", 1);
    buf_put(out, if_text, if_len - 1);

    if (saved) {
        memcpy(if_text, saved, (size_t) saved_len);
        free(saved);
    }
    if_len = saved_len;
    if_depth = saved_depth;
    for (i = 0; i < saved_depth; i++)
        if_active[i] = saved_active[i];
}

/* The condition of an #if or an #elif: what is left of the line, expanded,
 * and then read. */
static int if_condition(void)
{
    long long v;
    int len;

    skip_blanks();
    len = logical_line();

    if_len = 0;
    if_depth = 0;
    if_expand(body ? body : "", len);
    if_put("", 1);                      /* the terminator */

    ep = if_text;
    v = if_ternary();
    ep_blanks();
    if (*ep)
        acc_error_at(line, "'%c' is left over at the end of an #if", *ep);

    return v != 0;
}

/* One `#if` and its `#else`, innermost last. `taken` is whether any branch
 * of this group has been used, which is what makes the `#else` of a group
 * whose `#ifdef` was true a group to skip. */
/* What ended a skipped group. */
enum { GROUP_ELSE, GROUP_ELIF, GROUP_ENDIF };

/* Past a group that is not taken, to the directive that ends it.
 *
 * The text inside is not lexed: a group under an `#if 0` is only required
 * to be made of preprocessing tokens, and in practice holds prose, half a
 * function, or an apostrophe that would close nothing. So this reads lines,
 * not tokens, and looks at nothing but the directives.
 *
 * Block comments are the exception, because one can legitimately run over
 * the `#endif` that would otherwise end the group. */
static int skip_group(void)
{
    int nest = 0, in_comment = 0, opened = line;
    char name[32];

    for (;;) {
        /* At the start of a line. */
        if (!in_comment) {
            skip_blanks();
            if (hash_at(cursor)) {
                cursor += hash_at(cursor);
                skip_blanks();
                directive_name(name, (int) sizeof name);

                /* Only the conditionals are looked at in here: an
                 * `#error` under an `#if 0` is one the program meant not
                 * to say. */
                if (!strcmp(name, "if") || !strcmp(name, "ifdef")
                    || !strcmp(name, "ifndef")) {
                    nest++;
                } else if (!strcmp(name, "endif")) {
                    if (!nest)
                        return GROUP_ENDIF;
                    nest--;
                } else if (!nest && !strcmp(name, "else")) {
                    return GROUP_ELSE;
                } else if (!nest && !strcmp(name, "elif")) {
                    return GROUP_ELIF;
                }
            }
        }

        /* The rest of the line, watching for a comment that opens on it and
         * for the end of one that opened earlier. */
        for (;;) {
            while (*cursor && *cursor != '\n') {
                if (in_comment) {
                    if (cursor[0] == '*' && cursor[1] == '/') {
                        in_comment = 0;
                        cursor += 2;

                        continue;
                    }
                } else if (cursor[0] == '/' && cursor[1] == '*') {
                    in_comment = 1;
                    cursor += 2;

                    continue;
                } else if (cursor[0] == '/' && cursor[1] == '/') {
                    break;      /* the rest of the line is a comment */
                }
                cursor++;
            }
            while (*cursor && *cursor != '\n')
                cursor++;
            if (*cursor)
                break;
            if (!refill())
                acc_error_at(opened, "#if without #endif");
        }
        cursor++;               /* the newline */
        line++;
    }
}

static void cond_push(int taken)
{
    if (nconds == COND_MAX)
        acc_error_at(line, "conditionals nested more than %d deep", COND_MAX);
    conds[nconds].taken = (unsigned char) (taken != 0);
    conds[nconds].seen_else = 0;
    conds[nconds].line = line;
    nconds++;
}

/* A group whose condition was false is skipped, and whatever ends it is
 * handled here rather than by the directive loop: the `#else` of a group
 * that was skipped starts a group that is taken. */
static void cond_skip_until_taken(void)
{
    for (;;) {
        int end = skip_group();

        if (end == GROUP_ENDIF) {
            nconds--;

            return;
        }
        if (end == GROUP_ELSE) {
            if (conds[nconds - 1].seen_else)
                acc_error_at(line, "#else after #else");
            conds[nconds - 1].seen_else = 1;
            if (!conds[nconds - 1].taken) {
                conds[nconds - 1].taken = 1;

                return;         /* this branch is the one that runs */
            }

            continue;           /* a branch was taken already: skip on */
        }

        /* #elif. Its condition only has to be worked out when no branch of
         * this group has run yet; after one has, the rest are skipped
         * whatever they say. */
        if (conds[nconds - 1].seen_else)
            acc_error_at(line, "#elif after #else");
        if (conds[nconds - 1].taken)
            continue;
        if (if_condition()) {
            conds[nconds - 1].taken = 1;

            return;
        }
    }
}

static void do_if(void)
{
    if (if_condition()) {
        cond_push(1);

        return;
    }
    cond_push(0);
    cond_skip_until_taken();
}

/* An #elif reached by reading rather than by skipping: the branch before it
 * ran, so this one does not whatever it says. */
static void do_elif(void)
{
    if (!nconds)
        acc_error_at(line, "#elif without #if");
    if (conds[nconds - 1].seen_else)
        acc_error_at(line, "#elif after #else");
    rest_of_line();
    cond_skip_until_taken();
}

static void do_ifdef(int want)
{
    NameRef name;

    skip_blanks();
    name = directive_target(want ? "ifdef" : "ifndef");
    rest_of_line();

    if ((macro_find(name) != NULL || predefined_name(name)) == want) {
        cond_push(1);

        return;
    }
    cond_push(0);
    cond_skip_until_taken();
}

static void do_else(void)
{
    if (!nconds)
        acc_error_at(line, "#else without #if");
    if (conds[nconds - 1].seen_else)
        acc_error_at(line, "#else after #else");
    conds[nconds - 1].seen_else = 1;
    rest_of_line();

    /* Getting here means the branch before this one ran, so this one does
     * not: skip to the #endif. */
    cond_skip_until_taken();
}

static void do_endif(void)
{
    if (!nconds)
        acc_error_at(line, "#endif without #if");
    nconds--;
    rest_of_line();
}

/* At the end of the outermost file: a conditional left open is a mistake
 * that would otherwise be silent. */
void lex_end(void)
{
    if (nconds)
        acc_error_at(conds[nconds - 1].line, "#if without #endif");
}

/* The name a #define or an #undef is about. Interned, so that what the
 * lexer will hand back for the same spelling is the same reference. */
static NameRef directive_target(const char *what)
{
    const char *start = cursor;

    if (!is_alpha((unsigned char) *cursor))
        acc_error_at(line, "#%s needs a name", what);
    while (is_alnum((unsigned char) *cursor))
        cursor++;

    return name_intern(start, (int) (cursor - start));
}

static void do_define(void)
{
    NameRef name;
    NameRef *params = NULL;
    int nparams = -1, variadic = 0, pcap;
    int len;

    skip_blanks();
    name = directive_target("define");
    if (predefined_name(name))
        acc_error_at(line, "'%s' is the compiler's to define",
                     name_text(name));

    /* `#define f(x)` is a macro with parameters, which is a different thing
     * from a name standing for some text: the parenthesis has to follow the
     * name with nothing between it and the name to mean that, which is why
     * this is asked before any blanks are skipped. */
    if (*cursor == '(') {
        cursor++;
        pcap = 8;                       /* grown as a macro takes more */
        params = malloc((size_t) pcap * sizeof *params);
        if (!params)
            acc_error("out of memory for a macro's parameters");
        nparams = 0;

        skip_blanks();
        if (*cursor == ')') {
            cursor++;                   /* `f()`, which takes one empty one */
        } else {
            for (;;) {
                skip_blanks();
                if (cursor[0] == '.' && cursor[1] == '.' && cursor[2] == '.') {
                    cursor += 3;
                    variadic = 1;
                    skip_blanks();
                    break;
                }
                if (nparams == PARAMS_MAX)
                    acc_error_at(line, "a macro takes at most %d parameters",
                                 PARAMS_MAX);
                if (nparams == pcap) {
                    pcap *= 2;
                    params = realloc(params, (size_t) pcap * sizeof *params);
                    if (!params)
                        acc_error("out of memory for a macro's parameters");
                }
                params[nparams++] = directive_target("define");
                skip_blanks();
                if (*cursor != ',')
                    break;
                cursor++;
            }
            if (*cursor != ')')
                acc_error_at(line, "the parameters of a macro need a ')'");
            cursor++;
        }
    }

    skip_blanks();
    len = logical_line();

    /* Blanks at either end are left in. Nothing reads the text but the
     * lexer, which skips them; trimming them would be work whose result
     * nothing can tell apart. When `#` arrives it will be able to -- what a
     * parameter stringifies to is spelled out -- and the trimming belongs
     * with it, where a test can show the difference. */
    macro_define(name, body, len, params, nparams, variadic);
}

static void do_undef(void)
{
    NameRef name;

    skip_blanks();
    name = directive_target("undef");
    if (predefined_name(name))
        acc_error_at(line, "'%s' is the compiler's to define",
                     name_text(name));
    cursor = (char *) blanks_and_comments(cursor);
    if (*cursor && *cursor != '\n')
        acc_error_at(line, "#undef takes one name and nothing else");

    macro_undef(name);
    rest_of_line();
}

/* `#error` with whatever the program has to say about it, which is the rest
 * of the line as written. */
static void do_error(void)
{
    const char *start;

    skip_blanks();
    start = cursor;
    rest_of_line();

    acc_error_at(line, "#error %.*s", (int) (cursor - start), start);
}

/* The macros a compiler is expected to have defined before it reads a line.
 *
 * None of them is in C. What they are is the way a program written for a
 * machine it has not been told about asks what that machine is: <stdint.h>
 * is written in terms of them, and so is every test that wants an integer
 * of a named width without a header to get it from. A compiler that does
 * not have them is one such a program cannot be compiled by, and of the
 * gcc torture tests acc turns down, more are turned down for the want of
 * these than for anything else.
 *
 * The values are agondev's, read out of its clang with `-dM -E`, because
 * they have to be: a program compiled by one and then the other has to be
 * the same program, and these say how wide everything is. They are macros
 * and not words of the language because what most of them stand for is two
 * or three tokens, and because a program is allowed to take them back --
 * gcc lets a file #undef them, and so does this.
 */
static const struct {
    const char *name;
    const char *text;
} predefined_macros[] = {
    /* What a type of a given width is called here. */
    { "__SIZE_TYPE__",       "unsigned int" },
    { "__PTRDIFF_TYPE__",    "int" },
    { "__WCHAR_TYPE__",      "short" },
    { "__WINT_TYPE__",       "int" },
    { "__INTPTR_TYPE__",     "int" },
    { "__UINTPTR_TYPE__",    "unsigned int" },
    { "__INTMAX_TYPE__",     "long long int" },
    { "__UINTMAX_TYPE__",    "long long unsigned int" },
    { "__INT8_TYPE__",       "signed char" },
    { "__UINT8_TYPE__",      "unsigned char" },
    { "__INT16_TYPE__",      "short" },
    { "__UINT16_TYPE__",     "unsigned short" },
    { "__INT24_TYPE__",      "int" },
    { "__UINT24_TYPE__",     "unsigned int" },
    { "__INT32_TYPE__",      "long int" },
    { "__UINT32_TYPE__",     "long unsigned int" },
    { "__INT64_TYPE__",      "long long int" },
    { "__UINT64_TYPE__",     "long long unsigned int" },

    /* How wide each of them is, in bytes, and a char in bits. */
    { "__CHAR_BIT__",        "8" },
    { "__SIZEOF_SHORT__",    "2" },
    { "__SIZEOF_INT__",      "3" },
    { "__SIZEOF_LONG__",     "4" },
    { "__SIZEOF_LONG_LONG__", "8" },
    { "__SIZEOF_FLOAT__",    "4" },
    { "__SIZEOF_DOUBLE__",   "4" },
    { "__SIZEOF_LONG_DOUBLE__", "8" },
    { "__SIZEOF_POINTER__",  "3" },
    { "__SIZEOF_SIZE_T__",   "3" },
    { "__SIZEOF_PTRDIFF_T__", "3" },
    { "__SIZEOF_WCHAR_T__",  "2" },
    { "__SIZEOF_WINT_T__",   "3" },

    /* And the largest each will hold. */
    { "__SCHAR_MAX__",       "127" },
    { "__SHRT_MAX__",        "32767" },
    { "__INT_MAX__",         "8388607" },
    { "__LONG_MAX__",        "2147483647L" },
    { "__LONG_LONG_MAX__",   "9223372036854775807LL" },

    /* Which end the low byte is at. The three orders are named so that a
     * program can compare against them without knowing the numbers. */
    { "__ORDER_LITTLE_ENDIAN__", "1234" },
    { "__ORDER_BIG_ENDIAN__",    "4321" },
    { "__ORDER_PDP_ENDIAN__",    "3412" },
    { "__BYTE_ORDER__",      "__ORDER_LITTLE_ENDIAN__" }
};

static void predefined_macros_init(void)
{
    int i;

    for (i = 0; i < (int) (sizeof predefined_macros
                           / sizeof *predefined_macros); i++) {
        const char *name = predefined_macros[i].name;
        const char *text = predefined_macros[i].text;

        macro_define(name_intern(name, (int) strlen(name)),
                     text, (int) strlen(text), NULL, -1, 0);
    }
}

/* A macro given on the command line, as though the file had begun with the
 * directive that says it.
 *
 * Written out as `#define` would have read it and handed to the same code,
 * rather than built from the parts here: a name given on the command line
 * is a macro like any other, down to taking parameters, and two ways of
 * making one is one more than is needed. The text is pushed as a window of
 * its own -- the lexer already reads a macro's expansion that way -- and
 * popped when the directive has had it.
 *
 * `-DNAME` is `-DNAME=1`. `-D'NAME(a,b)=...'` is a macro with parameters,
 * because the bracket follows the name with nothing between them, which is
 * the same rule the directive has. */
void lex_define(const char *arg)
{
    size_t n = strlen(arg);
    const char *eq = strchr(arg, '=');
    char *text = malloc(n + 4);

    if (!is_alpha((unsigned char) arg[0]))
        acc_error("-D needs a name, and '%s' does not begin with one", arg);
    opt_fold('D', arg);
    if (!text)
        acc_error("out of memory for a -D");
    if (eq) {
        memcpy(text, arg, (size_t) (eq - arg));
        text[eq - arg] = ' ';
        strcpy(text + (eq - arg) + 1, eq + 1);
    } else {
        memcpy(text, arg, n);
        strcpy(text + n, " 1");
    }
    strcat(text, "\n");

    push_owned_text(NAME_NONE, text);
    do_define();
    pop_source();
}

/* And one taken away again, the same way. `-U` of a name nothing defined
 * is allowed, as `#undef` of one is. */
void lex_undefine(const char *arg)
{
    char *text = malloc(strlen(arg) + 2);

    if (!is_alpha((unsigned char) arg[0]))
        acc_error("-U needs a name, and '%s' does not begin with one", arg);
    opt_fold('U', arg);
    if (!text)
        acc_error("out of memory for a -U");
    strcpy(text, arg);
    strcat(text, "\n");

    push_owned_text(NAME_NONE, text);
    do_undef();
    pop_source();
}

/* `#pragma`. C says an unknown one is ignored, and acc knows two. `once`
 * says this file is to be read once however many times it is included.
 * `weak NAME` says a reference to NAME is not a reason to link it: if
 * nothing else wants it, it is not looked for, and without it the address
 * is zero -- which is how printf reaches the floating conversions only in a
 * program that has a float to print. */
static void do_pragma(void)
{
    char what[32];

    skip_blanks();
    if (directive_name(what, (int) sizeof what)) {
        if (!strcmp(what, "once")) {
            once_add(src_real);
        } else if (!strcmp(what, "weak")) {
            skip_blanks();
            name_set_weak(directive_target("pragma weak"), 1);
        }
    }
    rest_of_line();
}

/* `#line N` and `#line N "file"`: what the lines after this one are called,
 * which is what a program that generates C uses to point at what it was
 * generated from.
 *
 * The number is the line *after* this one, and the newline that ends this
 * directive is still to be counted, so what is stored is one less. */
/* Blanks in whichever text a #line is being read out of. */
static const char *line_blanks(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r')
        p++;

    return p;
}

static void do_line(void)
{
    const char *p;
    long n = 0;
    int digits = 0, expanded = 0;

    skip_blanks();

    /* A #line whose number is not written as one has its tokens put through
     * the macros first, which is what makes `#define WHERE 500` and then
     * `#line WHERE` mean line 500. The common form costs nothing for it:
     * a digit here is read where it stands. */
    if (!is_digit((unsigned char) *cursor)) {
        int len = logical_line();

        if_len = 0;
        if_depth = 0;
        if_expand(body ? body : "", len);
        if_put("", 1);
        p = line_blanks(if_text);
        expanded = 1;
    } else {
        p = cursor;
    }

    while (is_digit((unsigned char) *p)) {
        n = n * 10 + (*p++ - '0');
        digits = 1;
        if (n > 0xffffff)
            acc_error_at(line, "the line number in a #line is too large");
    }
    if (!digits)
        acc_error_at(line, "#line needs a line number");

    p = line_blanks(p);
    if (*p == '"') {
        const char *start;
        char *keep;

        p++;
        start = p;
        while (*p && *p != '"' && *p != '\n')
            p++;
        if (*p != '"')
            acc_error_at(line, "the file name in a #line is not closed");

        keep = malloc((size_t) (p - start) + 1);
        if (!keep)
            acc_error("out of memory for a #line");
        memcpy(keep, start, (size_t) (p - start));
        keep[p - start] = '\0';
        p++;

        /* The name belongs to this level now, and whatever it had before
         * goes -- which for a file is the copy push_source made. */
        if (src_owned & OWN_PATH)
            free((char *) src_path);
        src_path = keep;
        src_owned |= OWN_PATH;
    }

    p = blanks_and_comments(p);
    if (*p && *p != '\n')
        acc_error_at(line, "#line takes a number and a file name, and "
                           "nothing else");

    if (!expanded) {
        cursor = (char *) p;
        rest_of_line();
    }
    line = (int) n - 1;
}

/* Every directive in a row, and whatever space and comments follow them, so
 * that next() comes back to a real token. An `#include` leaves the cursor at
 * the start of the file it names, which may itself begin with directives.
 *
 * Out of line: a compile that uses no directive at all pays one test. */
__attribute__((noinline))
static void directives(void)
{
    for (;;) {
        int started = line;

        directive();
        skip_space();
        if (!*cursor)
            window_more();

        /* Another one only if it is first on its line too, which after a
         * directive means the line moved on. */
        if (!hash_at(cursor) || line == started)
            return;
    }
}

/* One directive, with the cursor on its '#'. The line it is on is consumed,
 * up to but not including its newline. */
static void directive(void)
{
    char name[32];

    cursor += hash_at(cursor);          /* the '#', or '%:' */
    skip_blanks();

    /* `# 200 "file"`, which is a #line with the word left out. Nothing
     * writes it by hand, but it is what a preprocessor puts in front of the
     * text it produces, and acc reads such a file whenever one is handed to
     * it. The digits are left where they are for do_line to read. */
    if (is_digit((unsigned char) *cursor)) {
        do_line();

        return;
    }
    if (!directive_name(name, (int) sizeof name)) {
        rest_of_line();                 /* a '#' alone, which C allows */

        return;
    }

    if (!strcmp(name, "include")) {
        do_include();

        return;
    }
    if (!strcmp(name, "define")) {
        do_define();

        return;
    }
    if (!strcmp(name, "undef")) {
        do_undef();

        return;
    }
    if (!strcmp(name, "if")) {
        do_if();

        return;
    }
    if (!strcmp(name, "elif")) {
        do_elif();

        return;
    }
    if (!strcmp(name, "ifdef")) {
        do_ifdef(1);

        return;
    }
    if (!strcmp(name, "ifndef")) {
        do_ifdef(0);

        return;
    }
    if (!strcmp(name, "else")) {
        do_else();

        return;
    }
    if (!strcmp(name, "endif")) {
        do_endif();

        return;
    }
    if (!strcmp(name, "error")) {
        do_error();

        return;
    }
    if (!strcmp(name, "pragma")) {
        do_pragma();

        return;
    }
    if (!strcmp(name, "line")) {
        do_line();

        return;
    }

    acc_error_at(line, "'#%s' is not a directive acc knows", name);
}

__attribute__((noinline))
static void window_more(void)
{
    for (;;) {
        if (!refill() && !pop_source())
            return;                     /* the outermost file has ended */
        skip_space();
        if (*cursor)
            return;
    }
}

__attribute__((noinline))
static void skip_space(void)
{
    for (;;) {
        while (is_space(*cursor)) {
            if (*cursor == '\n')
                line++;
            cursor++;
        }
        if (cursor[0] != '/' || (cursor[1] != '/' && cursor[1] != '*'))
            return;
        skip_comment();
    }
}

/* The keywords, checked against an interned name rather than by strcmp on
 * every identifier. Interning gives each spelling one offset, so recognising
 * a keyword is three integer compares. */
/* Keywords are recognised by where they are, not by comparing against each
 * of them in turn.
 *
 * They are interned before anything else, so they occupy the first bytes of
 * the arena and every other name has a larger offset. That makes one compare
 * and one indexed load enough: a chain of `tok_name == kw_...` tests costs a
 * comparison per keyword on every identifier in the program, which is what
 * the compiler does most, and adding a keyword made it slower for everyone.
 * Adding one here costs nothing.
 *
 * Each keyword's token is kept where any other name keeps its file-scope
 * symbol, in the bytes in front of its text: a keyword can never name one,
 * so the field is free, and the lexer reads the token from the name it has
 * just interned. */
static NameRef kw_limit;

static void keyword(const char *text, int len, int token)
{
    NameRef ref = name_intern(text, len);

    name_arena[ref - 3] = (char) token;
    kw_limit = ref + 1;
}

static void keywords_init(void)
{
    keyword("int", 3, TK_KW_INT);
    keyword("void", 4, TK_KW_VOID);
    keyword("return", 6, TK_KW_RETURN);
    keyword("if", 2, TK_KW_IF);
    keyword("else", 4, TK_KW_ELSE);
    keyword("while", 5, TK_KW_WHILE);
    keyword("char", 4, TK_KW_CHAR);
    keyword("short", 5, TK_KW_SHORT);
    keyword("long", 4, TK_KW_LONG);
    keyword("signed", 6, TK_KW_SIGNED);
    keyword("unsigned", 8, TK_KW_UNSIGNED);
    keyword("float", 5, TK_KW_FLOAT);
    keyword("double", 6, TK_KW_DOUBLE);
    keyword("for", 3, TK_KW_FOR);

    /* The rest of what C99 reserves. None of it is implemented and all of it
     * is refused by name, which is the whole reason for interning it: a word
     * the lexer does not know becomes an identifier, and the complaint that
     * follows is about a name not being declared rather than about the
     * feature not being there. */
    keyword("auto", 4, TK_KW_AUTO);
    keyword("break", 5, TK_KW_BREAK);
    keyword("case", 4, TK_KW_CASE);
    keyword("const", 5, TK_KW_CONST);
    keyword("continue", 8, TK_KW_CONTINUE);
    keyword("default", 7, TK_KW_DEFAULT);
    keyword("do", 2, TK_KW_DO);
    keyword("enum", 4, TK_KW_ENUM);
    keyword("extern", 6, TK_KW_EXTERN);
    keyword("goto", 4, TK_KW_GOTO);
    keyword("inline", 6, TK_KW_INLINE);
    keyword("register", 8, TK_KW_REGISTER);
    keyword("restrict", 8, TK_KW_RESTRICT);
    keyword("sizeof", 6, TK_KW_SIZEOF);
    keyword("static", 6, TK_KW_STATIC);
    keyword("struct", 6, TK_KW_STRUCT);
    keyword("switch", 6, TK_KW_SWITCH);
    keyword("typedef", 7, TK_KW_TYPEDEF);

    /* Not words of the language but names the preprocessor defines, which
     * is why they are here rather than among the macros: a macro costs a
     * lookup for every identifier in the program, and a keyword costs the
     * one the lexer was doing anyway. C forbids a program from defining or
     * undefining either, so nothing is lost by their not being in the
     * table. */
    keyword("__FILE__", 8, TK_FILE);
    keyword("__LINE__", 8, TK_LINE);
    keyword("__DATE__", 8, TK_DATE);
    keyword("__TIME__", 8, TK_TIME);
    keyword("__STDC__", 8, TK_STDC);
    keyword("__STDC_VERSION__", 16, TK_STDC_VERSION);
    keyword("__STDC_HOSTED__", 15, TK_STDC_HOSTED);
    keyword("_Pragma", 7, TK_PRAGMA_OP);
    keyword("union", 5, TK_KW_UNION);
    keyword("volatile", 8, TK_KW_VOLATILE);
    keyword("_Bool", 5, TK_KW_BOOL);
    keyword("_Static_assert", 14, TK_KW_STATIC_ASSERT);
    keyword("__builtin_offsetof", 18, TK_KW_OFFSETOF);
    keyword("__attribute__", 13, TK_KW_ATTRIBUTE);

    /* gcc's spellings of restrict, which headers written for it use where
     * restrict cannot go -- before C99, or in C++ -- and which are the
     * implementation's to give a meaning to. Read as a name, one was a
     * parameter's: `void *memcpy(void *__restrict, const void
     * *__restrict, size_t)` named two parameters the same. */
    keyword("__restrict", 10, TK_KW_RESTRICT);
    keyword("__restrict__", 12, TK_KW_RESTRICT);

    /* What <stdarg.h> would give, taken as words of the language: acc has
     * no preprocessor to include it with, and a program that reads its
     * variable arguments has no other way to. */
    keyword("va_list", 7, TK_KW_VA_LIST);
    keyword("va_start", 8, TK_KW_VA_START);
    keyword("va_arg", 6, TK_KW_VA_ARG);
    keyword("va_end", 6, TK_KW_VA_END);
    keyword("va_copy", 7, TK_KW_VA_COPY);
    keyword("_Complex", 8, TK_KW_RESERVED);
    keyword("_Imaginary", 10, TK_KW_RESERVED);
}

/* Punctuation, by the character that begins it. TK_EOF means the character
 * is not punctuation at all, which is what makes zero the right default.
 *
 * A switch over these compiled to twenty-three comparisons in a row -- the
 * backend builds no jump table -- so a `;` cost a few and anything late in
 * the list cost twenty, once per punctuation token in the program. */
static const unsigned char punct[256] = {
    ['('] = TK_LPAREN, [')'] = TK_RPAREN,
    ['{'] = TK_LBRACE, ['}'] = TK_RBRACE,
    [';'] = TK_SEMI,   [','] = TK_COMMA,
    ['='] = TK_ASSIGN, ['!'] = TK_NOT,
    ['+'] = TK_PLUS,   ['-'] = TK_MINUS,
    ['*'] = TK_STAR,   ['/'] = TK_SLASH,
    ['&'] = TK_AMP,    ['|'] = TK_PIPE,   ['^'] = TK_CARET,
    ['~'] = TK_TILDE,
    ['>'] = TK_GT,
    ['?'] = TK_QUESTION,
    ['['] = TK_LBRACKET, [']'] = TK_RBRACKET,
    ['.'] = TK_DOT
};

/* Read a floating literal from the cursor, which is at its first character.
 *
 * Converted by float_literal rather than the C library's strtod, which on the
 * Agon does not round correctly and on the host rounds twice: see float.c.
 *
 * Out of line because next() is the hottest function in the compiler and most
 * programs have no floating literals at all. */
__attribute__((noinline))
static void lex_floating(void)
{
    uint32_t bits;
    const char *end = float_literal(cursor, &bits);

    if (end == cursor)
        acc_error_at(tok_line, "a floating-point number with no digits");

    /* An l suffix would make it a long double, which acc does not have: said
     * here rather than left to the parser, which would see the letter as a
     * name of its own and blame the punctuation. */
    if (*end == 'l' || *end == 'L')
        acc_error_at(tok_line, "'long double' is not supported: agondev's "
                               "library has no arithmetic for one, so a "
                               "program that asks for it does not link");

    memcpy(&tok_fval, &bits, sizeof tok_fval);
    cursor = (char *) end;
    if (*cursor == 'f' || *cursor == 'F')
        cursor++;
    tok = TK_FLOAT;
    tok_type = TY_FLOAT;
}

/* A constant past 32 bits, read again from its start as the long long it
 * is: its high half returned and its low half in *low. Out of line, and
 * rare, because its 64-bit steps are calls into the runtime on the Agon. */
__attribute__((noinline))
static uint32_t wide_constant(const char *p, uint32_t *low)
{
    uint64_t value = 0, limit;
    unsigned base = 10;

    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        base = 16;
        p += 2;
    } else if (p[0] == '0') {
        base = 8;
    }
    limit = UINT64_MAX / base;
    for (;; p++) {
        int c = (unsigned char) *p, digit;

        if (is_digit(c))
            digit = c - '0';
        else if (base == 16 && (c | 0x20) >= 'a' && (c | 0x20) <= 'f')
            digit = (c | 0x20) - 'a' + 10;
        else
            break;
        if (value > limit || value * base > UINT64_MAX - (unsigned) digit)
            acc_error_at(tok_line, "the constant does not fit in 64 bits");
        value = value * base + (unsigned) digit;
    }
    *low = (uint32_t) value;

    return (uint32_t) (value >> 32);
}

/* Out of line, and the attribute is load-bearing -- there is one caller, so
 * it goes straight back inline without it.
 *
 * It is not about the size of the code. `value` is an unsigned long, which is
 * four bytes on a three-byte machine, and the accumulator and the bound it is
 * checked against want frame slots. Inline, next() opened a frame wide enough
 * for them on every token it read, and the tokens a program is mostly made of
 * are names and punctuation, which need no frame at all. Out of line, the
 * cost is paid by the numeric constants and by nothing else. */
__attribute__((noinline))
static void lex_number(void)
{
    /* One or two decimal digits, with nothing after them that could make the
     * constant anything but a small int -- no suffix, no point, no exponent,
     * and no leading 0 that would make it octal or hex. That is most of the
     * constants in a program, and the answer is at most 99, so the multiply
     * is a byte one: this target does it with MLT and no call into the
     * runtime, where the general path below makes four such calls for every
     * digit and then climbs a ladder of four-byte compares. A wider fast path
     * in an int was tried and was slower -- a 24-bit multiply is itself a
     * call, and the accumulator lived in the frame. */
    {
        unsigned char first = (unsigned char) cursor[0];
        unsigned char second = (unsigned char) cursor[1];

        if (!is_alnum(second) && second != '.') {
            cursor++;
            tok = TK_INT;
            tok_type = TY_INT;
            tok_val = first - '0';

            return;
        }
        if (first != '0' && is_digit(second)) {
            unsigned char third = (unsigned char) cursor[2];

            if (!is_alnum(third) && third != '.') {
                cursor += 2;
                tok = TK_INT;
                tok_type = TY_INT;
                tok_val = (unsigned char) ((first - '0') * 10 + (second - '0'));

                return;
            }
        }
    }

    /* Accumulated with the bound checked before each step rather than
     * after, so the accumulator never overflows and there is nothing to
     * detect after the fact. Everything here stays in an int, which on
     * this target is the 24 bits the answer has to fit in anyway. */
    /* Unsigned, because the largest constant acc takes is 0xFFFFFFFF and
     * long on this target is 32 bits signed. The bits are what is wanted;
     * tok_val holds them and the type says how to read them.
     *
     * Exactly 32 bits, and not `unsigned long`, which is 32 bits when acc is
     * compiled for the Agon and 64 when it is compiled for the host it is
     * tested on. With the guard below reading a width that changes, the two
     * builds disagreed about which constants a program may contain: the host
     * build accumulated an eleven-digit constant without complaint and let
     * the ladder decide, while the Agon build wrapped and refused it. The
     * width the answer has to fit in is a fact about the language acc
     * compiles, not about the machine acc is running on. */
    uint32_t value = 0, high = 0;
    int not_decimal = 0;        /* hex or octal: may be typed unsigned */
    int bad_digit = 0;
    int overflowed = 0;
    int suffix_u = 0, suffix_l = 0;
    char *start = cursor;

    tok_type = TY_VOID;         /* no type chosen yet; the ladder picks one */

    if (*cursor == '0' && (cursor[1] == 'x' || cursor[1] == 'X')) {
        not_decimal = 1;
        cursor += 2;
        if (!is_alnum((unsigned char) *cursor))
            acc_error_at(tok_line, "hex constant with no digits");
        while (is_alnum((unsigned char) *cursor)) {
            int digit = *cursor;

            /* A suffix, which the loop used to read as a digit and refuse:
             * 0xffu and 0x10L were errors. No hex digit is u or l. Nor is
             * p, which is a hex float's exponent: C99 lets one go without
             * a point, as 0x1p-126 does, and the loop refused that too. */
            if ((digit | 0x20) == 'u' || (digit | 0x20) == 'l'
                || (digit | 0x20) == 'p')
                break;
            if (is_digit(digit))                     digit -= '0';
            else if (digit >= 'a' && digit <= 'f')   digit -= 'a' - 10;
            else if (digit >= 'A' && digit <= 'F')   digit -= 'A' - 10;
            else acc_error_at(tok_line, "bad digit '%c' in a hex constant", digit);
            if (value > 0xfffffffUL)
                overflowed = 1;         /* a long long: wide_constant */
            else
                value = value * 16 + digit;
            cursor++;
        }
    } else {
        /* A leading 0 followed by more digits is octal, which C has always
         * said and acc did not: `010` was ten. An 8 or a 9 in one is noted
         * rather than refused, for the same reason the overflow below is --
         * `09.5` is an ordinary decimal float, and which this is shows only
         * at the character the digits stop at. */
        int octal = (*cursor == '0' && is_digit((unsigned char) cursor[1]));
        int bad_octal = 0;

        not_decimal = octal;

        /* The first digits in an unsigned, for as long as another digit
         * cannot carry it past 24 bits: that covers every decimal constant
         * below ten million, and the steps are a 24-bit multiply and an add
         * where the loop below makes a four-byte multiply, add and compare
         * of each digit. Octal is left to the loop: it is rare, and
         * its digits are fewer. */
        if (!octal) {
            unsigned small = 0;

            while (is_digit((unsigned char) *cursor) && small < 1677721u) {
                small = small * 10 + (unsigned) (*cursor - '0');
                cursor++;
            }
            value = small;
        }
        while (is_digit((unsigned char) *cursor)) {
            int digit = *cursor - '0';

            /* Noted rather than refused, because the digits might still turn
             * out to be the whole part of a floating literal, where a
             * hundred of them are ordinary. Only once the terminator says
             * this was an integer does too many digits become an error. */
            if (octal) {
                if (digit > 7 && !bad_octal)
                    bad_octal = *cursor;
                if (value > 0x1fffffffUL)
                    overflowed = 1;
                else
                    value = value * 8 + digit;
                cursor++;

                continue;
            }
            if (value > 429496729UL
                || (value == 429496729UL && digit > 5)) {
                overflowed = 1;
                cursor++;

                continue;
            }
            value = value * 10 + digit;
            cursor++;
        }
        bad_digit = bad_octal;
    }

    /* C99 types a constant by the first type that can hold it. A decimal
     * one goes int, long int, long long int; a hex or octal one may also
     * be unsigned at each step, which is the only place the two forms
     * differ. acc has no long long, so past the end of long it refuses. */
    /* The character the digits stopped at decides which kind it was. A
     * hex constant's digits take in e and f, so only a hex one stops at p. */
    if (*cursor == '.' || *cursor == 'e' || *cursor == 'E'
        || *cursor == 'f' || *cursor == 'F'
        || ((*cursor | 0x20) == 'p' && (start[1] | 0x20) == 'x')) {
        cursor = start;
        lex_floating();

        return;
    }

    if (bad_digit)
        acc_error_at(tok_line, "'%c' is not an octal digit, and a constant that "
                           "starts with 0 is octal", bad_digit);
    if (overflowed)
        high = wide_constant(start, &value);

    /* The suffix, which narrows the list before the value is measured
     * against it: u takes the signed types out, l takes int out. Either
     * order and either case, and at most one u and two l -- and `lL` is not
     * a suffix at all, because the two letters of a long long one have to
     * match. */
    while (*cursor == 'u' || *cursor == 'U'
           || *cursor == 'l' || *cursor == 'L') {
        if (*cursor == 'u' || *cursor == 'U') {
            if (suffix_u)
                acc_error_at(tok_line, "the constant has more than one 'u' suffix");
            suffix_u = 1;
            cursor++;

            continue;
        }

        if (suffix_l)
            acc_error_at(tok_line, "the constant has more than one 'l' suffix");
        suffix_l = 1;
        if (cursor[1] == *cursor) {     /* ll or LL, but not lL */
            suffix_l = 2;
            cursor++;
        }
        cursor++;
    }
    if (is_alnum((unsigned char) *cursor))
        acc_error_at(tok_line, "'%c' is not a suffix a constant can have", *cursor);

    /* C99 types a constant by the first type in that list that can hold it.
     * The unsigned types are in it when the suffix says u, and also when a
     * hex or octal constant has no suffix at all -- which is the only place
     * the decimal and hex forms differ. Past the end of long it is long
     * long, below. */
    if (high || suffix_l == 2) {
        /* long long, which only the steps past long reach */
    } else if (suffix_u) {
        if (!suffix_l && value <= 0xffffffUL)
            tok_type = TY_UINT;
        else if (value <= 0xffffffffUL)
            tok_type = TY_ULONG;
    } else if (suffix_l) {
        if (value <= 0x7fffffffUL)
            tok_type = TY_LONG;
        else if (not_decimal)
            tok_type = TY_ULONG;
    } else if (value <= 0x7fffffUL) {
        tok_type = TY_INT;
    } else if (not_decimal && value <= 0xffffffUL) {
        tok_type = TY_UINT;
    } else if (value <= 0x7fffffffUL) {
        tok_type = TY_LONG;
    } else if (not_decimal) {
        tok_type = TY_ULONG;
    }

    /* The long long steps: signed if it fits and nothing says unsigned.
     * A decimal constant past the signed range has no type in C99; it is
     * taken as unsigned, which is what agondev does with it. */
    if (tok_type == TY_VOID)
        tok_type = (!suffix_u && high <= 0x7fffffffUL) ? TY_LLONG : TY_ULLONG;

    /* An unsigned int is normalised to the signed pattern of the same 24
     * bits, so that acc folds it identically whether it is itself running on
     * a 24-bit int or a 32-bit one. */
    if (tok_type == TY_UINT && value > 0x7fffffUL)
        value -= 0x1000000UL;

    tok = TK_INT;
    tok_val = (long) value;
    tok_val_hi = high;
}

/* The punctuation next() refuses: a character that begins no token, or the
 * `&&=` and `||=` that C does not have, named by their first character. Out
 * of line, reading the line for itself, so that next() keeps nothing for an
 * error it will almost never report: holding the line and the character for
 * these calls was a stack frame on every token. */
__attribute__((noinline, noreturn))
static void punct_error(int c)
{
    if (c == '&' && cursor[0] == '=')
        acc_error_at(tok_line, "'&&=' is not an operator in C; "
                               "`a = a && b` is what it would mean");
    if (c == '|' && cursor[0] == '=')
        acc_error_at(tok_line, "'||=' is not an operator in C; "
                               "`a = a || b` is what it would mean");
    acc_error_at(tok_line, "stray '%c' in the source", c);
}

/* ------------------------------------------------------------------ */
/* character and string literals                                       */

const char *tok_str;
int         tok_str_len;
int         tok_str_wide;
int         tok_str_escaped;

static char *str_buf;
static int   str_cap;

#define is_hexdigit(c) (is_digit(c) || ((unsigned) (((c) | 0x20) - 'a') < 6u))

/* A universal character name that reached a token still written as one,
 * at p: every one that names a character C allows was made UTF-8 when its
 * window was read, so this one either is short of digits or names one of
 * the characters 6.4.3p2 forbids. */
__attribute__((noinline, noreturn))
static void ucn_refuse(const char *p)
{
    int n = ucn_at(p);

    if (!n)
        acc_error_at(tok_line, "'\\%c' is a universal character name, and "
                               "needs %d hex digits after it", p[1],
                     p[1] == 'u' ? 4 : 8);
    acc_error_at(tok_line, "\\%c%.*s is not a character a universal "
                           "character name may name", p[1], n - 2, p + 2);
}

/* One character of a literal, the backslash of an escape read already: the
 * byte it stands for. Octal takes up to three digits and hex as many as
 * there are, which is what C says; a value past a byte is refused. */
static int escape(int wide)
{
    int c = (unsigned char) *cursor++, value, digits;
    int most = wide ? 0xffff : 0xff;

    switch (c) {
    case 'n':  return '\n';
    case 't':  return '\t';
    case 'r':  return '\r';
    case 'a':  return 7;
    case 'b':  return 8;
    case 'f':  return 12;
    case 'v':  return 11;
    case '\\': case '\'': case '"': case '?':
        return c;
    case 'x':
        if (!is_hexdigit((unsigned char) *cursor))
            acc_error_at(tok_line, "'\\x' needs a hex digit after it");
        value = 0;
        while (is_hexdigit((unsigned char) *cursor)) {
            int d = *cursor++;

            d = is_digit(d) ? d - '0' : (d | 0x20) - 'a' + 10;
            value = value * 16 + d;
            if (value > most)
                acc_error_at(tok_line, wide ? "a '\\x' escape past 0xffff "
                                              "does not fit in a wchar_t"
                                            : "a '\\x' escape past 0xff does "
                                              "not fit in a char");
        }
        if (value > 0x7f)
            tok_str_escaped = 1;        /* see string_gather */

        return value;
    }
    if (c >= '0' && c <= '7') {
        value = c - '0';
        for (digits = 1; digits < 3 && *cursor >= '0' && *cursor <= '7'; digits++)
            value = value * 8 + (*cursor++ - '0');
        if (value > most)
            acc_error_at(tok_line, "an octal escape past \\377 does not fit "
                                   "in a char");
        if (value > 0x7f)
            tok_str_escaped = 1;        /* see string_gather */

        return value;
    }
    if (c == 'u' || c == 'U')
        ucn_refuse(cursor - 2);
    acc_error_at(tok_line, "'\\%c' is not an escape C has", c);
}

/* A character of a literal: an escape, or itself. A newline ends the line
 * the literal was meant to finish on, and the end of the file it all. */
static int literal_char(int quote)
{
    int c;

    c = (unsigned char) *cursor;

    if (c == '\0' || c == '\n')
        acc_error_at(tok_line, "a %s is not closed on the line it starts on",
                     quote == '"' ? "string" : "character constant");
    cursor++;

    return c == '\\' ? escape(0) : c;
}

/* What next() does not recognise as punctuation, the quote or the stray
 * character already consumed: a character constant, which is an int -- and
 * a char's value, so '\377' is -1 where char is signed, as it is here -- a
 * string, or a character that begins nothing. Out of line, where the
 * refusal of a stray character already was, so that punctuation pays
 * nothing for literals it is not. */
static void lex_two(int c);
static void lex_digraph(int c);

__attribute__((noinline))
static void lex_quoted(int c)
{
    int n = 0;

    /* '<', '%' and ':', which begin the digraphs as well as their own
     * tokens and pairs. They come here, out of line, rather than through
     * the table next() reads the others from. Testing for the digraphs in
     * next() cost 0.7% of every compile -- not in the test, but in what it
     * did to the registers of the loop that reads a name. Here they cost a
     * call each, 0.26%, and next() compiles as it did. */
    if (c == '\\' && (*cursor == 'u' || *cursor == 'U'))
        ucn_refuse(cursor - 1);
    if (c == '<' || c == '%' || c == ':') {
        tok = c == '<' ? TK_LT : c == '%' ? TK_PERCENT : TK_COLON;
        if (*cursor == '=' || *cursor == c)
            lex_two(c);
        else if (*cursor == ':' || *cursor == '%' || *cursor == '>')
            lex_digraph(c);

        return;
    }

    if (c == '\'') {
        int value;

        if (*cursor == '\'')
            acc_error_at(tok_line, "a character constant needs a character");
        value = literal_char(c);
        if (*cursor != '\'')
            acc_error_at(tok_line, *cursor == '\n' || *cursor == '\0'
                         ? "a character constant is not closed on the line it "
                           "starts on"
                         : "a character constant holds one character; for "
                           "more, use a string");
        cursor++;
        tok = TK_INT;
        tok_type = TY_INT;
        tok_val = (signed char) value;

        return;
    }
    if (c != '"')
        punct_error(c);

    tok_str_escaped = 0;                /* escape() sets it */
    while (*cursor != '"') {
        int ch = literal_char(c);

        if (n + 1 >= str_cap) {
            str_cap = str_cap ? str_cap * 2 : 128;
            str_buf = realloc(str_buf, (size_t) str_cap);
            if (!str_buf)
                acc_error("out of memory for a string");
        }
        str_buf[n++] = (char) ch;
    }
    cursor++;
    tok = TK_STRING;
    tok_str = str_buf;
    tok_str_len = n;
    tok_str_wide = 0;
}

/* _Pragma ( string-literal ), C99 6.10.9: the string with its quotes and
 * escapes taken off, done as a #pragma would do it -- which here means
 * `once` is acted on and anything else is ignored, as C says an unknown
 * pragma is. It is a unary operator that leaves nothing behind, so the
 * token after it is read to take its place. */
static void pragma_operator(void)
{
    int once;

    next();
    if (tok != TK_LPAREN)
        acc_error_at(tok_line, "_Pragma needs a string in parentheses");
    next();
    if (tok != TK_STRING || tok_str_wide)
        acc_error_at(tok_line, "_Pragma needs a string in parentheses");
    once = tok_str_len >= 4 && !strncmp(tok_str, "once", 4);
    while (once && tok_str_len > 4 && is_space(tok_str[tok_str_len - 1]))
        tok_str_len--;
    if (once && tok_str_len == 4)
        once_add(src_real);
    next();
    if (tok != TK_RPAREN)
        acc_error_at(tok_line, "_Pragma needs a string in parentheses");
    next();
}

/* One character of a wide literal: an escape, which may be as wide as a
 * wchar_t, or a character of the source -- UTF-8, which a universal
 * character name has been made into already, taken apart into the
 * character it spells. */
static uint32_t wide_char(int quote)
{
    int c = (unsigned char) *cursor, n, i;
    uint32_t v;

    if (c == '\0' || c == '\n')
        acc_error_at(tok_line, "a %s is not closed on the line it starts on",
                     quote == '"' ? "string" : "character constant");
    cursor++;
    if (c == '\\')
        return (uint32_t) escape(1);
    if (c < 0x80)
        return (uint32_t) c;

    n = c >= 0xf0 && c < 0xf8 ? 3 : c >= 0xe0 ? 2 : c >= 0xc0 ? 1 : -1;
    if (n < 0)
        acc_error_at(tok_line, "a wide literal is read as UTF-8, and this "
                               "byte cannot begin a character in it");
    v = (uint32_t) (c & (0x3f >> n));
    for (i = 0; i < n; i++) {
        int d = (unsigned char) *cursor;

        if ((d & 0xc0) != 0x80)
            acc_error_at(tok_line, "a wide literal is read as UTF-8, and a "
                                   "character in it stops short");
        cursor++;
        v = v << 6 | (uint32_t) (d & 0x3f);
    }

    return v;
}

static void wide_unit(int *n, uint32_t v)
{
    if (*n + 2 >= str_cap) {
        str_cap = str_cap ? str_cap * 2 : 128;
        str_buf = realloc(str_buf, (size_t) str_cap);
        if (!str_buf)
            acc_error("out of memory for a string");
    }
    str_buf[(*n)++] = (char) (v & 0xff);
    str_buf[(*n)++] = (char) (v >> 8);
}

/* A wide character constant or a wide string, the L read and the cursor on
 * the quote after it (C99 6.4.4.4, 6.4.5). A wchar_t is agondev's, a short,
 * so a wide string is UTF-16 -- a character past 0xffff is the two halves
 * of a surrogate pair, as agondev writes it -- and a wide character
 * constant past it is refused, as agondev refuses it. The string's units go
 * into the same buffer as a narrow string's bytes, two bytes each, low
 * first. */
static void wide_literal(void)
{
    int quote = *cursor, n = 0;
    uint32_t v;

    cursor++;

    if (quote == '\'') {
        if (*cursor == '\'')
            acc_error_at(tok_line, "a character constant needs a character");
        v = wide_char(quote);
        if (v > 0xffff)
            acc_error_at(tok_line, "a wide character constant past 0xffff "
                                   "does not fit in a wchar_t");
        if (*cursor != '\'')
            acc_error_at(tok_line, *cursor == '\n' || *cursor == '\0'
                         ? "a character constant is not closed on the line it "
                           "starts on"
                         : "a character constant holds one character; for "
                           "more, use a string");
        cursor++;
        tok = TK_INT;
        tok_type = TY_SHORT;
        tok_val = (short) v;
        tok_val_hi = 0;

        return;
    }

    while (*cursor != '"') {
        v = wide_char(quote);
        if (v > 0xffff) {
            wide_unit(&n, 0xd800 + ((v - 0x10000) >> 10));
            wide_unit(&n, 0xdc00 + ((v - 0x10000) & 0x3ff));
        } else {
            wide_unit(&n, v);
        }
    }
    cursor++;
    tok = TK_STRING;
    tok_str = str_buf;
    tok_str_len = n;
    tok_str_wide = 1;
}

/* The second character of a two- or three-character operator, the first
 * having been read as `c` and its one-character token set. Out of line, so
 * that next() -- which runs for every token -- needs no frame: its locals
 * were only for this, and the frame was a call on every token. */
__attribute__((noinline))
static void lex_two(int c)
{
    int assign = (*cursor == '=');

    if (c == '.') {                     /* `..`, which only `...` may be */
        if (cursor[1] != '.')
            acc_error_at(tok_line, "'..' is not something C has; '...' is");
        cursor += 2;
        tok = TK_ELLIPSIS;

        return;
    }
    switch (c) {
    case '+':
        cursor++;
        tok = assign ? TK_ADD_ASSIGN : TK_INC;
        break;
    case '-':
        cursor++;
        tok = assign ? TK_SUB_ASSIGN : TK_DEC;
        break;
    case '*':
        if (assign) { cursor++; tok = TK_MUL_ASSIGN; }
        break;
    case '/':
        if (assign) { cursor++; tok = TK_DIV_ASSIGN; }
        break;
    case '%':
        if (assign) { cursor++; tok = TK_MOD_ASSIGN; }
        break;
    case '^':
        if (assign) { cursor++; tok = TK_XOR_ASSIGN; }
        break;
    case '&':
        cursor++;
        if (assign) {
            tok = TK_AND_ASSIGN;
            break;
        }
        tok = TK_ANDAND;
        if (*cursor == '=')
            punct_error('&');
        break;
    case '|':
        cursor++;
        if (assign) {
            tok = TK_OR_ASSIGN;
            break;
        }
        tok = TK_OROR;
        if (*cursor == '=')
            punct_error('|');
        break;
    case '<':
        cursor++;
        if (assign) {
            tok = TK_LE;
            break;
        }
        tok = TK_SHL;
        if (*cursor == '=') { cursor++; tok = TK_SHL_ASSIGN; }
        break;
    case '>':
        cursor++;
        if (assign) {
            tok = TK_GE;
            break;
        }
        tok = TK_SHR;
        if (*cursor == '=') { cursor++; tok = TK_SHR_ASSIGN; }
        break;
    case '=':
        if (assign) { cursor++; tok = TK_EQ; }
        break;
    case '!':
        if (assign) { cursor++; tok = TK_NE; }
        break;
    }
}

/* The digraphs, C99 6.4.6: `<:` `:>` `<%` `%>` are the brackets and braces,
 * and `%:` is '#'. The cursor is past c, on the character that may make it
 * one. A %: first on its line is a directive, read here, and the token after
 * it is read with a call back into next(), which is where this came from. */
static void lex_digraph(int c)
{
    int second = *cursor;

    if (c == '<' && second == ':') { cursor++; tok = TK_LBRACKET; return; }
    if (c == '<' && second == '%') { cursor++; tok = TK_LBRACE;   return; }
    if (c == ':' && second == '>') { cursor++; tok = TK_RBRACKET; return; }
    if (c == '%' && second == '>') { cursor++; tok = TK_RBRACE;   return; }
    if (c == '%' && second == ':') {
        if (src_macro == NAME_NONE && cursor[1] != '%'
            && at_line_start(cursor - 1)) {
            cursor--;
            directives();
            next();

            return;
        }
        acc_error_at(tok_line, "stray '%%:' in the source: it is '#', which "
                               "is only a directive first on its line");
    }
}                                       /* `<>` `:%` and the like: two tokens */

/* The parentheses after `__attribute__`, which the standard doubles: read
 * tokens and count depth until the pair that opened it closes. Recursing
 * into next() is what lets the contents be anything at all, including a
 * macro that expands to more of them. */
__attribute__((noinline))
static void skip_attribute(void)
{
    int depth = 1;

    next();
    if (tok != TK_LPAREN)
        acc_error_at(tok_line, "expected '(' after '__attribute__'");

    while (depth > 0) {
        next();
        if (tok == TK_LPAREN)
            depth++;
        else if (tok == TK_RPAREN)
            depth--;
        else if (tok == TK_EOF)
            acc_error_at(tok_line, "unterminated '__attribute__'");
    }
}

void next(void)
{
    int c;

    /* A macro comes back here rather than calling next() again. The two say
     * the same thing -- the call would re-run everything below the label --
     * but a function that calls itself is one clang gives a frame and spills
     * around, and this one is where a compile spends its time. */
restart:
    skip_space();

    /* The end of the window, which is the one place the buffer moves. The
     * test is here rather than in skip_space because every token goes
     * through that loop, and the reading itself is out of line because it
     * happens once per 16 KB. */
    if (!*cursor)
        window_more();

    tok_prev_line = tok_line;
    tok_line = line;
    c = (unsigned char) *cursor;

    if (c == '\0') {
        tok = TK_EOF;

        return;
    }

    /* A leading `.` can only begin a floating literal. A leading digit might
     * begin either, and which it is shows at the character the digits stop
     * at -- so the integer is read first and handed back if that character
     * says it was one after all. Scanning ahead to decide instead cost every
     * numeric token in the program a second pass over its digits. */
    if (c == '.' && is_digit((unsigned char) cursor[1]))
    {
        lex_floating();

        return;
    }

    if (is_digit(c)) {
        lex_number();

        return;
    }

    if (is_alpha(c)) {
        const char *s = cursor;

        while (is_alnum((unsigned char) *cursor))
            cursor++;
        tok_name = name_intern(s, (int) (cursor - s));

        if (tok_name < kw_limit) {
            tok = (unsigned char) name_arena[tok_name - 3];

            /* The names the compiler defines, whose value is not the word:
             * they are the last codes in the enum, so this is one compare
             * on the path every keyword in the program takes. */
            if (tok >= TK_FILE)
                predefined();

            /* An attribute says nothing acc acts on -- `noinline` and
             * `always_inline` are advice to an optimiser that is not here,
             * and `noreturn` only lets one be quieter about a return that
             * never happens. It is thrown away here rather than in the
             * parser because it is allowed in more places than a parser this
             * shape has hooks for: before a declaration, after it, between
             * the specifiers, on a struct member, on a parameter. Skipping
             * whole tokens also means the text inside it -- strings, commas,
             * numbers -- needs no rules of its own. */
            if (tok == TK_KW_ATTRIBUTE) {
                skip_attribute();

                goto restart;
            }

            return;
        }

        /* A name that stands for something else. The byte in front of it
         * says whether it is a macro, which for most names is the only
         * question the preprocessor ever costs them. Being a keyword is
         * already ruled out above, which is one compare this used to make
         * and no longer does. */
        tok = TK_IDENT;
        if (name_is_macro(tok_name) && expand(tok_name))
            goto restart;

        return;
    }

    cursor++;
    tok = punct[(unsigned char) c];
    if (tok == TK_EOF) {
        /* A '#' first on its line is a directive. Asking here rather than
         * before every token costs nothing at all: a name or a number has
         * already returned, and '#' is not a punctuator acc has, so an
         * unknown character is where it was going anyway.
         *
         * Whether it is first on its line is read back off the buffer --
         * walk behind it over blanks and see whether a line begins there --
         * rather than kept in a flag, which would be a store per token. The
         * characters it walks over are always there: a window only ever
         * begins where a line does, so the start of this line is either in
         * it or is the window's own start.
         *
         * Not inside a macro, whose text has no lines of its own and whose
         * '#' will mean something else once there is a '#' to mean. */
        if (c == '#' && src_macro == NAME_NONE && at_line_start(cursor - 1)) {
            cursor--;
            directives();

            goto restart;
        }
        lex_quoted(c);

        return;
    }

    /* Most punctuation can be the first of two or three characters: an
     * operator doubled (`<<`, `&&`, `++`), followed by `=` (`<=`, `+=`), or
     * both (`<<=`). Everything else is one character, so the common case is
     * two compares that both fail rather than a walk through the pairs.
     *
     * Some of what is lexed here is not implemented. It is lexed anyway,
     * because the alternative is not that it is refused later but that it is
     * misread: `a && b` becomes a bitwise and of a with the address of b, and
     * `a ++ b` becomes `a + +b`, and neither says a word about it. */
    if (*cursor == '=' || *cursor == c) {
        lex_two(c);
    } else if (c == '-' && *cursor == '>') {
        cursor++;
        tok = TK_ARROW;
    }
}

const char *tok_spelling(int token)
{
    switch (token) {
    case TK_EOF:       return "end of file";
    case TK_INT:       return "a number";
    case TK_IDENT:     return "a name";
    case TK_KW_INT:    return "'int'";
    case TK_KW_VOID:   return "'void'";
    case TK_KW_RETURN: return "'return'";
    case TK_KW_IF:     return "'if'";
    case TK_KW_ELSE:   return "'else'";
    case TK_KW_WHILE:  return "'while'";
    case TK_KW_CHAR:   return "'char'";
    case TK_KW_SHORT:  return "'short'";
    case TK_KW_LONG:   return "'long'";
    case TK_KW_SIGNED: return "'signed'";
    case TK_KW_UNSIGNED: return "'unsigned'";
    case TK_KW_FLOAT:  return "'float'";
    case TK_KW_DOUBLE: return "'double'";
    case TK_KW_FOR:    return "'for'";
    case TK_KW_GOTO:   return "'goto'";
    case TK_KW_SIZEOF: return "'sizeof'";
    case TK_KW_ENUM:   return "'enum'";
    case TK_KW_STRUCT: return "'struct'";
    case TK_KW_UNION:  return "'union'";
    case TK_KW_TYPEDEF: return "'typedef'";
    case TK_KW_STATIC: return "'static'";
    case TK_KW_EXTERN: return "'extern'";
    case TK_KW_CONST:  return "'const'";
    case TK_KW_AUTO:   return "'auto'";
    case TK_KW_REGISTER: return "'register'";
    case TK_KW_VOLATILE: return "'volatile'";
    case TK_KW_RESTRICT: return "'restrict'";
    case TK_KW_INLINE: return "'inline'";
    case TK_KW_BOOL:   return "'_Bool'";
    case TK_ELLIPSIS:  return "'...'";
    case TK_KW_VA_LIST: return "'va_list'";
    case TK_KW_VA_START: return "'va_start'";
    case TK_KW_VA_ARG: return "'va_arg'";
    case TK_KW_VA_END: return "'va_end'";
    case TK_KW_VA_COPY: return "'va_copy'";
    case TK_DOT:       return "'.'";
    case TK_ARROW:     return "'->'";
    case TK_STRING:    return "a string";
    case TK_KW_BREAK:  return "'break'";
    case TK_KW_CONTINUE: return "'continue'";
    case TK_KW_DO:     return "'do'";
    case TK_KW_SWITCH: return "'switch'";
    case TK_KW_CASE:   return "'case'";
    case TK_KW_DEFAULT: return "'default'";
    case TK_KW_RESERVED: return "a reserved word";
    case TK_FLOAT:     return "a floating-point number";
    case TK_LPAREN:    return "'('";
    case TK_RPAREN:    return "')'";
    case TK_LBRACE:    return "'{'";
    case TK_RBRACE:    return "'}'";
    case TK_SEMI:      return "';'";
    case TK_COMMA:     return "','";
    case TK_ASSIGN:    return "'='";
    case TK_PLUS:      return "'+'";
    case TK_MINUS:     return "'-'";
    case TK_STAR:      return "'*'";
    case TK_SLASH:     return "'/'";
    case TK_PERCENT:   return "'%'";
    case TK_AMP:       return "'&'";
    case TK_PIPE:      return "'|'";
    case TK_CARET:     return "'^'";
    case TK_TILDE:     return "'~'";
    case TK_ANDAND:    return "'&&'";
    case TK_OROR:      return "'||'";
    case TK_ADD_ASSIGN: return "'+='";
    case TK_SUB_ASSIGN: return "'-='";
    case TK_MUL_ASSIGN: return "'*='";
    case TK_DIV_ASSIGN: return "'/='";
    case TK_MOD_ASSIGN: return "'%='";
    case TK_AND_ASSIGN: return "'&='";
    case TK_OR_ASSIGN:  return "'|='";
    case TK_XOR_ASSIGN: return "'^='";
    case TK_SHL_ASSIGN: return "'<<='";
    case TK_SHR_ASSIGN: return "'>>='";
    case TK_QUESTION:  return "'?'";
    case TK_LBRACKET:  return "'['";
    case TK_RBRACKET:  return "']'";
    case TK_COLON:     return "':'";
    case TK_INC:       return "'++'";
    case TK_DEC:       return "'--'";
    case TK_SHL:       return "'<<'";
    case TK_SHR:       return "'>>'";
    case TK_LT:        return "'<'";
    case TK_GT:        return "'>'";
    case TK_LE:        return "'<='";
    case TK_GE:        return "'>='";
    case TK_EQ:        return "'=='";
    case TK_NE:        return "'!='";
    case TK_NOT:       return "'!'";
    }

    return "that";
}

/* The operators the lexer knows and the code generator does not, so that the
 * parser can name the missing feature rather than complain about a ';'. The
 * eZ80 has no instruction for any of them; they are the next milestone. */

int accept_next(void)
{
    next();

    return 1;
}

/* Point at where the missing token should have gone, which is the end of the
 * token before it, not the start of whatever turned up instead: a ';' left
 * off the end of line 4 is a mistake on line 4, even though the '}' that
 * reveals it is on line 5. */
void expect_failed(const char *what)
{
    acc_error_at(tok_prev_line, "expected %s, found %s", what, tok_spelling(tok));
}

void lex_init(void)
{
    keywords_init();
    name_is_macro(name_intern("L", 1)) |= NAME_WIDE;
    predefined_macros_init();
}
