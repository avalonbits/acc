/*
 * Taking code back out: a file's unused static functions, and a function's
 * jumps made short where their target is near.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "runtime.h"
#include "gen_int.h"

/* The file's own functions, and the run of bytes each of them is.
 *
 * A `static` at file scope is this file's alone, so if nothing in the file
 * wanted it, nothing ever can -- and it does not have to be in the image.
 * That matters more than it sounds: a header full of `static inline`
 * helpers is compiled into every file that includes it, whether that file
 * calls them or not, and acc does not inline. Six files of zap include one
 * such header and were carrying fifteen kilobytes of it each.
 *
 * They cannot be left out as they are read -- a call may come later in the
 * file -- so they are written like any other function and taken out again at
 * the end, which is what the runs below are for. */
typedef struct {
    int sym, at, len;
} StaticFn;

static StaticFn *static_fns;
static int       nstatic_fns, static_fns_cap, static_now = -1;

/* One of them holding another's address: which one said it, and which one it
 * named. Recorded where the reference is made, because that is the one place
 * both are known without looking. Read back out of the image afterwards, it
 * was a search of the relocation table for every one of them, and the search
 * cost more than the pass saved: on this chip an index into a table is a
 * multiply and the length of one is a divide, and both are calls.
 *
 * A reference from anywhere else -- a function the whole program can name, a
 * global's bytes -- is not recorded at all. It is marked on the symbol as it
 * is made, and that mark is what a walk of these starts from. */
typedef struct {
    int from, sym;
} Want;

static Want *wants;
static int   nwants, wants_cap;

void want(int fn)
{
    if (static_now < 0 || !(sym_flags(fn) & SYMF_STATIC)) {
        sym_set_flags(fn, SYMF_USED);

        return;
    }
    if (nwants == wants_cap) {
        wants_cap = wants_cap ? wants_cap * 2 : 64;
        wants = realloc(wants, (size_t) wants_cap * sizeof *wants);
        if (!wants)
            acc_error("out of memory for what the file's functions want");
    }
    wants[nwants].from = static_now;
    wants[nwants].sym = fn;
    nwants++;
}

void static_begin(int fn)
{
    static_now = -1;
    if (!(sym_flags(fn) & SYMF_STATIC))
        return;

    if (nstatic_fns == static_fns_cap) {
        static_fns_cap = static_fns_cap ? static_fns_cap * 2 : 32;
        static_fns = realloc(static_fns,
                             (size_t) static_fns_cap * sizeof *static_fns);
        if (!static_fns)
            acc_error("out of memory for the file's own functions");
    }
    static_now = nstatic_fns++;
    static_fns[static_now].sym = fn;
    static_fns[static_now].at = out_here();
}

void static_end(void)
{
    if (static_now >= 0)
        static_fns[static_now].len = out_here() - static_fns[static_now].at;
    static_now = -1;
}

/* The file's own functions that nothing in it wanted, taken back out.
 *
 * Every address acc has written down is in one of four places: the
 * relocation table, which names each slot in the image holding one; the
 * fixups, the runtime's and the bss's, which name slots that hold something
 * else until gen_finish fills them in; and the symbols. So the walk is over
 * those, and nothing else has to be told.
 *
 * It runs before anything is laid down at the end of the image -- the
 * runtime blob, the argument routine, the clearing -- so that what those are
 * placed at is the address they will keep, and so that the bss and the heap
 * begin as far down as the shortened image allows. */
/* Where in the list a symbol's function is, or -1 for one that is not the
 * file's own, through the file's own functions in the order of the symbols
 * that name them.
 *
 * static_fns is in the order the functions were defined; a symbol's number
 * is the order it was first named. A forward declaration names one function
 * before a later line defines another, and from then on the two orders
 * disagree -- so searching the definition order as though it were sorted by
 * symbol found nothing. What it could not find it took to be a function
 * outside the file, and the call stayed while the function it called was
 * dropped. That is how aed's cmd_ops.c stopped compiling.
 *
 * Built once, before the marking, and insertion-sorted because the two
 * orders agree except where a forward declaration parts them: the walk is
 * the whole of it for a file that declares nothing ahead of itself. */
