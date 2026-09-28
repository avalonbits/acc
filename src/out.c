/*
 * The output image.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"

/* MOS loads a program at 0x040000 and enters it at 0x040045, past a 64-byte
 * header: a jump over itself, a 60-byte name field, then the signature and a
 * byte saying the program runs in ADL mode. There is no loader beyond that --
 * no relocation, no sections -- so what is written here is what runs. */
#define LOAD_ADDR    0x040000
#define HEADER_SIZE  0x45

/* Where the image will be loaded, which is 0x040000 for a program MOS runs
 * and settable with -b so that the same program can be built for somewhere
 * else. Nothing in the image is position-independent -- every call, jump and
 * address is absolute -- so moving it means knowing where all of those are,
 * which is what the relocation table below records. */
int out_base = LOAD_ADDR;

/* Held in memory and written at the end, because a call to a function defined
 * further down the file has to be patched once its address is known -- all
 * of it, but for a link: see out_flush. */
/* Held as a write pointer and the end of the buffer rather than as an index
 * and a capacity. Every byte the compiler emits comes through out_byte, and
 * `img[len++]` is an addition of two 24-bit values before the store, where a
 * pointer that walks is a store and an increment. The bound is the same
 * either way: one compare, against the end instead of against the size. */
unsigned char        *out_img;          /* the first byte of the image */
unsigned char        *out_put;          /* where the next byte goes */
unsigned char        *out_limit;        /* one past the last it may use */
static int            cap;
static const char    *out_path;

/* The image goes to a file as it is made, a function or an object at a
 * time, rather than being held whole: see out_flush. The first
 * `out_flushed` bytes are in the file, and memory holds the rest, from
 * out_img + out_flushed: out_img is where the first byte would be, so that
 * an address is found the same way whichever part it is in. What is written
 * to a byte in the file waits in `patches`, and is made when the file is
 * copied out or, for a link, in the file itself.
 *
 * A link's file is its output, patched where it stands at the end. A
 * compile's is `<output>~`, and its bytes are copied to the output at the
 * end: an object's text comes after tables not known until then, and
 * leaving out a file's unused functions takes bytes out of the middle of
 * what is in the file (see out_cut). */
int                   out_flushed;
static FILE          *spill;            /* the file, once it is written to */
static char          *spill_name;       /* and its name */
int                   out_may_flush;    /* OUT_FLUSH_*: see out_flush */

/* Each is PATCH_WIDE bytes: the offset, the value, and the kind. Walked
 * by a pointer, as the relocations are: an index into seven-byte entries
 * is a multiply, which is a call into the runtime on this target. */
#define PATCH_WIDE  7
#define PATCH_SET8  0
#define PATCH_SET24 1
#define PATCH_ADD24 2

/* Kept in blocks of a kilobyte, one after another, and not in one array:
 * a link of zap makes two thousand, and an array that doubled to hold them
 * wanted 42 KB at once at the end of a link, when the heap is fullest. */
#define PATCH_BLOCK (146 * PATCH_WIDE)

typedef struct PatchBlock {
    struct PatchBlock *next;
    unsigned char      bytes[PATCH_BLOCK];
} PatchBlock;

static PatchBlock    *patches, *patch_last;
static unsigned char *patch_put, *patch_limit;

#ifdef ACC_TABLE_STATS
/* How often the file was written to, cut, and read back: test/spill.sh. */
int out_nflushes, out_nspill_cuts, out_nresidents;
#define COUNT(n) ((n)++)
#else
#define COUNT(n) ((void) 0)
#endif

/* What the file is read and written through, a piece at a time: each read
 * and write is a call through the whole of MOS's file layer, so a piece of
 * a kilobyte cost a compile that went to a file 5 to 9% where one of 8 KB
 * costs it 3. */
#define SLOTS_ROOM 8192

/* A piece to read the file through, of SLOTS_ROOM, or when the heap has no
 * block that big -- the end of a big compile, when it is in pieces itself
 * -- of a kilobyte: said in *room. */
static unsigned char *piece(int *room)
{
    unsigned char *buf = malloc(SLOTS_ROOM);

    *room = SLOTS_ROOM;
    if (!buf) {
        buf = malloc(1024);
        *room = 1024;
    }
    if (!buf)
        acc_error("out of memory for the output");

    return buf;
}

static void spill_io(int off, unsigned char *buf, int n, int write);

#define OUT_LEN ((int) (out_put - out_img))

/* ------------------------------------------------------------------ */
/* relocations                                                         */

