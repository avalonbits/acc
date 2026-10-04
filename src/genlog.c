/*
 * opt-acc's log of what the parser asks the code generator to do.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The parser drives code generation through gen.h: a stack machine of
 * calls, each of which the classic backend turns into code as it comes. In
 * opt-acc every one of those calls made while a function is being compiled
 * is also written down, with its arguments, what it answered, and where the
 * image stood before and after it: see genlog.py, which writes the wrappers
 * from gen.h, and gen.h, which sends the parser's calls through them.
 *
 * The log is what the optimising backend will be built from
 * (docs/optimizer-plan.md). Before that can be trusted it has to be shown
 * to hold everything, and that is what this does now: at the end of each
 * function, the backend is wound back to where the function began and the
 * log is replayed into it, and the code that comes out has to be the code
 * that went in, byte for byte, with the same relocations and fixups. A
 * difference is an error, which names the call where the two parted.
 *
 * The parser writes some bytes itself -- the data of a static local, a
 * string's -- with out_byte and out_reloc, which are not calls through
 * gen.h. The log notices them as a gap between where one call left the
 * image and where the next finds it, and keeps them as a raw record: the
 * bytes and the relocations among them.
 */
#ifdef OPT_ACC

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "gen_int.h"
#include "out_int.h"
#include "genlog.h"


GenRec *gl_log;
int     gl_n;
static int gl_cap;
static char   *gl_bytes;        /* gen_data's bytes and the raw records' */
static long    gl_nbytes, gl_bytes_cap;
static int     gl_on;           /* inside a function */
static int     gl_depth;        /* calls in progress: only the outermost */
static int     gl_last;         /* out_here() when the last call returned */
static long    gl_last_relocs;  /* and how many relocations there were */
int            gl_fn;           /* the function, for messages */

/* A mark the parser made and may roll back to: by its address, the record
 * that made it; and in the replay, the mark that record made again. */
#define GL_MARKS 64
static GenMark *gl_mark_ptr[GL_MARKS];
static int      gl_mark_rec[GL_MARKS];
static int      gl_nmarks;
static GenMark *gl_replay_marks;   /* by record */

/* Where the backend stood as the function began, to be put back. */
static GenMark gl_start;

const unsigned char *gl_first_code;
int gl_first_from, gl_first_len;
int gl_backend;                 /* the last function's: see parse_int.h */
static int     gl_start_nwants, gl_start_nstatics;
static long    gl_start_nrelocs;
static int     gl_start_finish[4];
static unsigned char gl_start_marks[4096];

/* Every file's marks, saved as a function begins or put back. */
static void gl_marks(unsigned char *buf, int restore)
{
    size_t at = 0;

    at += arith_marks(buf + at, restore);
    at += branch_marks(buf + at, restore);
    at += insn_marks(buf + at, restore);
    at += func_marks(buf + at, restore);
    at += wide_marks(buf + at, restore);
    at += image_marks(buf + at, restore);
    if (at > sizeof gl_start_marks)
        acc_error("internal: the marks outgrew their buffer");
}

static int gl_mark_id(GenMark *mark, int op);
static long gl_keep(const char *bytes, int len);
static GenRec *gl_begin(int op);
static void gl_end(GenRec *rec);

#define GENLOG_OPS
#include "genlog_calls.h"
#undef GENLOG_OPS

static long gl_reloc_count(void)
{
    return (long) (out_reloc_put - out_relocs);
}

static void gl_counts(int *counts)
{
    counts[0] = nfixups;
    counts[1] = nrt_fixups;
    counts[2] = nbss_fixups;
    counts[3] = (int) gl_reloc_count();
}