static int *by_sym, by_sym_cap;

static void sort_by_sym(void)
{
    int i;

    if (nstatic_fns > by_sym_cap) {
        by_sym_cap = nstatic_fns * 2;
        by_sym = realloc(by_sym, (size_t) by_sym_cap * sizeof *by_sym);
        if (!by_sym)
            acc_error("out of memory ordering the file's own functions");
    }
    for (i = 0; i != nstatic_fns; i++) {
        int at = static_fns[i].sym, j = i;

        while (j > 0 && static_fns[by_sym[j - 1]].sym > at) {
            by_sym[j] = by_sym[j - 1];
            j--;
        }
        by_sym[j] = i;
    }
}

static int static_index(int sym)
{
    int lo = 0, hi = nstatic_fns - 1;

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int at = static_fns[by_sym[mid]].sym;

        if (sym < at)
            hi = mid - 1;
        else if (sym > at)
            lo = mid + 1;
        else
            return by_sym[mid];
    }

    return -1;
}

/* Which of the file's own functions anything that is staying still wants.
 *
 * The ones something outside them named are marked already, and each of the
 * rest is wanted only if the function that named it is itself staying -- so
 * this is a walk out from those, and not a question that can be answered one
 * function at a time. Two helpers that call each other and nothing else are
 * wanted by nobody, however loudly they say otherwise.
 *
 * The references are bucketed by who made them first, so that the walk is
 * one step per reference rather than a pass over all of them per function.
 * A function cannot want itself: the edge is not recorded when it would.
 */
static void mark_live(unsigned char *live)
{
    int *head, *next, *stack, top = 0, i;

    for (i = 0; i != nstatic_fns; i++)
        live[i] = (sym_flags(static_fns[i].sym) & SYMF_USED) != 0;
    if (!nwants)
        return;
    sort_by_sym();

    head = malloc((size_t) nstatic_fns * sizeof *head);
    next = malloc((size_t) nwants * sizeof *next);
    stack = malloc((size_t) nstatic_fns * sizeof *stack);
    if (!head || !next || !stack)
        acc_error("out of memory taking the unused functions out");

    for (i = 0; i != nstatic_fns; i++)
        head[i] = -1;
    for (i = 0; i != nwants; i++) {
        next[i] = head[wants[i].from];
        head[wants[i].from] = i;
    }

    for (i = 0; i != nstatic_fns; i++)
        if (live[i])
            stack[top++] = i;

    while (top) {
        int from = stack[--top], e;

        for (e = head[from]; e >= 0; e = next[e]) {
            int to = static_index(wants[e].sym);

            if (to < 0 || live[to])
                continue;
            live[to] = 1;
            stack[top++] = to;
        }
    }

    free(head);
    free(next);
    free(stack);
}

static int dead_statics(Cut *cuts, const unsigned char *live)
{
    int i, n = 0;

    for (i = 0; i != nstatic_fns; i++) {
        if (live[i] || !static_fns[i].len)
            continue;
        cuts[n].at = static_fns[i].at;
        cuts[n].len = static_fns[i].len;
        n++;
    }

    return n;
}

Mark func_mark;          /* where the lists stood at this function */

void mark_here(Mark *m)
{
    m->reloc = out_nrelocs();
    m->fixup = nfixups;
    m->rt = nrt_fixups;
    m->bss = nbss_fixups;
    m->jump = njumps;
}