/* Every three-byte slot in the image that holds an address inside the image.
 *
 * The eZ80 has no relative call and acc emits no relative jump, so an address
 * is written out in full wherever one is needed: a call, a jump, a global's
 * address loaded into a register, a function's address in a table. Each of
 * those is one entry here, as an offset from the start of the image.
 *
 * Offsets, not addresses, because that is what survives the image being put
 * somewhere else -- which is the point of recording them.
 *
 * Kept in increasing order, which out_rewind relies on to drop the ones it
 * undoes. They nearly always arrive that way -- a slot is recorded where it
 * is emitted, not where it is filled in -- so the loop that puts one in its
 * place usually does not run at all. The exceptions are real, though: a
 * variable said twice at file scope has its value written back at the address
 * the first mention reserved, which is behind everything compiled since.
 *
 * A linker needs two more things than this: which slots refer to a symbol
 * this object does not define, and what each one's addend is. Those come
 * with the object format. What is here is the half that has no other home --
 * only the code generator knows which of the numbers it writes are
 * addresses, and it knows it exactly once, as it writes them.
 *
 * Held as a walking pointer and an end, as the image itself is, and for the
 * same reason twice over. `relocs[nrelocs++]` scales an index by the width
 * of an int, which is a call into the runtime on this target; and the three
 * of them are visible so that out_reloc can be inlined, which for the same
 * measured reason as the byte emitters is what it is worth -- as a call it
 * opened a frame to do two compares and a store, and that was 0.9% of a
 * compile of test/bench/operators.c.
 *
 * Recording them at all costs about 2.7% of a compile of the heaviest
 * benchmark inputs, which is what a one-pass compiler that writes absolute
 * addresses everywhere pays to be able to move what it wrote.
 *
 * out_relocs[0] is a sentinel and not a relocation: offset zero is the first
 * byte of the header, which is never one, and having it there means the fast
 * path can read the slot behind the one it is about to write without first
 * asking whether the table is empty. The entries proper start at [1], which
 * out_nrelocs and out_reloc_at hide. */
int *out_relocs, *out_reloc_put, *out_reloc_limit;

void out_reloc_grow(void)
{
    int used = (int) (out_reloc_put - out_relocs);
    int cap = (int) (out_reloc_limit - out_relocs);

    cap = cap ? cap * 2 : 256;
    out_relocs = realloc(out_relocs, (size_t) cap * sizeof *out_relocs);
    if (!out_relocs)
        acc_error("out of memory for the relocations");
    if (!used) {
        out_relocs[0] = 0;      /* the sentinel, once there is room for it */
        used = 1;
    }
    out_reloc_put = out_relocs + used;
    out_reloc_limit = out_relocs + cap;
}

/* A slot behind the ones already recorded, which only a variable said twice
 * at file scope brings about: it goes where it belongs, so that the table
 * stays in order. */
void out_reloc_back(int at)
{
    int *scan = out_reloc_put;

    while (scan > out_relocs + 1 && scan[-1] > at) {
        scan[0] = scan[-1];
        scan--;
    }
    *scan = at;
    out_reloc_put++;
}

int out_len(void)
{
    return OUT_LEN;
}

int out_nrelocs(void)
{
    return (int) (out_reloc_put - out_relocs) - 1;
}

int out_reloc_at(int i)
{
    return out_relocs[i + 1];
}

/* The runs, in a shape an address can be looked up in cheaply.
 *
 * Cheaply is the whole difficulty. A division and a multiply are both calls
 * into the runtime on this chip, and so is an index into an array of
 * anything whose width is not a power of two -- which an array of ints is.
 * A binary search cost three thousand cycles an address between the two of
 * them, and an index by chunks with three arrays behind it cost two.
 *
 * So: the three numbers about a run -- where it starts, where it ends, and
 * how much is gone by the end of it -- lie next to each other, so that one
 * pointer stepping by three walks all of them and no index is computed. And
 * an index by 256 bytes of image says which run to start at, so the walk is
 * the handful of runs that overlap those 256 bytes. That leaves one index
 * per address looked up, and it is the only one. */
/* The index is worth having only when there are enough runs that walking
 * them all costs more than one lookup does, and a lookup here is dear: the
 * entries are three bytes wide, so `cut_first[c]` is a multiply and a call
 * into the runtime, and adding what it holds to `cut_run` is a second one.
 * Shortening one function's jumps means a handful of runs, and walking a
 * handful beats paying that twice; leaving out every function a file does
 * not use means hundreds, and then it does not. */
#define CUT_INDEX_FROM 12       /* runs, below which the walk is cheaper */

static int       *cut_run, *cut_first;
static const int *cut_stop;     /* one past the last run, kept rather than
                                 * worked out per lookup: that is a third */
static int  cut_n, cut_nchunks, cut_lo, cut_run_cap, cut_first_cap;
static int  cut_indexed;

#define CUT_AT   0
#define CUT_END  1
#define CUT_GONE 2
#define CUT_WIDE 3              /* the three of them, per run */

/* The runs a function's cuts made, given back once it is done when they
 * grew past a kilobyte: see gen_forget. */
void out_forget(void)
{
    if ((unsigned) cut_run_cap > 1024 / (CUT_WIDE * sizeof *cut_run)) {
        free(cut_run);
        cut_run = NULL;
        cut_stop = NULL;
        cut_run_cap = 0;
        cut_n = 0;
    }
    if ((unsigned) cut_first_cap > 1024 / sizeof *cut_first) {
        free(cut_first);
        cut_first = NULL;
        cut_first_cap = 0;
        cut_nchunks = 0;
    }
}