static long gl_keep(const char *bytes, int len)
{
    long at = gl_nbytes;

    if (gl_nbytes + len > gl_bytes_cap) {
        gl_bytes_cap = (gl_nbytes + len) * 2 + 256;
        gl_bytes = realloc(gl_bytes, (size_t) gl_bytes_cap);
        if (!gl_bytes)
            acc_error("out of memory for the log");
    }
    memcpy(gl_bytes + gl_nbytes, bytes, (size_t) len);
    gl_nbytes += len;

    return at;
}

const char *gl_kept(long at)
{
    return gl_bytes + at;
}

static GenRec *gl_new(int op)
{
    GenRec *rec;

    if (gl_n == gl_cap) {
        gl_cap = gl_cap ? gl_cap * 2 : 1024;
        gl_log = realloc(gl_log, (size_t) gl_cap * sizeof *gl_log);
        if (!gl_log)
            acc_error("out of memory for the log");
    }
    rec = &gl_log[gl_n++];
    memset(rec, 0, sizeof *rec);
    rec->op = (unsigned short) op;

    return rec;
}

/* Bytes the parser wrote itself since the last call returned: a raw record
 * of them and of the relocations among them. */
static void gl_gap(void)
{
    int now = out_here();
    long relocs = gl_reloc_count();
    GenRec *rec;

    if (now == gl_last && relocs == gl_last_relocs)
        return;
    if (now < gl_last)
        acc_error("internal: the image went back outside a logged call");
    rec = gl_new(GL_RAW);
    rec->arg[0] = gl_last;
    rec->arg[1] = now - gl_last;
    rec->arg[2] = gl_keep((const char *) out_img + (gl_last - out_base),
                          now - gl_last);
    rec->arg[3] = relocs - gl_last_relocs;
    rec->arg[4] = gl_keep((const char *) (out_relocs + gl_last_relocs),
                          (int) ((relocs - gl_last_relocs) * sizeof *out_relocs));
    rec->at_after = now;
    gl_counts(rec->counts);
    gl_last = now;
    gl_last_relocs = relocs;
}

static int gl_mark_id(GenMark *mark, int op)
{
    int at;

    if (op == GL_gen_mark) {
        for (at = 0; at != gl_nmarks; at++)
            if (gl_mark_ptr[at] == mark)
                break;
        if (at == gl_nmarks) {
            if (gl_nmarks == GL_MARKS)
                acc_error("internal: too many marks in one function");
            gl_nmarks++;
        }
        gl_mark_ptr[at] = mark;
        gl_mark_rec[at] = gl_n - 1;

        return gl_n - 1;
    }
    for (at = 0; at != gl_nmarks; at++)
        if (gl_mark_ptr[at] == mark)
            return gl_mark_rec[at];
    acc_error("internal: a rollback to a mark the log did not see");

    return 0;
}

static GenMark *gl_replay_mark(long rec_at)
{
    return &gl_replay_marks[rec_at];
}

static void gl_function_end(void);

static GenRec *gl_begin(int op)
{
    GenRec *rec;

    if (++gl_depth != 1)
        return NULL;
    if (op == GL_gen_func_begin) {
        gl_n = 0;
        gl_nbytes = 0;
        gl_nmarks = 0;
        gl_on = 1;
        free(gl_start.saved);           /* the last function's: see gl_back */
        ssa_keep_moves();
        gen_mark(&gl_start);
        relax_state(&gl_start_nwants, &gl_start_nstatics, 0);
        gl_start_nrelocs = gl_reloc_count();
        finish_state(gl_start_finish, 0);
        gl_marks(gl_start_marks, 0);
        gl_last = out_here();
        gl_last_relocs = gl_reloc_count();
    }
    if (!gl_on)
        return NULL;
    gl_gap();
    if (op == GL_gen_func_end) {
        gl_function_end();
        gl_on = 0;

        return NULL;
    }
    rec = gl_new(op);
    rec->vtop_in = vtop;

    return rec;
}

