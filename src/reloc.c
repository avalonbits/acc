/*
 * Where the addresses in the image are: the relocation table, and the cuts
 * that take runs of bytes out of the image and move every address past
 * them.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "out_int.h"

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