void out_cut_sum(const Cut *cuts, int n)
{
    int total = 0, i, c, *r;

    /* Grown and kept, never given back. This is asked for once per function
     * whose jumps are being shortened, and an allocator walk for each of
     * them came to more than the work it was for. */
    if (n > cut_run_cap) {
        cut_run_cap = n * 2;
        cut_run = realloc(cut_run,
                          (size_t) cut_run_cap * CUT_WIDE * sizeof *cut_run);
        if (!cut_run)
            acc_error("out of memory for the runs being taken out");
    }
    {
        const Cut *c2 = cuts;

        for (i = 0, r = cut_run; i < n; i++, r += CUT_WIDE, c2++) {
            total += c2->len;
            r[CUT_AT] = c2->at;
            r[CUT_END] = c2->at + c2->len;
            r[CUT_GONE] = total;
        }
    }
    cut_n = n;
    cut_stop = cut_run + n * CUT_WIDE;
    cut_indexed = n >= CUT_INDEX_FROM;
    if (!cut_indexed)
        return;

    /* The first run a chunk has to look at is the first one that has not
     * ended before the chunk begins, and not the first one that begins in
     * it: a run that starts in one chunk and ends in the next would
     * otherwise be skipped for every address in that next one, and an
     * address inside it would read as an address after it.
     *
     * What is kept is how far into cut_run it is rather than which run it
     * is, so that reaching it is an addition.
     *
     * Only the chunks the runs are in. Shortening one function's jumps means
     * runs within that function, and an index over the whole image would be
     * the whole image once per function. An address below the first of them
     * has nothing taken out before it and needs no index at all. */
    cut_lo = n ? (int) ((unsigned) (cuts[0].at - out_base) >> 8) : 0;
    cut_nchunks = n
        ? (int) ((unsigned) (cuts[n - 1].at - out_base) >> 8) - cut_lo + 2 : 1;
    if (cut_nchunks > cut_first_cap) {
        cut_first_cap = cut_nchunks * 2;
        cut_first = realloc(cut_first,
                            (size_t) cut_first_cap * sizeof *cut_first);
        if (!cut_first)
            acc_error("out of memory for the runs being taken out");
    }
    {
        const int *run = cut_run;
        int *first = cut_first;
        int off = 0;

        for (c = 0; c < cut_nchunks; c++, first++) {
            int start = out_base + ((c + cut_lo) << 8);

            while (off < n * CUT_WIDE && run[CUT_END] <= start)
                run += CUT_WIDE, off += CUT_WIDE;
            *first = off;
        }
    }
}

/* A rising run of slots recorded after the ones already in the table, put in
 * among them.
 *
 * A function's jumps are not recorded as they are written -- see jump_op --
 * so what relax_function hands over here is every surviving jump's operand,
 * in the order they appear, against the handful of slots the function made
 * for anything else. Two sorted runs, so this is a merge from the top down
 * and not a sort, and not out_reloc either: one slot at a time through that
 * would be one shift of the tail per jump.
 *
 * `first` is how many relocations there were when the function began.
 * Everything before that is below all of these and is not touched. */
void out_reloc_merge(const int *add, int n, int first)
{
    const int *a;
    int *lo, *src, *put;

    if (n <= 0)
        return;
    while (out_reloc_limit - out_reloc_put < n)
        out_reloc_grow();

    /* Only now, past the growing: out_reloc_grow may move the table, and a
     * `lo` taken before it pointed into the one it left. Compared against
     * the new one it was below all of it, the merge took nothing from the
     * table, and the function's jumps went on top of its other slots. The
     * table was out of order from there, in a file big enough for a
     * function's jumps not to fit the room left -- gcc's strlen-5 was one,
     * and the order of an object's relocations is a promise its format
     * makes. */
    lo = out_relocs + 1 + first;

    src = out_reloc_put - 1;
    a = add + n - 1;
    put = out_reloc_put + n - 1;
    while (a >= add) {
        if (src >= lo && *src > *a)
            *put-- = *src--;
        else
            *put-- = *a--;
    }
    out_reloc_put += n;
}

/* Where an address lands once the runs are taken out of the image, or -1 for
 * one that is inside a run and so does not land anywhere. */
/* Compared unsigned throughout: every one of these is an offset or an
 * address in the image, so none of them is ever negative, and a signed
 * compare is a call into the runtime on this chip where an unsigned one is
 * three instructions. Every address acc has recorded comes through here. */
int out_cut_moved(int a)
{
    const int *r = cut_run;
    unsigned u = (unsigned) a;

    if (!cut_n || u < (unsigned) r[CUT_AT])
        return a;               /* nothing was taken out before it */
    if (cut_indexed) {
        int c = (int) ((unsigned) (a - out_base) >> 8) - cut_lo;

        if (c >= cut_nchunks)
            c = cut_nchunks - 1;
        r += cut_first[c];
    }
    for (; r < cut_stop && (unsigned) r[CUT_AT] <= u; r += CUT_WIDE)
        if (u < (unsigned) r[CUT_END])
            return -1;

    return r == cut_run ? a : a - r[CUT_GONE - CUT_WIDE];
}

/* The same answer for addresses asked about in rising order, which is how
 * the relocations, the fixups and the jumps are walked: the run the last one
 * stopped at is where this one starts, so there is no index at all. */
static const int *cut_cursor;