static void gl_end(GenRec *rec)
{
    gl_depth--;
    if (!rec)
        return;
    rec->at_after = out_here();
    gl_counts(rec->counts);
    rec->vtop_out = vtop;
    if (vtop) {
        rec->top = vsp[-1];
        if (rec->top.kind == VAL_WIDE) {
            Type wide_type;

            vconst_wide(&rec->wide, &wide_type);
        }
    }
    gl_last = rec->at_after;
    gl_last_relocs = gl_reloc_count();
    if (rec->op == GL_gen_func_begin)
        gl_fn = (int) rec->arg[0];
}

/* ------------------------------------------------------------------ */
/* the replay                                                          */

static int gl_rec;              /* the record being replayed */

static void gl_differs(const char *what, long got, long want)
{
    const GenRec *rec = &gl_log[gl_rec];

    acc_error("internal: replaying %s's log, %s (record %d of %d) %s: %ld, "
              "where the first time it was %ld",
              name_text(sym_at(gl_fn)->name), gl_names[rec->op], gl_rec, gl_n,
              what, got, want);
}

static void gl_differs_byte(int at, int got, int want)
{
    char what[64];

    snprintf(what, sizeof what, "wrote at offset %d the byte", at);
    gl_differs(what, got, want);
}

static void gl_check_ret(const GenRec *rec, long got)
{
    if (got != rec->ret)
        gl_differs("answered", got, rec->ret);
}

static void gl_check_out(const GenRec *rec, int which, long got)
{
    if (got != rec->out[which])
        gl_differs("gave back", got, rec->out[which]);
}

static void gl_replay_raw(const GenRec *rec)
{
    const unsigned char *bytes = (const unsigned char *) gl_kept(rec->arg[2]);
    const int *relocs = (const int *) (const void *) gl_kept(rec->arg[4]);
    long at;

    if (out_here() != rec->arg[0])
        gl_differs("began its raw bytes at", out_here(), rec->arg[0]);
    for (at = 0; at != rec->arg[1]; at++)
        out_byte(bytes[at]);
    for (at = 0; at != rec->arg[3]; at++) {
        if (out_reloc_put == out_reloc_limit)
            out_reloc_grow();
        *out_reloc_put++ = relocs[at];
    }
}

/* GENLOG_STATS: for each function, what an SSA backend that handles only
 * int, pointer and byte values would have to leave to the classic one, and
 * why -- the first reason found. For planning, not for the build. */
static void gl_stats(void)
{
    const char *why = NULL;
    int rec_at, arg_at;

    for (rec_at = 0; rec_at != gl_n && !why; rec_at++) {
        const GenRec *rec = &gl_log[rec_at];

        switch (rec->op) {
        case GL_RAW:                why = "data"; break;
        case GL_vpush_const_long:
        case GL_vpush_const_wide:   why = "long"; break;
        case GL_vpush_const_float:  why = "float"; break;
        case GL_gen_inline_begin:   why = "inline"; break;
        case GL_gen_stack_take:     why = "vla"; break;
        case GL_vset_bits:          why = "bitfield"; break;
        default:
            break;
        }
        for (arg_at = 0; arg_at != 6 && !why; arg_at++) {
            Type type;

            if (!(gl_type_args[rec->op] & (1 << arg_at)))
                continue;
            type = (Type) rec->arg[arg_at];
            if (type_is_struct(type))
                why = "struct";
            else if (type_float(type))
                why = "float";
            else if (type_wide(type))
                why = "long";
        }
    }
    fprintf(stderr, "genlog-stats %s %s %d\n", name_text(sym_at(gl_fn)->name),
            why ? why : "ssa", gl_n);
}

static int gl_break = -1;       /* GENLOG_BREAK: a record the replay skips */

/* The log replayed into the backend, which is where the function began, and
 * each call checked against what it did the first time. */
