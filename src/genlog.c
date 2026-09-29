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

typedef struct {
    unsigned short op;
    long a[6];                  /* the arguments, in order */
    long out[4];                /* what each pointer argument came back as */
    long ret;
    int  at_after;              /* out_here() when it returned */
    int  counts[4];             /* and the fixups, runtime calls, bss
                                 * fixups and relocations there were */
} GenRec;

static GenRec *gl_log;
static int     gl_n, gl_cap;
static char   *gl_bytes;        /* gen_data's bytes and the raw records' */
static long    gl_nbytes, gl_bytes_cap;
static int     gl_on;           /* inside a function */
static int     gl_depth;        /* calls in progress: only the outermost */
static int     gl_last;         /* out_here() when the last call returned */
static long    gl_last_relocs;  /* and how many relocations there were */
static int     gl_fn;           /* the function, for messages */

/* A mark the parser made and may roll back to: by its address, the record
 * that made it; and in the replay, the mark that record made again. */
#define GL_MARKS 64
static GenMark *gl_mark_ptr[GL_MARKS];
static int      gl_mark_rec[GL_MARKS];
static int      gl_nmarks;
static GenMark *gl_replay_marks;   /* by record */

/* Where the backend stood as the function began, to be put back. */
static GenMark gl_start;
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

static int gl_mark_id(GenMark *m, int op);
static long gl_keep(const char *bytes, int len);
static const char *gl_kept(long at);
static GenRec *gl_begin(int op);
static void gl_end(GenRec *r);

#define GENLOG_OPS
#include "genlog_calls.h"
#undef GENLOG_OPS

static long gl_reloc_count(void)
{
    return (long) (out_reloc_put - out_relocs);
}