void out_cut_rewind(void)
{
    cut_cursor = cut_run;
}

int out_cut_next(int a)
{
    unsigned u = (unsigned) a;

    for (; cut_cursor < cut_stop && (unsigned) cut_cursor[CUT_AT] <= u;
         cut_cursor += CUT_WIDE)
        if (u < (unsigned) cut_cursor[CUT_END])
            return -1;

    return cut_cursor == cut_run ? a : a - cut_cursor[CUT_GONE - CUT_WIDE];
}

/* Runs of bytes taken out of the middle of the image, and the relocation
 * table brought along: an entry inside a run goes with it, and every other
 * entry comes back by as much as was taken out before it.
 *
 * What the slots hold is not touched. out.c knows where the addresses in the
 * image are and not which of them are addresses yet -- a slot waiting on a
 * function that has not been defined holds nothing of the sort -- so moving
 * what is in them is the caller's, which does it before calling this.
 *
 * `first` is the relocation to start at. Everything recorded before the
 * first run is where it was, so a caller shortening the jumps of one
 * function says where that function's relocations begin and the rest of the
 * table is not walked at all -- which is what keeps doing it a function at a
 * time from being the whole table once per function. */
static void slots_done(void);

/* The runs among the first `n` that are in the file, taken out of it: what
 * follows each is copied down over it, in the file, a kilobyte at a time --
 * down, so what is read is always past what has been written. What is gone
 * from the file; the bytes in memory are then where the file ends. */
static int spill_cut(const Cut *cuts, const Cut *stop)
{
    int w = cuts->at - out_base, room;
    unsigned char *buf = piece(&room);

    COUNT(out_nspill_cuts);
    for (; cuts != stop; cuts++) {
        int r = cuts->at + cuts->len - out_base;
        int end = cuts + 1 != stop ? cuts[1].at - out_base : out_flushed;

        if ((unsigned) r > (unsigned) out_flushed)
            acc_error("internal: a run cut out of the file runs past it");
        while (r != end) {
            int k = (unsigned) (end - r) < (unsigned) room ? end - r : room;

            spill_io(r, buf, k, 0);
            spill_io(w, buf, k, 1);
            r += k;
            w += k;
        }
    }
    free(buf);

    return out_flushed - w;
}

void out_cut(const Cut *cuts, int n, int first)
{
    int *put = out_relocs + 1 + first, *scan = put;
    unsigned char *dst;
    int i, infile = 0, gone = 0;

    if (!n)
        return;
    slots_done();

    /* The table holds offsets and the runs are addresses, so each entry goes
     * out to one and comes back as the other. */
    out_cut_rewind();
    for (; scan < out_reloc_put; scan++) {
        int at = out_cut_next(*scan + out_base);

        if (at >= 0)
            *put++ = at - out_base;
    }
    out_reloc_put = put;

    /* The runs in the file first, in the file. */
    {
        const Cut *c = cuts;

        while (infile != n && (unsigned) (c->at - out_base)
                              < (unsigned) out_flushed)
            infile++, c++;
        if (infile)
            gone = spill_cut(cuts, c);
        cuts = c;
        n -= infile;
    }
    if (!n) {
        out_img += gone;
        out_flushed -= gone;

        return;
    }

    dst = out_img + (cuts[0].at - out_base);
    for (i = 0; i < n; i++, cuts++) {
        unsigned char *from = out_img + (cuts->at + cuts->len - out_base);
        unsigned char *to = i + 1 < n ? out_img + (cuts[1].at - out_base)
                                      : out_put;

        if (to > from)
            memmove(dst, from, (size_t) (to - from));
        dst += to - from;
    }
    out_put = dst;

    /* And the bytes in memory start where the file now ends. */
    out_img += gone;
    out_flushed -= gone;
}

/* The table, written out as one hexadecimal offset a line.
 *
 * A diagnostic, and the shape of what an object file will carry: it answers
 * "what would have to change for this to run somewhere else" without a
 * linker existing yet, and it is what test/reloc.sh reads to check that the
 * answer is complete. */
void out_relocs_write(const char *path)
{
    FILE *file = fopen(path, "w");
    int i;

    if (!file)
        acc_error("cannot write '%s'", path);
    for (i = 0; i < out_nrelocs(); i++)
        fprintf(file, "%06x\n", out_reloc_at(i));
    fclose(file);
}

/* Where the image lives.
 *
 * On the Agon, at the top of the heap, out of malloc's way. agondev's
 * realloc never extends a block: it mallocs the new one, copies, and frees
 * the old, so an image doubling from 32 KB held 96 KB at once -- the peak of
 * compiling one of zap's or vi's larger files -- and a link's image of a
 * hundred kilobytes could never grow to its size. At the top it grows
 * downwards by moving its bytes within what is then its own: growing costs
 * the new bytes and nothing more. malloc's break is kept below it by acc's
 * own sbrk, which is what malloc calls (the link says --wrap=_sbrk; see
 * Makefile.agon). A move changes out_img as realloc did.
 *
 * On the host, malloc and realloc: memory is not short there, and every
 * test whose program outgrows 4 KB moves the image, which is what shows up
 * code that keeps a pointer into it across a write. So too when acc builds
 * itself for the Agon (test/selfbuild.sh): that links acc's own library,
 * whose malloc is not libagon's. */