static void gl_replay(void)
{
    static const char *const what[4] = {
        "left this many fixups", "left this many runtime calls",
        "left this many bss fixups", "left this many relocations"
    };

    gl_replay_marks = calloc((size_t) gl_n + 1, sizeof *gl_replay_marks);
    if (!gl_replay_marks)
        acc_error("out of memory for the log");
    for (gl_rec = 0; gl_rec != gl_n; gl_rec++) {
        const GenRec *rec = &gl_log[gl_rec];
        int counts[4], which;

        /* A test's way to prove the check works: the first record from
         * GENLOG_BREAK on that wrote code, left out. */
        if (gl_break >= 0 && gl_rec >= gl_break
            && rec->at_after != (gl_rec ? gl_log[gl_rec - 1].at_after
                                        : gl_start.at)) {
            gl_break = -2;
            continue;
        }
        switch (rec->op) {
        case GL_RAW:
            gl_replay_raw(rec);
            break;
#define GENLOG_REPLAY
#include "genlog_calls.h"
#undef GENLOG_REPLAY
        default:
            acc_error("internal: a record the replay does not know");
        }
        gl_counts(counts);
        if (getenv("GENLOG_TRACE"))
            fprintf(stderr, "%4d %-22s at %d/%d fix %d/%d rt %d/%d rel %d/%d\n",
                    gl_rec, gl_names[rec->op], out_here(), rec->at_after,
                    counts[0], rec->counts[0], counts[1], rec->counts[1],
                    counts[3], rec->counts[3]);
        if (out_here() != rec->at_after)
            gl_differs("left the image at", out_here(), rec->at_after);
        for (which = 0; which != 4; which++)
            if (counts[which] != rec->counts[which])
                gl_differs(what[which], counts[which], rec->counts[which]);
    }
    free(gl_replay_marks);
    gl_replay_marks = NULL;
}

/* Back to where the function began, as its first pass left things. */
static void gl_back(void)
{
    GenMark back = gl_start;

    /* gen_rollback frees the values the mark saved, and the pick goes back
     * here more than once: each time from a copy of them, the mark's own
     * kept until the next function's. */
    if (back.saved) {
        back.saved = malloc((size_t) back.vtop * sizeof *back.saved);
        if (!back.saved)
            acc_error("out of memory for the log");
        memcpy(back.saved, gl_start.saved, (size_t) back.vtop * sizeof *back.saved);
    }
    gen_rollback(&back);
    ssa_restore();
    relax_state(&gl_start_nwants, &gl_start_nstatics, 1);
    finish_state(gl_start_finish, 1);
    gl_marks(gl_start_marks, 1);
}

/* Whether the code just made from the SSA form loses to the first pass's:
 * costlier to run, or bigger; `*why` says which. */
static int ssa_loses(const char **why)
{
    if (ssa_cost_made > ssa_cost_first) {
        *why = "the first pass's code is cheaper";
        return 1;
    }
    if (ssa_size_made > ssa_size_first) {
        *why = "the first pass's code is smaller";
        return 1;
    }

    return 0;
}

/* The end of a function: the backend wound back to where it began, and the
 * function made again -- from its SSA form, when OPTACC_SSA asks for that
 * and the form can be built, and otherwise by replaying the log, which has
 * to give the same code again, byte for byte. */