static void gl_counts(int *c)
{
    c[0] = nfixups;
    c[1] = nrt_fixups;
    c[2] = nbss_fixups;
    c[3] = (int) gl_reloc_count();
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

static const char *gl_kept(long at)
{
    return gl_bytes + at;
}

static GenRec *gl_new(int op)
{
    GenRec *r;

    if (gl_n == gl_cap) {
        gl_cap = gl_cap ? gl_cap * 2 : 1024;
        gl_log = realloc(gl_log, (size_t) gl_cap * sizeof *gl_log);
        if (!gl_log)
            acc_error("out of memory for the log");
    }
    r = &gl_log[gl_n++];
    memset(r, 0, sizeof *r);
    r->op = (unsigned short) op;

    return r;
}

/* Bytes the parser wrote itself since the last call returned: a raw record
 * of them and of the relocations among them. */
static void gl_gap(void)
{
    int now = out_here();
    long relocs = gl_reloc_count();
    GenRec *r;

    if (now == gl_last && relocs == gl_last_relocs)
        return;
    if (now < gl_last)
        acc_error("internal: the image went back outside a logged call");
    r = gl_new(GL_RAW);
    r->a[0] = gl_last;
    r->a[1] = now - gl_last;
    r->a[2] = gl_keep((const char *) out_img + (gl_last - out_base),
                      now - gl_last);
    r->a[3] = relocs - gl_last_relocs;
    r->a[4] = gl_keep((const char *) (out_relocs + gl_last_relocs),
                      (int) ((relocs - gl_last_relocs) * sizeof *out_relocs));
    r->at_after = now;
    gl_counts(r->counts);
    gl_last = now;
    gl_last_relocs = relocs;
}

static int gl_mark_id(GenMark *m, int op)
{
    int i;

    if (op == GL_gen_mark) {
        for (i = 0; i != gl_nmarks; i++)
            if (gl_mark_ptr[i] == m)
                break;
        if (i == gl_nmarks) {
            if (gl_nmarks == GL_MARKS)
                acc_error("internal: too many marks in one function");
            gl_nmarks++;
        }
        gl_mark_ptr[i] = m;
        gl_mark_rec[i] = gl_n - 1;

        return gl_n - 1;
    }
    for (i = 0; i != gl_nmarks; i++)
        if (gl_mark_ptr[i] == m)
            return gl_mark_rec[i];
    acc_error("internal: a rollback to a mark the log did not see");

    return 0;
}

static GenMark *gl_replay_mark(long rec)
{
    return &gl_replay_marks[rec];
}

static void gl_replay(void);

static GenRec *gl_begin(int op)
{
    GenRec *r;

    if (++gl_depth != 1)
        return NULL;
    if (op == GL_gen_func_begin) {
        gl_n = 0;
        gl_nbytes = 0;
        gl_nmarks = 0;
        gl_on = 1;
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
        gl_replay();
        gl_on = 0;

        return NULL;
    }
    r = gl_new(op);

    return r;
}

static void gl_end(GenRec *r)
{
    gl_depth--;
    if (!r)
        return;
    r->at_after = out_here();
    gl_counts(r->counts);
    gl_last = r->at_after;
    gl_last_relocs = gl_reloc_count();
    if (r->op == GL_gen_func_begin)
        gl_fn = (int) r->a[0];
}

/* ------------------------------------------------------------------ */
/* the replay                                                          */

static int gl_rec;              /* the record being replayed */

static void gl_differs(const char *what, long got, long want)
{
    const GenRec *r = &gl_log[gl_rec];

    acc_error("internal: replaying %s's log, %s (record %d of %d) %s: %ld, "
              "where the first time it was %ld",
              name_text(sym_at(gl_fn)->name), gl_names[r->op], gl_rec, gl_n,
              what, got, want);
}

static void gl_differs_byte(int at, int got, int want)
{
    char what[64];

    snprintf(what, sizeof what, "wrote at offset %d the byte", at);
    gl_differs(what, got, want);
}

static void gl_check_ret(const GenRec *r, long got)
{
    if (got != r->ret)
        gl_differs("answered", got, r->ret);
}

static void gl_check_out(const GenRec *r, int i, long got)
{
    if (got != r->out[i])
        gl_differs("gave back", got, r->out[i]);
}

static void gl_replay_raw(const GenRec *r)
{
    const unsigned char *bytes = (const unsigned char *) gl_kept(r->a[2]);
    const int *relocs = (const int *) (const void *) gl_kept(r->a[4]);
    long i;

    if (out_here() != r->a[0])
        gl_differs("began its raw bytes at", out_here(), r->a[0]);
    for (i = 0; i != r->a[1]; i++)
        out_byte(bytes[i]);
    for (i = 0; i != r->a[3]; i++) {
        if (out_reloc_put == out_reloc_limit)
            out_reloc_grow();
        *out_reloc_put++ = relocs[i];
    }
}

static int gl_break = -1;       /* GENLOG_BREAK: a record the replay skips */

static void gl_replay(void)
{
    int from = gl_start.at, to = out_here(), n = to - from, i;
    long relocs = gl_reloc_count() - gl_start_nrelocs;
    unsigned char *first = malloc((size_t) n + 1);
    int *first_relocs = malloc((size_t) relocs * sizeof *first_relocs + 1);
    int first_fixups = nfixups, first_rt = nrt_fixups;
    int first_bss = nbss_fixups;

    if (gl_break == -1 && getenv("GENLOG_BREAK"))
        gl_break = atoi(getenv("GENLOG_BREAK"));

    if (!first || !first_relocs)
        acc_error("out of memory for the log");
    out_copy(from, first, n);
    memcpy(first_relocs, out_relocs + gl_start_nrelocs,
           (size_t) relocs * sizeof *first_relocs);

    /* Back to where the function began. */
    gen_rollback(&gl_start);
    relax_state(&gl_start_nwants, &gl_start_nstatics, 1);
    finish_state(gl_start_finish, 1);
    gl_marks(gl_start_marks, 1);

    gl_replay_marks = calloc((size_t) gl_n + 1, sizeof *gl_replay_marks);
    if (!gl_replay_marks)
        acc_error("out of memory for the log");
    gl_on = 0;
    for (gl_rec = 0; gl_rec != gl_n; gl_rec++) {
        const GenRec *r = &gl_log[gl_rec];

        /* A test's way to prove the check works: the first record from
         * GENLOG_BREAK on that wrote code, left out. */
        if (gl_break >= 0 && gl_rec >= gl_break
            && r->at_after != (gl_rec ? gl_log[gl_rec - 1].at_after
                                      : gl_start.at)) {
            gl_break = -2;
            continue;
        }
        switch (r->op) {
        case GL_RAW:
            gl_replay_raw(r);
            break;
#define GENLOG_REPLAY
#include "genlog_calls.h"
#undef GENLOG_REPLAY
        default:
            acc_error("internal: a record the replay does not know");
        }
        if (getenv("GENLOG_TRACE")) {
            int c[4];

            gl_counts(c);
            fprintf(stderr, "%4d %-22s at %d/%d fix %d/%d rt %d/%d rel %d/%d\n",
                    gl_rec, gl_names[r->op], out_here(), r->at_after, c[0],
                    r->counts[0], c[1], r->counts[1], c[3], r->counts[3]);
        }
        if (out_here() != r->at_after)
            gl_differs("left the image at", out_here(), r->at_after);
        {
            static const char *const what[4] = {
                "left this many fixups", "left this many runtime calls",
                "left this many bss fixups", "left this many relocations"
            };
            int c[4], k;

            gl_counts(c);
            for (k = 0; k != 4; k++)
                if (c[k] != r->counts[k])
                    gl_differs(what[k], c[k], r->counts[k]);
        }
    }
    free(gl_replay_marks);
    gl_replay_marks = NULL;

    /* The same code, and the same things said about it. */
    gl_rec = gl_n - 1;
    if (out_here() != to)
        gl_differs("ended the function at", out_here(), to);
    for (i = 0; i != n; i++)
        if (out_img[from - out_base + i] != first[i])
            gl_differs_byte(i, out_img[from - out_base + i], first[i]);
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