#if defined(AGONDEV) && defined(__clang__)     /* agondev's build, libagon's malloc */
extern char *_sbrkbase;                 /* libagon's break, set by crt0 */
extern char __heaptop[];                /* ___heaptop: see src/agon.ld */

static unsigned char *img_low;          /* the image's first byte, or null */

/* malloc's sbrk: libagon's, with the image's bottom as the top of the heap.
 * As libagon's does, it answers null when there is no room, and keeps the
 * break strictly below the top. */
void *_wrap__sbrk(int incr)
{
    char *was = _sbrkbase;
    char *top = img_low ? (char *) img_low : __heaptop;

    /* Unsigned: neither can be negative, and a signed compare is a call
     * to repair the flags after it. */
    if ((unsigned) incr >= (unsigned) (top - was))
        return NULL;
    _sbrkbase = was + incr;

    return was;
}

static unsigned char *img_take(int want)
{
    unsigned char *low = (unsigned char *) __heaptop - want;

    if (low <= (unsigned char *) _sbrkbase)
        return NULL;
    img_low = low;

    return low;
}

/* Twice as big, or failing that as big as there is room for, so long as
 * that is a kilobyte more. */
static unsigned char *img_grow(unsigned char *img, int used, int *capp)
{
    int want = *capp * 2;
    unsigned char *low = (unsigned char *) __heaptop - want;

    if (low <= (unsigned char *) _sbrkbase) {
        low = (unsigned char *) _sbrkbase + 1;
        want = (int) ((unsigned char *) __heaptop - low);
        if ((unsigned) want < (unsigned) *capp + 1024)
            return NULL;
    }
    memmove(low, img, (size_t) used);
    img_low = low;
    *capp = want;

    return low;
}

static void img_free(unsigned char *img)
{
    (void) img;
    img_low = NULL;
}
#else
static unsigned char *img_take(int want)
{
    return malloc((size_t) want);
}

static unsigned char *img_grow(unsigned char *img, int used, int *capp)
{
    (void) used;
    *capp *= 2;

    return realloc(img, (size_t) *capp);
}

static void img_free(unsigned char *img)
{
    free(img);
}
#endif

extern int out_start_cap;

static void hold(unsigned char *held, int used);

/* `header` is whether MOS's is wanted: a program has one and an object does
 * not, its bytes being something a linker will put after a header of its own
 * making. */
void out_open(const char *path, int header)
{
    static const unsigned char hdr[HEADER_SIZE - 4] = { 0 };
    const char *base, *scan;

    cap = out_start_cap;
    hold(img_take(cap), 0);
    out_reloc_grow();                   /* so the sentinel is there to read */
    out_path = path;
    if (!header)
        return;

    /* jp 0x040045, over the header */
    out_byte(0xc3);
    out_reloc(out_base + 1);
    out_word24(out_base + HEADER_SIZE);

    memcpy(out_put, hdr, sizeof hdr);
    out_put += sizeof hdr;

    /* The name MOS lists the program under, which is the file it is written
     * to. agondev has a separate step for this; there is no reason for one. */
    base = path;
    for (scan = path; *scan; scan++)
        if (*scan == '/' || *scan == '\\')
            base = scan + 1;
    {
        size_t name_len = strlen(base);

        if (name_len > 0x40 - 4 - 1)
            name_len = 0x40 - 4 - 1;
        memcpy(out_img + 4, base, name_len);
    }
    out_img[0x40] = 'M';
    out_img[0x41] = 'O';
    out_img[0x42] = 'S';
    out_img[0x43] = 0;        /* header version */
    out_img[0x44] = 1;        /* ADL, 24-bit addressing */
}

/* Kept out of the emitters in acc.h, which are inlined wherever an
 * instruction is written. Inline, its `cap *= 2` and its error string would
 * give every one of those places a frame slot to serve a path taken a dozen
 * times in a compile. Out of line, what is inlined is a compare and a store.
 *
 * The emitters used to be functions here, and an earlier note said inlining
 * them was slower. That was before the output became a walking pointer, and
 * it is no longer true: each call opened a frame to do a compare and a store,
 * and doing without the calls took big.c from 871 to 848 cycles a byte. */
/* Where the image starts: 16 KB on the Agon, which is most programs' whole
 * image.
 *
 * It grows by doubling, and on the Agon a growth holds both copies at once:
 * agondev's realloc never extends a block, it mallocs the new one, copies,
 * and frees the old. It used to start at 64 KB so that it would never grow,
 * and those 64 KB came out of every compile however small its output --
 * enough that no file including <agon/vdp.h> could be compiled on the
 * machine, and that 7 of aed's 23 did not fit. 16 KB still leaves an image
 * that outgrows it to double, and a thousand-function source of 53 KB still
 * compiles (test/cycles.sh); the headers and 20 of aed's files now do too
 * (test/headers.sh).
 *
 * On the host, 4 KB, so that the tests move the image often: that is what
 * shows up code keeping a pointer into it across a write. test/test_out.c
 * changes it to see it grow. */