/* The slots that do not hold an address yet, gathered and put in order.
 *
 * A call waiting on a function that is not defined holds the amount to add
 * to that function's address, one waiting on the runtime holds nothing at
 * all, and one waiting on the bss holds an offset into it. All three are
 * filled in after this pass has run, so what is in them now is not an
 * address and must not be moved as if it were.
 *
 * Only what was recorded since the function began, when it is a function's
 * jumps being shortened: everything before that is where it was, and
 * gathering it again for every function is the whole file's worth of it once
 * per function.
 *
 * The three lists are each in the order they were written, which is rising,
 * so they are merged rather than sorted: a qsort over them cost three
 * quarters of a million cycles on the Agon, which is more than the pass it
 * belongs to. The one exception is a variable said twice at file scope,
 * whose value is written back over the room the first mention reserved --
 * that puts a slot behind ones already recorded, and fixup_add says so. The
 * merge is followed by an insertion pass, which is a walk when the merge
 * came out in order and a repair when it did not. */
static int *pending, pending_cap;

static int *pending_slots(int *count, const Mark *from)
{
    int n = (nfixups - from->fixup) + (nrt_fixups - from->rt)
          + (nbss_fixups - from->bss);
    int *all, *put, b, b_end, *scan, f, f_end, *bp;
    const Fixup *fp;
    const RtFixup *r, *r_end;

    /* Nothing is waiting, which is the usual answer once each function is
     * asked about its own tail: most of them have no call left to fill in
     * and nothing in the bss. Said here rather than left to the loops
     * because the buffer is not there yet the first time through, and one
     * past the start of a buffer that is not there is not a place. */
    *count = 0;
    if (n <= 0)
        return pending;

    b = from->bss;
    b_end = nbss_fixups;
    bp = bss_fixup(b);
    f = from->fixup;
    f_end = nfixups;
    fp = fixup_at(f);
    r = rt_fixups + from->rt;
    r_end = rt_fixups + nrt_fixups;

    /* Grown and kept: see the note in out_cut_sum. */
    if (n > pending_cap) {
        pending_cap = n * 2;
        pending = realloc(pending, (size_t) pending_cap * sizeof *pending);
        if (!pending)
            acc_error("out of memory taking the unused functions out");
    }
    all = put = pending;

    /* Walked with pointers and not indexed: an index into an array of
     * anything three bytes wide is a multiply, and a multiply is a call.
     * The fixups are in blocks, and found by fixup_at, which is not. */
    while (f != f_end || r < r_end || b != b_end) {
        int x = f != f_end ? fp->at : INT_MAX;
        int y = r < r_end ? r->at : INT_MAX;
        int z = b != b_end ? *bp : INT_MAX;

        if (x <= y && x <= z) {
            *put++ = x;
            f++;
            fp = FIXUP_STEP(fp, f);
        }
        else if (y <= z)
            *put++ = r++->at;
        else {
            *put++ = z;
            b++;
            bp = BSS_STEP(bp, b);
        }
    }

    for (scan = all + 1; scan < put; scan++) {
        int at = *scan, *back = scan;

        while (back > all && back[-1] > at) {
            back[0] = back[-1];
            back--;
        }
        *back = at;
    }
    *count = (int) (put - all);

    return all;
}

/* A list of positions, with what is gone taken out of it. */
static int cut_positions(int *at, int n)
{
    int *from = at, *put = at, i;

    out_cut_rewind();
    for (i = 0; i < n; i++, from++) {
        int to = out_cut_next(*from);

        if (to >= 0)
            *put++ = to;
    }

    return (int) (put - at);
}

/* Runs of bytes taken out of the image, and everything acc knows about
 * where things are brought along.
 *
 * Two passes want this: the one that leaves out the functions a file does
 * not use, and the one that makes a jump two bytes when its target is near.
 * What they cut differs; what has to be told afterwards does not.
 *
 * `holes` says whether a run may be pointed into. It may not, for a function
 * that is going: an address inside one is a reference to something nothing
 * was said to want, which is a mistake in the marking rather than something
 * to carry on from. It may, for a jump being shortened: the run is the
 * middle of its own operand, and the slot that holds it goes with it. */