static void gl_function_end(void)
{
    int from = gl_start.at, to = out_here(), len = to - from, at;
    long relocs = gl_reloc_count() - gl_start_nrelocs;
    unsigned char *first = malloc((size_t) len + 1);
    int *first_relocs = malloc((size_t) relocs * sizeof *first_relocs + 1);
    int first_fixups = nfixups, first_rt = nrt_fixups;
    int first_bss = nbss_fixups;

    gl_backend = BACKEND_FIRST;
    if (getenv("GENLOG_STATS"))
        gl_stats();
    if (gl_break == -1 && getenv("GENLOG_BREAK"))
        gl_break = atoi(getenv("GENLOG_BREAK"));

    if (!first || !first_relocs)
        acc_error("out of memory for the log");
    out_copy(from, first, len);
    gl_first_code = first;
    gl_first_from = from;
    gl_first_len = len;
    memcpy(first_relocs, out_relocs + gl_start_nrelocs,
           (size_t) relocs * sizeof *first_relocs);

    /* Back to where the function began. */
    gl_back();
    gl_on = 0;

    /* OPTACC_SSA_ONLY: one function's name, the only one made from its SSA
     * form -- for finding which function a difference is in. */
    if (getenv("OPTACC_SSA")
        && (!getenv("OPTACC_SSA_ONLY")
            || !strcmp(getenv("OPTACC_SSA_ONLY"),
                       name_text(sym_at(gl_fn)->name)))) {
        const char *why = NULL;
        int picking = !(getenv("OPTACC_PICK") && *getenv("OPTACC_PICK") == '0');
        int made, ssa_leaf_tried = 0, first_leaf, first_cached, first_refused;

        /* OPTACC_MIR: the machine-level backend is two more ways, its
         * allocator splitting intervals or not -- the first, where nothing
         * is picked, and with OPTACC_MIR_SPLIT the splitting one. */
        ssa_mir_want = !picking && getenv("OPTACC_MIR") != NULL
                       ? (getenv("OPTACC_MIR_SPLIT") ? 2 : 1) : 0;
        made = ssa_generate(&why);
        ssa_mir_want = 0;
        first_leaf = ssa_made_leaf;
        first_cached = ssa_cached_used;
        first_refused = ssa_cached_refused;
        if (made == 0 && ssa_cached_refused) {
            ssa_leaf_off = 1;           /* the hybrid path's, uncached */
            made = ssa_generate(&why);
            ssa_leaf_off = 0;
        }

        if (made < 0)
            acc_error("internal: generating %s from its SSA form: %s",
                      name_text(sym_at(gl_fn)->name), why);

        /* Made, but costlier to run than what the first pass made, or
         * bigger: back again, and that replayed instead. OPTACC_PICK=0
         * keeps it. Bigger for a gain the estimate sees was, on zap, 537
         * bytes for less than the gain the smaller code alone had: the
         * estimate weighs every path alike, and a path is not run alike.
         *
         * Every way it can be made is weighed: the leaf backend's with
         * locals cached, without, and the hybrid path's -- a function the
         * leaf backend could take was one the hybrid path had won, which
         * lost zap 131 bytes in br_byte alone, and caching made
         * macro_expand's leaf code lose where it had won. And the leaf
         * backend's caching only the locals a loop reads or writes: zap's
         * assemble_line, which takes p's address for the calls it makes,
         * keeps p in a register in its scan loops that way, 1.6% of zap's
         * time, where the pick had kept it in memory. Of those no
         * costlier and no bigger than the first pass's, the smallest, and
         * of two the same size the cheaper, is made again -- unless the
         * cheapest of them costs a tenth less than it: sieve's main, the
         * leaf backend's 13 bytes smaller and its loops a quarter slower,
         * once it could hold the long the program checks. */
        if (made > 0 && picking) {
            static const struct { int leaf_off, cache_off, mir; } ways[] = {
                { 0, CACHE_ALL, 0 }, { 0, CACHE_NONE, 0 }, { 1, CACHE_NONE, 0 },
                { 0, CACHE_LOOPS, 0 }, { 0, CACHE_NONE, 1 }, { 0, CACHE_NONE, 2 },
            };
            int nways = getenv("OPTACC_MIR") ? 6 : 4;
            const char *lost = NULL;
            int way, best = -1, best_size = 0, made_way = first_refused ? 2 : 0;
            int cheap = -1;
            long best_cost = 0, cheap_cost = 0;

            /* The first is made already: way 0, or the hybrid path's where
             * that was refused. */
            for (way = made_way; way != nways; way++) {
                const char *way_why = NULL;
                int way_made = 1;

                if ((way == 1 || way == 3) && !(first_leaf && first_cached))
                    continue;           /* the same as the first */
                if (way == 2 && way != made_way && !first_leaf)
                    continue;           /* the first was the hybrid path's */
                if (way != made_way) {
                    ssa_leaf_off = ways[way].leaf_off;
                    ssa_cache_off = ways[way].cache_off;
                    ssa_mir_want = ways[way].mir;
                    way_made = ssa_generate(&way_why);
                    ssa_leaf_off = ssa_cache_off = ssa_mir_want = 0;
                    if (ways[way].mir && way_made > 0 && !ssa_made_mir) {
                        gl_back();      /* declined: the same as another */
                        continue;
                    }
                    if (way_made < 0)
                        acc_error("internal: generating %s from its SSA form: %s",
                                  name_text(sym_at(gl_fn)->name), way_why);
                }
                if (way_made == 0)
                    continue;
                ssa_leaf_tried |= way == 2 && first_leaf;
                if (ssa_loses(&way_why)) {
                    lost = way_why;     /* the last way's, the hybrid path's */
                } else {
                    if (best < 0 || ssa_size_made < best_size
                        || (ssa_size_made == best_size
                            && ssa_cost_made < best_cost)) {
                        best = way;
                        best_size = ssa_size_made;
                        best_cost = ssa_cost_made;
                    }
                    if (cheap < 0 || ssa_cost_made < cheap_cost) {
                        cheap = way;
                        cheap_cost = ssa_cost_made;
                    }
                }
                gl_back();
            }
            if (best >= 0 && cheap_cost * 10 < best_cost * 9)
                best = cheap;           /* a tenth cheaper: see above */
            made = 0;
            why = lost ? lost : why;
            if (best >= 0) {
                ssa_leaf_off = ways[best].leaf_off;
                ssa_cache_off = ways[best].cache_off;
                ssa_mir_want = ways[best].mir;
                made = ssa_generate(&why);
                ssa_leaf_off = ssa_cache_off = ssa_mir_want = 0;
                if (made <= 0)
                    acc_error("internal: generating %s again: %s",
                              name_text(sym_at(gl_fn)->name),
                              why ? why : "nothing made");
            }
        }
        if (getenv("OPTACC_SSA_STATS"))
            fprintf(stderr, "ssa %s %s\n", name_text(sym_at(gl_fn)->name),
                    made <= 0 ? why : ssa_leaf_tried && !ssa_made_leaf
                    ? "made, not by the leaf backend" : "made");
        if (made > 0) {
            gl_backend = ssa_made_leaf ? BACKEND_LEAF : BACKEND_SSA;
            free(first);
            free(first_relocs);
            return;
        }
    }

    gl_replay();

    /* The same code, and the same things said about it. */
    gl_rec = gl_n - 1;
    if (out_here() != to)
        gl_differs("ended the function at", out_here(), to);
    for (at = 0; at != len; at++)
        if (out_img[from - out_base + at] != first[at])
            gl_differs_byte(at, out_img[from - out_base + at], first[at]);
    if (gl_reloc_count() - gl_start_nrelocs != relocs
        || memcmp(first_relocs, out_relocs + gl_start_nrelocs,
                  (size_t) relocs * sizeof *first_relocs))
        gl_differs("made different relocations, this many",
                   gl_reloc_count() - gl_start_nrelocs, relocs);
    if (nfixups != first_fixups)
        gl_differs("made this many fixups", nfixups, first_fixups);
    if (nrt_fixups != first_rt)
        gl_differs("made this many runtime calls", nrt_fixups, first_rt);
    if (nbss_fixups != first_bss)
        gl_differs("made this many bss fixups", nbss_fixups, first_bss);
    free(first);
    free(first_relocs);
}

#define GENLOG_WRAPPERS
#include "genlog_calls.h"
#undef GENLOG_WRAPPERS

#endif