#ifdef AGONDEV
int out_start_cap = 16384;
#else
int out_start_cap = 4096;
#endif

int out_capacity(void)
{
    return cap;
}

/* The image as `held`, of cap bytes, `used` of them written: past the
 * `out_flushed` bytes in the file. */
static void hold(unsigned char *held, int used)
{
    if (!held)
        acc_error("out of memory for the output");
    out_img = held - out_flushed;
    out_put = held + used;
    out_limit = held + cap;
}

void out_grow(void)
{
    unsigned char *held = out_img + out_flushed;
    int used = (int) (out_put - held);

    /* One doubling is enough for any one write: cap starts at 4096 or more
     * and the largest is the four bytes of out_opcode24. */
    hold(img_grow(held, used, &cap), used);
}

/* The file the image goes to, opened the first time it is needed: the
 * output itself for a link, `<output>~` for a compile. */
static void spill_open(void)
{
    size_t n = strlen(out_path);

    spill_name = (char *) out_path;
    if (out_may_flush == OUT_FLUSH_COPY) {
        spill_name = malloc(n + 2);
        if (!spill_name)
            acc_error("out of memory for the output");
        memcpy(spill_name, out_path, n);
        spill_name[n] = '~';
        spill_name[n + 1] = '\0';
    }
    spill = fopen(spill_name, "w+b");
    if (!spill)
        acc_error("cannot write '%s'", spill_name);
}

/* `n` bytes at `off` in the file, read into `buf`, or with `write`
 * written from it. */
static void spill_io(int off, unsigned char *buf, int n, int write)
{
    if (fseek(spill, off, SEEK_SET) != 0
        || (int) (write ? fwrite(buf, 1, (size_t) n, spill)
                        : fread(buf, 1, (size_t) n, spill)) != n)
        acc_error("short %s on '%s'", write ? "write" : "read", spill_name);
}

/* How much of the image memory holds before out_flush writes it to the
 * file. On the Agon 32 KB: a compile whose image is smaller -- most of
 * them, and every input of test/bench.sh -- never writes one and pays
 * nothing, and one that is bigger has a file of a hundred kilobytes and
 * more to go to. On the host 2 KB, so that the tests go to a file all the
 * time: they are what shows the file's bytes to be the image's. */
#ifdef AGONDEV
int out_flush_at = 32768;
#else
int out_flush_at = 2048;
#endif

/* What memory holds of the image, written to its file and let go of once
 * it is out_flush_at. Called where nothing written
 * so far will be written again but through out_patch and out_add24, or read
 * but through out_resident: between a link's objects, and at the end of
 * each function a compile writes -- a function's jumps, its constants and
 * its frame are all settled within it.
 *
 * The image starts again at its first size, and on the Agon what it grew to
 * goes back to malloc. */
void out_flush(void)
{
    unsigned char *held = out_img + out_flushed;
    int used = (int) (out_put - held);

    if (!out_may_flush || (unsigned) used < (unsigned) out_flush_at)
        return;
    if (!spill)
        spill_open();
    COUNT(out_nflushes);
    spill_io(out_flushed, held, used, 1);
    out_flushed += used;
    img_free(held);
    cap = out_start_cap;
    hold(img_take(cap), 0);
}

/* Before the file is read back or cut: nothing waiting to be written in
 * it, since a patch says where its bytes are now. A compile fills its
 * slots only at its end (gen_finish), after both, so none is waiting; one
 * that was would be written in the wrong place, and is refused. */
void out_cut_prepare(void)
{
    if (out_flushed && patches)
        acc_error("internal: the file is moved with patches waiting on it");
}

/* The image from `at` on, in memory again: what a global defined a second
 * time writes over (out_seek), or what a variable moved out of the bss
 * reads back (gen_bss_move). Both are rare, and both may reach bytes a
 * function ago; so the file's tail from `at` is read back in and taken off
 * it. */
void out_resident(int at)
{
    int off = at - out_base, need = out_flushed - off;
    unsigned char *held = out_img + out_flushed;
    int used = (int) (out_put - held);

    if ((unsigned) off >= (unsigned) out_flushed)
        return;
    COUNT(out_nresidents);
    out_cut_prepare();
    while ((unsigned) (cap - used) < (unsigned) need)
        hold(held = img_grow(held, used, &cap), used);
    memmove(held + need, held, (size_t) used);
    spill_io(off, held, need, 0);
    out_img += need;
    out_put += need;
    out_flushed = off;
}

/* The three bytes of a slot in the file, for the walk that moves every
 * address when a file's unused functions are left out (cut_out): read and
 * written through a kilobyte of the file held here, since the walk goes
 * through the slots in order. */
static unsigned char *slots;
static int            slots_at, slots_len, slots_dirty, slots_room;

static void slots_write_back(void)
{
    if (slots_dirty)
        spill_io(slots_at, slots, slots_len, 1);
    slots_dirty = 0;
}

static unsigned char *slot_bytes(int off)
{
    if (!slots_len
        || (unsigned) (off - slots_at) > (unsigned) (slots_len - 3)) {
        slots_write_back();
        if (!slots)
            slots = piece(&slots_room);
        slots_at = off;
        slots_len = (unsigned) (out_flushed - off) < (unsigned) slots_room
                    ? out_flushed - off : slots_room;
        spill_io(off, slots, slots_len, 0);
    }

    return slots + (off - slots_at);
}