static void cut_out(Cut *cuts, int ncuts, int holes, const Mark *from)
{
    int *slots, *p, *seen_at, npending, seen = 0, i;

    /* Taking bytes out moves everything after them, as a rewind does, and
     * a mark left on bytes before this -- the code generator's marks, each
     * good only while out_rewinds is what it was -- could come to stand on
     * others: a load that ended at an address the next function's code
     * then reached was made a different instruction. Counted here and not
     * in out_cut, whose only caller this is: agondev compiled out_cut with
     * the count in it into an Agon build of acc that hung writing objects.
     * test/target.sh caught it; this is the same count. */
    out_rewinds++;

    out_cut_sum(cuts, ncuts);
    slots = pending_slots(&npending, from);
    seen_at = slots;

    /* What the slots hold, while they are still where they were written.
     *
     * The relocations are in order and so is the list of slots to leave
     * alone, so telling them apart is one step through that list per slot. */
    /* A slot may be in the file the image has gone to (out_flush), for the
     * cut that leaves the file's unused functions out: read and written
     * there, once what waits to be written in it has been. A function's
     * own jumps are all in memory. */
    if (!holes)
        out_cut_prepare();
    out_cut_rewind();
    for (p = out_relocs + 1 + from->reloc; p < out_reloc_put; p++) {
        int slot = out_base + *p, to, infile;

        while (seen < npending && *seen_at < slot)
            seen++, seen_at++;
        if (seen < npending && *seen_at == slot)
            continue;
        if (out_cut_next(slot) < 0)
            continue;                   /* going away with the code it is in */
        infile = (unsigned) *p < (unsigned) out_flushed;
        to = out_cut_moved(infile ? out_slot_get(*p) : get24(out_img + *p));
        if (to < 0) {
            if (holes)
                continue;
            acc_error("internal: %06x holds the address of a function "
                      "nothing was said to want", slot);
        }
        if (infile)
            out_slot_put(*p, to);
        else
            put24(out_img + *p, to);
    }

    out_cut(cuts, ncuts, from->reloc);

    /* And then every position and address acc is still holding. A fixup
     * inside a run goes with it: the call it was going to fill in is not
     * there any more, and neither is whatever it would have asked a library
     * for. */
    {
        int scan = from->fixup, keep = scan;
        Fixup *f = fixup_at(scan), *k = f;
        RtFixup *rscan = rt_fixups + from->rt, *rkeep = rscan;

        out_cut_rewind();
        for (; scan != nfixups; scan++, f = FIXUP_STEP(f, scan)) {
            int to = out_cut_next(f->at);

            if (to < 0)
                continue;
            f->at = to;
            *k = *f;
            keep++;
            k = FIXUP_STEP(k, keep);
        }
        nfixups = keep;

        out_cut_rewind();
        for (; rscan < rt_fixups + nrt_fixups; rscan++) {
            int to = out_cut_next(rscan->at);

            if (to < 0)
                continue;
            *rkeep = *rscan;
            rkeep->at = to;
            rkeep++;
        }
        nrt_fixups = (int) (rkeep - rt_fixups);
    }

    /* The bss's slots, which are in blocks: see bss_fixup. */
    {
        int scan = from->bss, keep = scan, *p = bss_fixup(scan), *k = p;

        out_cut_rewind();
        for (; scan != nbss_fixups; scan++, p = BSS_STEP(p, scan)) {
            int to = out_cut_next(*p);

            if (to == -1)
                continue;
            *k = to;
            keep++;
            k = BSS_STEP(k, keep);
        }
        nbss_fixups = keep;
    }

    /* The function's uses of its constants' pool, which is laid down after
     * this: none of them is inside a run, a run being a jump's operand or
     * the frame's load, so the list keeps its length. */
    if (holes && cut_positions(pool_site_at, npool_sites) != npool_sites)
        acc_error("internal: a use of a function's constants was cut out");

    /* The jumps are not brought along here. relax_function is the only
     * caller that has any, and it works out where each one landed from the
     * runs directly -- they and the jumps are both in rising order, so that
     * is a running total and not a lookup, and it saves a third walk of a
     * list that is as long as the function has jumps. */

    if (holes)
        return;                 /* a jump shortened moves nothing outside it */

    for (i = 0; i < sym_nglobals(); i += (int) sizeof(Sym)) {
        Sym *sym = sym_at(i);
        int to;

        /* Only what is at an address in this image: a constant's value is a
         * value, and a variable waiting for the bss holds an offset that is
         * negative until bss_emit turns it into one. */
        if (sym->val < 0 || sym->kind == SYM_CONST || sym->kind == SYM_TYPEDEF)
            continue;
        if (sym->kind == SYM_FUNC && !(sym_flags(i) & SYMF_DEFINED))
            continue;
        to = out_cut_moved(sym->val);
        if (to >= 0)
            sym->val = to;
        else if (sym->kind == SYM_FUNC)
            sym->val = -1;      /* one of the ones taken out: it is nowhere */
    }
}

