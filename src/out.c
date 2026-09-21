/*
 * The output image.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

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
 * further down the file has to be patched once its address is known. The
 * image is small -- the previous compiler's output for four hundred lines of
 * input was 33 KB against 206 KB of room -- and it is not what runs the
 * machine out of memory. */
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
    int *lo = out_relocs + 1 + first, *src, *put;

    if (n <= 0)
        return;
    while (out_reloc_limit - out_reloc_put < n)
        out_reloc_grow();

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
void out_cut(const Cut *cuts, int n, int first)
{
    int *put = out_relocs + 1 + first, *scan = put;
    unsigned char *dst;
    int i;

    if (!n)
        return;

    /* The table holds offsets and the runs are addresses, so each entry goes
     * out to one and comes back as the other. */
    out_cut_rewind();
    for (; scan < out_reloc_put; scan++) {
        int at = out_cut_next(*scan + out_base);

        if (at >= 0)
            *put++ = at - out_base;
    }
    out_reloc_put = put;

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

/* `header` is whether MOS's is wanted: a program has one and an object does
 * not, its bytes being something a linker will put after a header of its own
 * making. */
void out_open(const char *path, int header)
{
    static const unsigned char hdr[HEADER_SIZE - 4] = { 0 };
    const char *base, *scan;

    cap = 4096;
    out_img = malloc(cap);
    if (!out_img)
        acc_error("out of memory for the output");
    out_put = out_img;
    out_limit = out_img + cap;
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
void out_grow(void)
{
    int used = OUT_LEN;

    /* One doubling is always enough: cap starts at 4096 and the largest
     * single write is the four bytes of out_opcode24. */
    cap *= 2;
    out_img = realloc(out_img, cap);
    if (!out_img)
        acc_error("out of memory for the output");
    out_put = out_img + used;
    out_limit = out_img + cap;
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


void out_rewind(int here)
{
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
    out_put = out_img + (here - out_base);
}

/* Bytes already written, copied back out: an initialiser that turns out to
 * be given out of order has to take what it wrote in order and put it in the
 * buffer the walk builds. */
void out_copy(int at, unsigned char *to, int len)
{
    int off = at - out_base;

    if (len <= 0)
        return;                 /* nothing written yet, and `to` may be null */
    if (off < 0 || off + len > OUT_LEN)
        acc_error("internal: a read at %06x is outside the image", at);
    memcpy(to, out_img + off, (size_t) len);
}

/* The three bytes at `at`, read back: what a relocation's slot was emitted
 * with, which for an address in a global's bytes is the amount to add to the
 * symbol's own address. */
int out_read24(int at)
{
    int off = at - out_base;

    if (off < 0 || off + 3 > OUT_LEN)
        acc_error("internal: a read at %06x is outside the image", at);

    return get24(out_img + off);
}

void out_patch24(int at, int value)
{
    int off = at - out_base;

    if (off < 0 || off + 3 > OUT_LEN)
        acc_error("internal: patch at %06x is outside the image", at);
    put24(out_img + off, value);
}

/* The image and its table let go of, without writing anything: what the
 * object writer wants, having written the same bytes itself. */
void out_free(void)
{
    free(out_img);
    out_img = NULL;
    free(out_relocs);
    out_relocs = out_reloc_put = out_reloc_limit = NULL;
}

void out_close(void)
{
    FILE *file = fopen(out_path, "wb");

    if (!file)
        acc_error("cannot write '%s'", out_path);
    if ((int) fwrite(out_img, 1, (size_t) OUT_LEN, file) != OUT_LEN)
        acc_error("short write on '%s'", out_path);
    fclose(file);
    out_free();
}