int out_slot_get(int off)
{
    return get24(slot_bytes(off));
}

void out_slot_put(int off, int value)
{
    put24(slot_bytes(off), value);
    slots_dirty = 1;
}

/* Done with the slots: what changed written back, and the kilobyte let go. */
static void slots_done(void)
{
    slots_write_back();
    free(slots);
    slots = NULL;
    slots_len = 0;
}





/* Two bytes and three bytes, which is what nearly every instruction acc emits
 * is made of: a prefix and an opcode, or those and a displacement.
 *
 * Written as repeated out_byte calls they cost a call and a bounds check
 * each. The check is the same one either way -- the buffer either has room
 * for the whole instruction or it does not -- and the call is pure overhead
 * on the path that runs once per byte of every program compiled. */


void out_word24(int value)
{
    /* One bounds check and three stores, rather than three calls that each
     * check. Every call instruction and every loaded constant emits one of
     * these, so it is a third of the output path. */
    if ((unsigned) (out_limit - out_put) < 3)
        out_grow();
    put24(out_put, value);
    out_put += 3;
}


/* How many times the image has been taken back. A mark something has left
 * on bytes it wrote -- see vcmp's -- is good only while this is what it was
 * when the mark was made: taken back past the mark and written again, the
 * image can come back to the very place the mark ends, with other bytes
 * before it. */
unsigned out_rewinds;

/* The lowest address the image has been taken back to since whoever reads
 * this last set it high: which marks a rewind reached, where out_rewinds
 * says only that there was one. */
int out_rewind_floor = INT_MAX;

/* Told of every rewind, if set: what the code generator keeps that a
 * rewind can take back. A pointer rather than a call, so that out.c stands
 * on its own. */
void (*out_on_rewind)(int here);

void out_rewind(int here)
{
    if ((unsigned) (here - out_base) < (unsigned) out_flushed)
        acc_error("internal: a rewind to %06x, which is in the file", here);
    out_rewinds++;
    if (here < out_rewind_floor)
        out_rewind_floor = here;
    if (out_on_rewind)
        out_on_rewind(here);
    /* The slots recorded in what is being undone go with it. The table is
     * in increasing order, so the ones to drop are the last ones. */
    while (out_reloc_put > out_relocs + 1 && out_reloc_put[-1] >= here - out_base)
        out_reloc_put--;
    out_put = out_img + (here - out_base);
}

/* Back to an address already written, to write over it: a variable said twice
 * at file scope, whose value goes where the first mention reserved room for
 * it. Nothing is being undone -- everything compiled since stays, relocations
 * and all -- so this is not out_rewind, which forgets. */
void out_seek(int here)
{
    out_resident(here);
    out_put = out_img + (here - out_base);
}

/* Bytes already written, copied back out: an initialiser that turns out to
 * be given out of order has to take what it wrote in order and put it in the
 * buffer the walk builds. */
void out_copy(int at, unsigned char *to, int len)
{
    int off;

    if (len <= 0)
        return;                 /* nothing written yet, and `to` may be null */
    out_resident(at);
    off = at - out_base;
    if (off < out_flushed || off + len > OUT_LEN)
        acc_error("internal: a read at %06x is outside the image", at);
    memcpy(to, out_img + off, (size_t) len);
}

/* The three bytes at `at`, read back: what a relocation's slot was emitted
 * with, which for an address in a global's bytes is the amount to add to the
 * symbol's own address. */
int out_read24(int at)
{
    int off = at - out_base;

    out_resident(at);
    if (off < out_flushed || off + 3 > OUT_LEN)
        acc_error("internal: a read at %06x is outside the image", at);

    return get24(out_img + off);
}

/* A patch made: `value` written to the bytes at `at`, or added to them. */
static void patch_apply(unsigned char *at, int value, int kind)
{
    if (kind == PATCH_SET8)
        *at = (unsigned char) value;
    else
        put24(at, kind == PATCH_SET24 ? value : get24(at) + value);
}

/* A patch to the image at `at`: made, or for bytes in the file already,
 * kept for out_close. A slot is never split between the two: a link
 * flushes between objects, and no slot is split across two of them. */
static void patch(int at, int value, int kind)
{
    int off = at - out_base, width = kind == PATCH_SET8 ? 1 : 3;

    if ((unsigned) off >= (unsigned) OUT_LEN
        || (unsigned) (OUT_LEN - off) < (unsigned) width
        || ((unsigned) off < (unsigned) out_flushed
            && (unsigned) (off + width) > (unsigned) out_flushed))
        acc_error("internal: patch at %06x is outside the image", at);
    if ((unsigned) off >= (unsigned) out_flushed) {
        patch_apply(out_img + off, value, kind);

        return;
    }
    if (patch_put == patch_limit) {
        PatchBlock *b = malloc(sizeof *b);

        if (!b)
            acc_error("out of memory for the patches to the output");
        b->next = NULL;
        if (patch_last)
            patch_last->next = b;
        else
            patches = b;
        patch_last = b;
        patch_put = b->bytes;
        patch_limit = b->bytes + PATCH_BLOCK;
    }
    put24(patch_put, off);
    put24(patch_put + 3, value);
    patch_put[6] = (unsigned char) kind;
    patch_put += PATCH_WIDE;
}