/* The `jr` that says the same thing as a `jp`, or zero where there is none.
 * This chip has a relative jump for the four conditions the flags register
 * answers directly and none for the three a signed comparison needs. */
/* Whether a distance fits in the one signed byte a `jr` carries.
 *
 * Written as one unsigned compare rather than two signed ones, because a
 * signed compare is a call into the runtime on this chip and an unsigned
 * one is not -- the same reason out_reloc compares offsets unsigned. This
 * is asked twice for every jump the compiler writes. */
#define JR_REACHES(d) ((unsigned) ((d) + 128) <= 255u)

/* A table from JP_NZ, the lowest, to JP_M, the highest, rather than a
 * switch: this is asked twice of every jump in every function, and the
 * switch was a call and a chain of compares each time. */
static const unsigned char jr_table[JP_M - JP_NZ + 1] = {
    [JP_ANY - JP_NZ] = 0x18,
    [JP_Z - JP_NZ]   = 0x28,
    [JP_NZ - JP_NZ]  = 0x20,
    [JP_C - JP_NZ]   = 0x38,
    [JP_NC - JP_NZ]  = 0x30
};

#define jr_of(op)  (jr_table[(unsigned char) (op) - JP_NZ])

/* The jumps of the function just compiled whose target is near enough,
 * written again in two bytes.
 *
 * Done as each function ends rather than once at the end of the file, and
 * that is the whole of why it is affordable. Nothing outside a function
 * points into it -- C has no way to name a place inside one -- and nothing
 * after it has been written yet, so taking bytes out of it moves nothing
 * that is already down. What has to be brought back is the function's own
 * relocations, its own fixups and its own jumps, and each of those lists is
 * walked from where it stood when the function began.
 *
 * Done at the end of the file instead, every one of those lists had to be
 * walked whole, and every address in the image looked up in a list of
 * thousands of runs: it cost between a fifth and a half of the time it takes
 * to compile, against the four percent of the image it saves.
 *
 * The two bytes taken out are the middle of the jump's own operand, so the
 * opcode stays where it is and one byte of distance is left behind it.
 *
 * Nothing here looks a jump up in the relocation table, because a jump is
 * not in it yet: jump_op leaves the operand unrecorded and this says where
 * each surviving one ended up, in one rising run that out_reloc_merge puts
 * in among the few relocations a function makes for anything else. That is
 * what the pass costs and what it used to cost: walking the table meant
 * walking every jump a second time, and the table is nearly all jumps. */
Cut           *relax_cuts;
int           *relax_target, *relax_slot;
unsigned char *relax_short;
int            relax_cap;

/* The seven bytes each local array's address no longer needs, made lea by
 * gen_func_end: cut with the jumps, in order among them. */
int *arr_cut_at, narr_cuts, arr_cuts_cap;

/* A conditional jump over an unconditional one -- `if (x) continue;`,
 * `if (x) break;`, and a return jumping back to a constant already
 * returned -- made one jump the other way: jp z past a jp to L is jp nz to
 * L, and the jump it went over is taken out with the jumps' operands. Not
 * where anything else jumps to the one taken out. A conditional jump's
 * opposite is its opcode with bit 3 turned over, every one of them. */