/* One byte and two, for what an assembler's objects ask of a slot. */
void out_patch8(int at, int value)
{
    patch(at, value, PATCH_SET8);
}

void out_patch16(int at, int value)
{
    out_patch8(at, value & 0xff);
    out_patch8(at + 1, (value >> 8) & 0xff);
}

void out_patch24(int at, int value)
{
    patch(at, value, PATCH_SET24);
}

/* `delta` added to the three bytes at `at`: an address filled in on top of
 * what its slot was emitted with, which may be in the file by then. */
void out_add24(int at, int delta)
{
    patch(at, delta, PATCH_ADD24);
}

/* The image and its table let go of, without writing anything: what the
 * object writer wants, having written the same bytes itself. */
static void patches_free(void)
{
    while (patches) {
        PatchBlock *next = patches->next;

        free(patches);
        patches = next;
    }
    patch_last = NULL;
    patch_put = patch_limit = NULL;
}

/* The file's bytes from 0 to out_flushed, with the patches waiting on them
 * made: written to `to`, or with `to` null back where they are, a piece at a
 * time through `buf`, of `room`. Each patch is made in the order it was
 * made, in the piece it starts in, short of the piece's last two bytes;
 * those are carried to the front of the next piece -- with what a patch
 * that started before them wrote in them -- so a slot is never split. */
static void sweep(FILE *to, unsigned char *buf, int room)
{
    int from = 0, have = 0;

    while (from != out_flushed) {
        int n = out_flushed - from, stop;
        const PatchBlock *b;

        if ((unsigned) n > (unsigned) room)
            n = room;
        spill_io(from + have, buf + have, n - have, 0);
        stop = from + n == out_flushed ? out_flushed : from + n - 2;
        for (b = patches; b; b = b->next) {
            const unsigned char *p = b->bytes;
            const unsigned char *end = b == patch_last ? patch_put
                                                       : p + PATCH_BLOCK;

            for (; p != end; p += PATCH_WIDE)
                if ((unsigned) (get24(p) - from) < (unsigned) (stop - from))
                    patch_apply(buf + (get24(p) - from), get24(p + 3), p[6]);
        }
        if (!to)
            spill_io(from, buf, stop - from, 1);
        else if ((int) fwrite(buf, 1, (size_t) (stop - from), to) != stop - from)
            acc_error("short write on '%s'", out_path);
        have = from + n - stop;
        memmove(buf, buf + (stop - from), (size_t) have);
        from = stop;
    }
}

/* The image, written to `to`: from the file and then from memory, or all
 * from memory when none of it went to a file. */
void out_write_text(void *to)
{
    unsigned char *held = out_img + out_flushed;
    int used = (int) (out_put - held);

    if (spill) {
        int room;
        unsigned char *buf = piece(&room);

        sweep((FILE *) to, buf, room);
        free(buf);
    }
    if (used && (int) fwrite(held, 1, (size_t) used, (FILE *) to) != used)
        acc_error("short write on '%s'", out_path);
}

/* A compile's file, which is copied out and no longer wanted. */
static void spill_remove(void)
{
    if (!spill)
        return;
    fclose(spill);
    spill = NULL;
    remove(spill_name);
    if (spill_name != out_path)
        free(spill_name);
    spill_name = NULL;
}

void out_free(void)
{
    if (out_may_flush == OUT_FLUSH_COPY)
        spill_remove();
    img_free(out_img + out_flushed);
    out_img = NULL;
    out_flushed = 0;
    patches_free();
    free(out_relocs);
    out_relocs = out_reloc_put = out_reloc_limit = NULL;
}

#ifdef ACC_TABLE_STATS
/* How many patches wait for out_close: see test/linkstream.sh. */
int out_npatches(void)
{
    const PatchBlock *b;
    int n = 0;

    for (b = patches; b; b = b->next)
        n += (int) ((b == patch_last ? patch_put : b->bytes + PATCH_BLOCK)
                    - b->bytes) / PATCH_WIDE;

    return n;
}
#endif

void out_close(void)
{
    unsigned char *held = out_img + out_flushed;
    int used = (int) (out_put - held);
    FILE *file;

    /* A link's file is the output: the rest written after it, and the
     * patches made where they stand. */
    if (spill && out_may_flush == OUT_FLUSH_IN_PLACE) {
        spill_io(out_flushed, held, used, 1);
        sweep(NULL, held, cap);
        fclose(spill);
        spill = NULL;
        out_free();

        return;
    }
    file = fopen(out_path, "wb");
    if (!file)
        acc_error("cannot write '%s'", out_path);
    out_write_text(file);
    fclose(file);
    out_free();
}

/* A compile or a link that fails leaves no program: what it wrote is not
 * one. */
void out_abandon(void)
{
    if (!spill)
        return;
    fclose(spill);
    spill = NULL;
    remove(spill_name);
}