#define JP_GONE 0               /* a jump taken out: see branch_over */

/* Whether any of the n jumps at `at` goes to `u`. */
static int jumped_to(const int *at, int n, int u)
{
    for (; n; n--, at++)
        if (get24(out_img + (*at + 1 - out_base)) == u)
            return 1;

    return 0;
}

static void branch_over(int *at, unsigned char *cc, int n)
{
    int i;

    /* Found first, which is a look at neighbours; the question of whether
     * anything jumps to the one jumped over is a walk of every jump, and
     * is asked only of those. Most functions have none. */
    for (i = 0; i + 1 < n; i++) {
        unsigned char *j;

        if (cc[i] == JP_ANY || cc[i + 1] != JP_ANY || at[i + 1] != at[i] + 4)
            continue;
        j = out_img + (at[i] - out_base);
        if (get24(j + 1) != at[i] + 8 || jumped_to(at, n, at[i + 1]))
            continue;
        cc[i] ^= 0x08;                  /* the other way */
        j[0] = cc[i];
        put24(j + 1, get24(j + 5));     /* to where the other went */
        cc[i + 1] = JP_GONE;
        i++;
    }
}

void relax_function(const Mark *from, int frame_at)
{
    Cut *cuts;
    int *target, *slot, *put;
    unsigned char *shrink;
    int n = njumps - from->jump, ncuts = 0, nslots = 0, i, ai = 0;

    if (n <= 0 && frame_at < 0 && !narr_cuts)
        return;

    /* Grown and kept, as everything on this path is: a function is a handful
     * of jumps and there are hundreds of functions. */
    if (n + 1 + narr_cuts > relax_cap) {
        relax_cap = (n + 1 + narr_cuts) * 2;
        relax_cuts = realloc(relax_cuts, (size_t) relax_cap * sizeof *relax_cuts);
        relax_target = realloc(relax_target,
                               (size_t) relax_cap * sizeof *relax_target);
        relax_slot = realloc(relax_slot,
                             (size_t) relax_cap * sizeof *relax_slot);
        relax_short = realloc(relax_short, (size_t) relax_cap);
        if (!relax_cuts || !relax_target || !relax_slot || !relax_short)
            acc_error("out of memory shortening the jumps");
    }
    cuts = relax_cuts;
    target = relax_target;
    slot = relax_slot;
    shrink = relax_short;

    if (n >= 2)
        branch_over(jump_at + from->jump, jump_cc + from->jump, n);

    /* Where each jump goes, and which of them will reach in one byte.
     *
     * Decided against the function as it stands, before any of them have
     * shrunk. That is the conservative answer and needs no second thought:
     * taking bytes out from between a jump and its target only brings the
     * two closer, whichever way round they are. Some that just miss would
     * come into reach once their neighbours shrink, and they are left. */
    {
        const int *at = jump_at + from->jump;
        const unsigned char *cc = jump_cc + from->jump;
        unsigned char *fits = shrink;
        Cut *cut = cuts;

        /* A frame of no bytes, which the prologue made room to set up before
         * it knew: the ld hl of its size, taken out with the jumps'
         * operands. It comes before every jump in the function, so the runs
         * are still in order, and nothing jumps into it. */
        if (frame_at >= 0) {
            cut->at = frame_at;
            cut->len = 4;
            cut++;
            ncuts++;
        }

        put = target;
        for (i = 0; i < n; i++, at++, cc++) {
            int to = get24(out_img + (*at + 1 - out_base));
            int d = to - (*at + 2);

            while (ai < narr_cuts && (unsigned) arr_cut_at[ai] < (unsigned) *at) {
                cut->at = arr_cut_at[ai++];     /* an array's, before it */
                cut->len = 7;
                cut++;
                ncuts++;
            }
            *put++ = to;
            if (*cc == JP_GONE) {
                *fits++ = 0;
                cut->at = *at;          /* all four bytes of it */
                cut->len = 4;
                cut++;
                ncuts++;
                continue;
            }
            *fits++ = jr_of(*cc) && JR_REACHES(d);
            if (!fits[-1])
                continue;
            cut->at = *at + 1;
            cut->len = 2;
            cut++;
            ncuts++;
        }
        while (ai < narr_cuts) {
            cut->at = arr_cut_at[ai++];
            cut->len = 7;
            cut++;
            ncuts++;
        }
    }

    /* The frame is cut even alone, when there is no jump to shorten: it is
     * a pass over the function's relocations and fixups, for six bytes, and
     * the bytes are what is short. */
    if (ncuts)
        cut_out(cuts, ncuts, 1, from);

    /* Each jump written where it now is, and the ones still four bytes wide
     * handed to the relocation table.
     *
     * Both of those come from a running total rather than from asking. The
     * runs are the jumps' own operands, so walking the two together in
     * rising order says how much has gone before each jump; a jump's own run
     * begins a byte after the jump does, so it is never counted into its own
     * position. And a target is a step or two from there either way -- a
     * jump that is being shortened reaches 127 bytes and no further, and one
     * that is not has a target that has moved by whatever its neighbours
     * did -- so the same cursor answers for it, walked forward or back.
     * Looking each target up among the runs instead was the single most
     * expensive thing this pass did.
     *
     * A target is never inside a run: a run is the middle of a jump's
     * operand and a label is at an instruction, so there is no case here for
     * an address that does not land anywhere. */
    {
        const int *here = jump_at + from->jump, *want = target;
        const unsigned char *cc = jump_cc + from->jump, *fits = shrink;
        const Cut *run = cuts, *run_end = cuts + ncuts;
        int gone = 0;

        put = slot;
        for (i = 0; i < n; i++, here++, cc++, want++, fits++) {
            const Cut *r;
            unsigned char *at;
            int now, to, g;

            while (run < run_end && (unsigned) run->at <= (unsigned) *here) {
                gone += run->len;
                run++;
            }
            now = *here - gone;

            r = run;
            g = gone;
            if ((unsigned) *want > (unsigned) *here)
                while (r < run_end && (unsigned) r->at <= (unsigned) *want) {
                    g += r->len;
                    r++;
                }
            else
                while (r > cuts && (unsigned) r[-1].at > (unsigned) *want) {
                    r--;
                    g -= r->len;
                }
            to = *want - g;
            at = out_img + (now - out_base);

            if (*cc == JP_GONE)
                continue;               /* taken out with its run */
            if (*fits) {
                int d = to - (now + 2);

                if (!JR_REACHES(d))
                    acc_error("internal: a jump at %06x reaches %d, which is "
                              "further than it was", now, d);
                at[0] = (unsigned char) jr_of(*cc);
                at[1] = (unsigned char) d;

                continue;
            }
            put24(at + 1, to);
            *put++ = now + 1 - out_base;
            nslots++;      /* counted, not measured: the difference of two
                            * int pointers is a divide here */
        }
    }

    out_reloc_merge(slot, nslots, from->reloc);

    /* The function is written; what its jumps were is nobody's business now. */
    jumps_rewind(from->jump);
}

void drop_unused_statics(void)
{
    static int dropped;

    /* Once: a program built in one step drops them before its link (see
     * rt_name_all), and gen_finish would ask again. */
    if (dropped)
        return;
    dropped = 1;

    unsigned char *live;
    Cut *cuts;
    int ncuts;

    if (!nstatic_fns)
        return;

    cuts = malloc((size_t) nstatic_fns * sizeof *cuts);
    live = calloc((size_t) nstatic_fns, 1);
    if (!cuts || !live)
        acc_error("out of memory taking the unused functions out");
    mark_live(live);
    ncuts = dead_statics(cuts, live);
    free(live);
    if (ncuts) {
        Mark start;

        start.reloc = start.fixup = start.rt = start.bss = start.jump = 0;
        cut_out(cuts, ncuts, 0, &start);
    }
    free(cuts);
}
