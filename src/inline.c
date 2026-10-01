/*
 * Inline functions: a body recorded as it is read, and expanded in place
 * of a call that can take it.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define ACC_FRONT       /* the parser: see gen.h */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef OPT_ACC
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "acc.h"
#include "ctype.h"
#include "fmt.h"
#include "timing.h"
#include "version.h"
#include "parse_int.h"

/* ------------------------------------------------------------------ */
/* inline functions                                                    */

/* A call to a `static inline` function whose body is one `return`, compiled
 * in place: `is_space_ch(*p)` as the test it returns, with no call, no
 * frame and no argument pushed. On the Agon a call costs sixty cycles or so
 * before the function does anything, and zap asks whether a character is a
 * space a quarter of a million times assembling BBC BASIC.
 *
 * One pass means the body is gone by the time a call is read, so its text
 * is kept: the return expression as written, which the call reads again as
 * source, with each parameter a name for a slot in the caller's scratch
 * area that the argument has been stored to -- converted, as a call would
 * convert it. What the expression makes is converted to the function's
 * type, as its return would. The function is compiled all the same, for
 * anything that takes its address or calls it where this cannot.
 *
 * Read again somewhere else, a name could mean something else: a local of
 * the caller's by that name, a macro defined since. So every name in the
 * text is kept with what it meant where the function was written -- the
 * symbol, and the macro definition -- and a call where any of them has
 * changed is an ordinary call. So is one nested too deep, which is how a
 * function that calls itself stops. Names that begin with two underscores
 * are refused outright: __LINE__ and __func__ would answer for the caller. */
#define INLINE_TEXT_MAX   240
#define INLINE_DEPTH_MAX  4

static struct Inline *inlines;
static int            ninlines, inlines_cap;
int            inline_capture;   /* the next statement may be kept */
static int            inline_depth;
static char           inline_buf[INLINE_TEXT_MAX + 2];

/* Whether a function's result and parameters are ones a call can take in
 * place: nothing wider than an int, and no struct -- what fits a scratch
 * slot and a register. */
int inline_fits(int fn, Type ret)
{
    int first = sym_params_first(fn), n = sym_nparams(fn), i;

    if (ret == TY_VOID || type_wide(ret) || type_is_struct(ret)
        || n > INLINE_PARAMS_MAX)
        return 0;
    for (i = 0; i != n; i++) {
        Type t = sym_param_type(first, i);

        if (type_wide(t) || type_is_struct(t))
            return 0;
    }

    return 1;
}

#ifdef OPT_ACC
/* Not packed by msgpack.py, which would choose its words again, and acc.bin
 * then grow for what only opt-acc says. */
static const char no_room[] = "out of memory for an inline function";

/* The same of the parameters alone, for a function that answers nothing. */
static int inline_fits_params(int fn)
{
    int first = sym_params_first(fn), n = sym_nparams(fn), i;

    if (n > INLINE_PARAMS_MAX)
        return 0;
    for (i = 0; i != n; i++) {
        Type t = sym_param_type(first, i);

        if (type_wide(t) || type_is_struct(t))
            return 0;
    }

    return 1;
}
#endif

static const struct Inline *inline_of(int fn)
{
    int i;

    for (i = 0; i != ninlines; i++)
        if (inlines[i].fn == fn)
            return &inlines[i];

    return NULL;
}

/* The names in a text, outside its strings, characters, numbers and
 * comments, each given to `each`; 0 if `each` refused one. */
static int inline_names(const char *p, const char *end,
                        int (*each)(struct Inline *, NameRef), struct Inline *in)
{
    while (p < end) {
        char c = *p;

        if (c == '"' || c == '\'') {
            for (p++; p < end && *p != c; p++)
                if (*p == '\\')
                    p++;
            p++;
        } else if (c == '/' && p + 1 < end && p[1] == '*') {
            for (p += 2; p + 1 < end && !(p[0] == '*' && p[1] == '/'); p++)
                ;
            p += 2;
        } else if (c == '/' && p + 1 < end && p[1] == '/') {
            while (p < end && *p != '\n')
                p++;
        } else if (is_digit((unsigned char) c)
                   || (c == '.' && p + 1 < end && is_digit((unsigned char) p[1]))) {
            while (p < end && (is_alnum((unsigned char) *p) || *p == '.'))
                p++;
        } else if (is_alpha((unsigned char) c) || c == '_') {
            const char *start = p;

            while (p < end && (is_alnum((unsigned char) *p) || *p == '_'))
                p++;
            if (p - start >= 2 && start[0] == '_' && start[1] == '_')
                return 0;
            if (!each(in, name_intern(start, (int) (p - start))))
                return 0;
        } else if (c == '#') {
#ifdef OPT_ACC
            /* In a whole body read again, a directive is obeyed again in
             * the same state -- its names are checked with the rest -- but
             * for #include, which would bring the file in again. */
            const char *word = p + 1;

            while (word < end && (*word == ' ' || *word == '\t'))
                word++;
            if (!in->body || (end - word >= 7 && !memcmp(word, "include", 7)))
                return 0;
            p++;
#else
            return 0;           /* a directive, which the reading again would obey */
#endif
        } else {
            p++;
        }
    }

    return 1;
}

/* A name in the kept text, where the function is defined: a parameter, or
 * a name whose meaning is kept. */
static int inline_note(struct Inline *in, NameRef name)
{
    int i;

    for (i = 0; i != in->nparams; i++)
        if (in->params[i] == name)
            return 1;
    for (i = 0; i != in->nids; i++)
        if (in->ids[i] == name)
            return 1;
    if (in->nids % 8 == 0) {
        int n = in->nids + 8;

        in->ids = realloc(in->ids, (size_t) n * sizeof *in->ids);
        in->id_sym = realloc(in->id_sym, (size_t) n * sizeof *in->id_sym);
        in->id_macro = realloc(in->id_macro, (size_t) n * sizeof *in->id_macro);
        if (!in->ids || !in->id_sym || !in->id_macro)
            acc_error("out of memory for an inline function");
    }
    in->ids[in->nids] = name;
    in->id_sym[in->nids] = sym_find(name);
    in->id_macro[in->nids] = lex_macro_def(name);
    in->nids++;

    return 1;
}

/* The body's one return, as its text from `start` to `end`, kept for calls
 * to read again. Its parameters are the function's locals, since there are
 * no others: in the order of their places in the frame. */
__attribute__((noinline))
static void inline_keep(int fn, const char *start, const char *end)
{
    struct Inline in;
    int s, i, len = (int) (end - start);
    int vals[INLINE_PARAMS_MAX];

    memset(&in, 0, sizeof in);
    in.fn = fn;
    for (s = sym_nglobal_bytes; s < sym_nbytes; s += (int) sizeof(Sym)) {
        const Sym *v = sym_at(s);

        if (v->kind != SYM_LOCAL || in.nparams == INLINE_PARAMS_MAX)
            return;
        for (i = in.nparams; i > 0 && vals[i - 1] > v->val; i--) {
            vals[i] = vals[i - 1];
            in.params[i] = in.params[i - 1];
        }
        vals[i] = v->val;
        in.params[i] = v->name;
        in.nparams++;
    }
    if (in.nparams != sym_nparams(fn))
        return;
    if (!inline_names(start, end, inline_note, &in)) {
        free(in.ids);
        free(in.id_sym);
        free(in.id_macro);

        return;
    }

    in.text = malloc((size_t) len + 2);
    if (!in.text)
        acc_error("out of memory for an inline function");
    memcpy(in.text, start, (size_t) len);
    in.text[len] = ')';
    in.text[len + 1] = '\0';
    in.len = len + 1;

    if (ninlines == inlines_cap) {
        inlines_cap = inlines_cap ? inlines_cap * 2 : 8;
        inlines = realloc(inlines, (size_t) inlines_cap * sizeof *inlines);
        if (!inlines)
            acc_error("out of memory for inline functions");
    }
    inlines[ninlines++] = in;
    sym_set_flags(fn, SYMF_INLINE);
}

/* The body's first statement, a return, when the function may be inlined:
 * compiled as ever, and its text kept if it is the only statement. */
__attribute__((noinline))
void return_kept(int line, const char *spot)
{
    char *end;

    inline_capture = 0;
    lex_record_from(inline_buf, inline_buf + INLINE_TEXT_MAX);
    next();
    if (tok != TK_SEMI)
        comma_expr();
    end = tok == TK_SEMI ? lex_record_take_semi() : (lex_record_take(), NULL);
    expect(TK_SEMI, "';'");
    gen_return(line, spot);
    if (end && tok == TK_RBRACE)
        inline_keep(current_fn, inline_buf, end);
}

/* The function's kept text, if a call here may be compiled in place. */
#ifdef OPT_ACC
/* Whether a call to fn may be compiled in place, as far as its text goes:
 * what prescan.c asks, which counts the calls a local in IY is saved
 * around, and a call compiled in place has none. */
int inline_has(int fn)
{
    return inline_of(fn) != NULL;
}
#endif

const struct Inline *inline_usable(int fn)
{
    const struct Inline *in;
    int i;

    if (!in_body || in_params || inline_depth == INLINE_DEPTH_MAX)
        return NULL;
    in = inline_of(fn);
    if (!in)
        return NULL;
    for (i = 0; i != in->nids; i++) {
#ifdef OPT_ACC
        /* In a whole body, a name nothing had where it was written is one
         * the body declares, which its declaration read again hides
         * whatever the caller has by that name. */
        if (in->body && in->id_sym[i] == SYM_NONE
            && lex_macro_def(in->ids[i]) == in->id_macro[i])
            continue;
#endif
        if (sym_find(in->ids[i]) != in->id_sym[i]
            || lex_macro_def(in->ids[i]) != in->id_macro[i])
            return NULL;
    }

    return in;
}

/* The call, with its arguments on the stack and its `)` the current token:
 * each argument into a slot as its parameter's type, the parameters' names
 * given to the slots in a scope of their own, and the text read in the
 * `)`'s place. */
__attribute__((noinline))
void inline_expand(const struct Inline *in, int fn)
{
    int first = sym_params_first(fn), n = in->nparams;
    int size = 0, lock, base, i, mark, at[INLINE_PARAMS_MAX];
    Type ret = sym_at(fn)->type, outer = narrow_dest;
    int ret_ext = sym_at(fn)->ext;

    for (i = 0; i != n; i++) {
        at[i] = size;
        size += type_size(sym_param_type(first, i));
    }
    base = gen_inline_begin(size ? size : 1, &lock);
    for (i = n - 1; i >= 0; i--) {
        vstore_local(base + at[i], sym_param_type(first, i));
        gen_discard();
    }

    mark = sym_scope_begin();
    for (i = 0; i != n; i++) {
        int s = push_here(in->params[i], SYM_LOCAL, base + at[i]);

        sym_at(s)->type = sym_param_type(first, i);
        sym_at(s)->ext = (unsigned char) sym_param_ext(first, i);
    }

    inline_depth++;
    narrow_dest = 0;
    lex_push_record(in->text, in->len);
    next();
    comma_expr();
    narrow_dest = outer;
    inline_depth--;
    sym_scope_end(mark);

    /* As the function's return would have it, and in a register: the
     * slots are given up now, and the answer is a value, not an object,
     * and not a constant a case label could take either. */
    vconvert(ret);
    vpush_reg(vpop_reg());      /* HL, unless it holds an earlier argument */
    vset_type(type_promote(ret), ret_ext);
    gen_inline_end(lock);
    expect(TK_RPAREN, "')'");
}

#ifdef OPT_ACC
/* ------------------------------------------------------------------ */
/* whole bodies in place                                               */

/* A static function called from one place in its file, and whose address
 * is not taken, compiled in place of that call: its whole body read again
 * there as a compound statement, its parameters locals the arguments are
 * stored to, and each return a store of the answer and a jump to after the
 * body. Then nothing calls the function itself, and acc leaves it out at
 * the end of the file as it does any static nothing calls -- so the call,
 * the frame and the pushes go, and the body's code is the caller's, where
 * the SSA form makes it with the caller's values in its registers.
 *
 * One place, so that the body is not written twice: which functions those
 * are is counted before the file is compiled, by a child of this process
 * that compiles it and counts the calls (inline_counts_fork). And only
 * where it makes the caller no bigger than it and the bodies were apart:
 * a second child reads every body it can in place, and a caller that grew
 * takes none. One backend makes the merged function, where apart each part
 * had the one that suited it -- suffix_bit's tests the first pass's code,
 * the loop calling it the leaf backend's -- and in zap that cost as many
 * bytes as the calls saved. */
enum { CHILD_COUNT = 1, CHILD_SIZES, CHILD_TRIAL };    /* inline_count_mode's */
enum { CHILD_NONE, CHILD_IN, CHILD_READ };      /* what inline_child gives */

int inline_count_mode;
char inline_count_out[64];

struct InlineCount {
    NameRef name;
    int     calls, address;
};

static struct InlineCount *counts;
static int ncounts, counts_cap;
static int counts_fd = -1;
static int counts_had;          /* the child answered: counts are known */

static struct InlineCount *count_of(NameRef name, int add)
{
    int at;

    for (at = 0; at != ncounts; at++)
        if (counts[at].name == name)
            return &counts[at];
    if (!add)
        return NULL;
    if (ncounts == counts_cap) {
        counts_cap = counts_cap ? counts_cap * 2 : 64;
        counts = realloc(counts, (size_t) counts_cap * sizeof *counts);
        if (!counts)
            acc_error(no_room);
    }
    counts[ncounts].name = name;
    counts[ncounts].calls = counts[ncounts].address = 0;

    return &counts[ncounts++];
}

void inline_count_call(int fn)
{
    if (inline_count_mode == CHILD_COUNT && (sym_flags(fn) & SYMF_STATIC))
        count_of(sym_at(fn)->name, 1)->calls++;
}

void inline_count_address(int fn)
{
    if (inline_count_mode == CHILD_COUNT && (sym_flags(fn) & SYMF_STATIC))
        count_of(sym_at(fn)->name, 1)->address = 1;
}

/* What the children said of each function: its bytes made with no body
 * read in place (alone), and with every body that can be (merged), and
 * which backend made each; and which bodies each took, those inside a body
 * it took among them. */
struct InlineSize {
    NameRef name;
    int     alone, merged;
    int     alone_backend, merged_backend;
};

struct InlinePair {
    NameRef caller, callee;
};

static struct InlineSize *fsizes;
static int nfsizes, fsizes_cap;
static struct InlinePair *pairs;
static int npairs, pairs_cap;
static NameRef *refused;        /* callers no body is read in place in */
static int nrefused, refused_cap;

static struct InlineSize *size_of(NameRef name, int add)
{
    int at;

    for (at = 0; at != nfsizes; at++)
        if (fsizes[at].name == name)
            return &fsizes[at];
    if (!add)
        return NULL;
    if (nfsizes == fsizes_cap) {
        fsizes_cap = fsizes_cap ? fsizes_cap * 2 : 64;
        fsizes = realloc(fsizes, (size_t) fsizes_cap * sizeof *fsizes);
        if (!fsizes)
            acc_error(no_room);
    }
    fsizes[nfsizes].name = name;
    fsizes[nfsizes].alone = fsizes[nfsizes].merged = -1;
    fsizes[nfsizes].alone_backend = fsizes[nfsizes].merged_backend = 0;

    return &fsizes[nfsizes++];
}

static void pair_add(NameRef caller, NameRef callee)
{
    if (npairs == pairs_cap) {
        pairs_cap = pairs_cap ? pairs_cap * 2 : 32;
        pairs = realloc(pairs, (size_t) pairs_cap * sizeof *pairs);
        if (!pairs)
            acc_error(no_room);
    }
    pairs[npairs].caller = caller;
    pairs[npairs].callee = callee;
    npairs++;
}

static int is_refused(NameRef caller)
{
    int at;

    for (at = 0; at != nrefused; at++)
        if (refused[at] == caller)
            return 1;

    return 0;
}

static void refuse(NameRef caller)
{
    if (is_refused(caller))
        return;
    if (nrefused == refused_cap) {
        refused_cap = refused_cap ? refused_cap * 2 : 16;
        refused = realloc(refused, (size_t) refused_cap * sizeof *refused);
        if (!refused)
            acc_error(no_room);
    }
    refused[nrefused++] = caller;
}

static void size_note(NameRef name, int mode, int size, int backend)
{
    struct InlineSize *at = size_of(name, 1);

    if (mode == CHILD_SIZES) {
        at->alone = size;
        at->alone_backend = backend;
    } else {
        at->merged = size;
        at->merged_backend = backend;
    }
}

/* A function's code made, `size` bytes of it: noted in a child, for its
 * report. */
void inline_func_done(int fn, int size)
{
    if (inline_count_mode == CHILD_SIZES || inline_count_mode == CHILD_TRIAL)
        size_note(sym_at(fn)->name, inline_count_mode, size, gl_backend);
}

/* A child that compiles the file: CHILD_IN in it, which compiles and then
 * calls inline_counts_report; CHILD_READ here, what it sent read -- or
 * CHILD_NONE, where it could not be had or failed. `mode` CHILD_COUNT
 * counts the calls, by the first pass alone, which is quick; CHILD_SIZES
 * makes every function as this one would with no body read in place, and
 * CHILD_TRIAL with every body it can. */
static int inline_child(int mode)
{
    int fds[2], status;
    pid_t child;
    char *text = NULL, *line, *next_line;
    size_t len = 0, cap = 0;
    ssize_t got;

    if (pipe(fds))
        return CHILD_NONE;
    child = fork();
    if (child < 0) {
        close(fds[0]);
        close(fds[1]);
        return CHILD_NONE;
    }
    if (child == 0) {
        close(fds[0]);
        counts_fd = fds[1];
        inline_count_mode = mode;
        snprintf(inline_count_out, sizeof inline_count_out,
                 "/tmp/opt-acc-count-%d.o", (int) getpid());
        if (mode == CHILD_COUNT)
            unsetenv("OPTACC_SSA");     /* only the calls are wanted */
        unsetenv("OPTACC_SSA_STATS");
        unsetenv("OPTACC_SSA_DUMP");
        return CHILD_IN;
    }
    close(fds[1]);
    for (;;) {
        if (len + 4096 > cap) {
            cap = (len + 4096) * 2;
            text = realloc(text, cap);
            if (!text)
                acc_error(no_room);
        }
        got = read(fds[0], text + len, cap - len - 1);
        if (got <= 0)
            break;
        len += (size_t) got;
    }
    close(fds[0]);
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status)
        || WEXITSTATUS(status) != 0 || !text) {
        free(text);
        return CHILD_NONE;              /* it failed: the compile will say why */
    }
    text[len] = '\0';
    for (line = text; *line; line = next_line) {
        int calls, address, size, backend, used = 0;
        char caller[128];

        next_line = strchr(line, '\n');
        if (!next_line)
            break;
        *next_line++ = '\0';
        if (mode == CHILD_COUNT
            && sscanf(line, "C %d %d %n", &calls, &address, &used) == 2 && used) {
            struct InlineCount *c = count_of(name_intern(line + used,
                                                         (int) strlen(line + used)), 1);

            c->calls = calls;
            c->address = address;
        } else if (sscanf(line, "S %d %d %n", &size, &backend, &used) == 2
                   && used) {
            size_note(name_intern(line + used, (int) strlen(line + used)),
                      mode, size, backend);
        } else if (sscanf(line, "I %127s %n", caller, &used) == 1 && used) {
            pair_add(name_intern(caller, (int) strlen(caller)),
                     name_intern(line + used, (int) strlen(line + used)));
        }
    }
    free(text);

    return CHILD_READ;
}

/* Each caller that took a body, and came out bigger than it and the
 * bodies it took were apart, takes none: the same calls are made as
 * calls. So does one a worse backend made than made a body it took apart:
 * lists' insert_sorted, made from its SSA form, ran 14% slower read into a
 * main the first pass made, for 36 bytes less. A size not known -- a
 * function left out, say -- is refused too. */
static void inline_decide(void)
{
    int at, other;

    for (at = 0; at != npairs; at++) {
        NameRef caller = pairs[at].caller;
        const struct InlineSize *whole = size_of(caller, 0);
        long grow;

        if (is_refused(caller))
            continue;
        if (!whole || whole->alone < 0 || whole->merged < 0) {
            refuse(caller);
            continue;
        }
        grow = whole->merged - whole->alone;
        for (other = 0; other != npairs; other++) {
            const struct InlineSize *part;

            if (pairs[other].caller != caller)
                continue;
            part = size_of(pairs[other].callee, 0);
            if (!part || part->alone < 0
                || part->alone_backend > whole->merged_backend) {
                grow = 1;
                break;
            }
            grow -= part->alone;
        }
        if (grow > 0)
            refuse(caller);
    }
}

/* Whether some static is called from one place and its address not taken:
 * a body the trial could read in place. */
static int any_candidate(void)
{
    int at;

    for (at = 0; at != ncounts; at++)
        if (counts[at].calls == 1 && !counts[at].address)
            return 1;

    return 0;
}

/* Before the file is compiled: a child to count its calls, and where any
 * body could be read in place, one to make it as it is and one to read
 * every body it can in place, while this one waits for what each sends;
 * then which callers would grow. 1 in a child; 0 here, with no body read
 * in place where a child could not be had. A file with no such body costs
 * the count's quick compile and no more: gcc's pr48141, a function of
 * 110000 statements, takes 13 minutes to make once. */
int inline_counts_fork(void)
{
    int got = inline_child(CHILD_COUNT);

    if (got == CHILD_IN)
        return 1;
    if (got == CHILD_NONE || !any_candidate())
        return 0;
    got = inline_child(CHILD_SIZES);
    if (got == CHILD_IN)
        return 1;
    if (got == CHILD_NONE)
        return 0;
    counts_had = 1;
    got = inline_child(CHILD_TRIAL);
    if (got == CHILD_IN)
        return 1;
    if (got == CHILD_NONE) {
        counts_had = 0;                 /* no sizes: nothing read in place */
        return 0;
    }
    inline_decide();

    return 0;
}

/* The child's report, written to the parent, and the child gone: the calls
 * counted, each function's size, and each body read in place, by caller. */
void inline_counts_report(void)
{
    int at;
    char line[300];

#define REPORT(...) do {                                                 \
        int n_ = snprintf(line, sizeof line, __VA_ARGS__);               \
        if (n_ > 0 && n_ < (int) sizeof line                             \
            && write(counts_fd, line, (size_t) n_) != n_)                \
            _exit(1);                                                    \
    } while (0)
    for (at = 0; at != ncounts; at++)
        REPORT("C %d %d %s\n", counts[at].calls, counts[at].address,
               name_text(counts[at].name));
    for (at = 0; at != nfsizes; at++) {
        int sizes = inline_count_mode == CHILD_SIZES;

        REPORT("S %d %d %s\n", sizes ? fsizes[at].alone : fsizes[at].merged,
               sizes ? fsizes[at].alone_backend : fsizes[at].merged_backend,
               name_text(fsizes[at].name));
    }
    for (at = 0; at != npairs; at++)
        REPORT("I %s %s\n", name_text(pairs[at].caller),
               name_text(pairs[at].callee));
#undef REPORT
    {
        char temp[sizeof inline_count_out + 2];

        snprintf(temp, sizeof temp, "%s~", inline_count_out);
        unlink(temp);
        unlink(inline_count_out);
    }
    _exit(0);
}

/* The function whose body is being kept, and the parameters it has. */
static int body_fn = SYM_NONE;
static NameRef body_params[INLINE_PARAMS_MAX];
static int body_nparams;

/* Whether a call to fn may have its whole body read in place: on, static,
 * called once in the file, its address never taken, and with parameters
 * and an answer a slot holds. */
static int body_wanted(int fn, Type ret)
{
    const struct InlineCount *c;

    if (!getenv("OPTACC_INLINE") || !counts_had || !(sym_flags(fn) & SYMF_STATIC))
        return 0;
    c = count_of(sym_at(fn)->name, 0);
    if (!c || c->calls != 1 || c->address)
        return 0;
    if (ret != TY_VOID && (type_wide(ret) || type_is_struct(ret) || type_float(ret)))
        return 0;

    return ret == TY_VOID ? inline_fits_params(fn) : inline_fits(fn, ret);
}

void inline_body_begin(int fn)
{
    int s, i;
    int vals[INLINE_PARAMS_MAX];

    body_fn = SYM_NONE;
    if (!body_wanted(fn, sym_at(fn)->type))
        return;
    body_nparams = 0;
    for (s = sym_nglobal_bytes; s < sym_nbytes; s += (int) sizeof(Sym)) {
        const Sym *v = sym_at(s);

        if (v->kind != SYM_LOCAL || body_nparams == INLINE_PARAMS_MAX)
            return;
        for (i = body_nparams; i > 0 && vals[i - 1] > v->val; i--) {
            vals[i] = vals[i - 1];
            body_params[i] = body_params[i - 1];
        }
        vals[i] = v->val;
        body_params[i] = v->name;
        body_nparams++;
    }
    if (body_nparams != sym_nparams(fn))
        return;
    body_fn = fn;
    lex_body_record_from();
}

/* A name in the body that a call cannot have read again where it is: a
 * label it would jump to, or a static it would make a second time. */
static int body_refused(struct Inline *in, NameRef name)
{
    const char *text = name_text(name);

    (void) in;

    return strcmp(text, "goto") && strcmp(text, "static")
           && strcmp(text, "asm") && strcmp(text, "__asm__");
}

void inline_body_end(void)
{
    struct Inline in;
    char *text;
    int i, len;

    if (body_fn == SYM_NONE)
        return;
    if (inline_of(body_fn)) {           /* its one return is kept already */
        (void) lex_body_record_take();
        body_fn = SYM_NONE;
        return;
    }
    text = lex_body_record_take();
    if (!text)
        goto none;
    len = (int) strlen(text);
    memset(&in, 0, sizeof in);
    in.fn = body_fn;
    in.body = 1;
    in.nparams = body_nparams;
    for (i = 0; i != body_nparams; i++)
        in.params[i] = body_params[i];
    if (!inline_names(text, text + len, body_refused, &in)
        || !inline_names(text, text + len, inline_note, &in)) {
        free(in.ids);
        free(in.id_sym);
        free(in.id_macro);
        free(text);
        goto none;
    }

    /* The body's own locals are in scope still, at its `}`: a name that is
     * one of them is the body's to declare again, and is kept as no name
     * at all -- see inline_usable. */
    for (i = 0; i != in.nids; i++)
        if (in.id_sym[i] != SYM_NONE && in.id_sym[i] >= sym_nglobal_bytes)
            in.id_sym[i] = SYM_NONE;

    /* `{` body `}` `)`: a compound statement, and the call's `)` after it,
     * which the call reads as it would its own. */
    in.text = malloc((size_t) len + 4);
    if (!in.text)
        acc_error(no_room);
    in.text[0] = '{';
    memcpy(in.text + 1, text, (size_t) len);
    in.text[len + 1] = '}';
    in.text[len + 2] = ')';
    in.text[len + 3] = '\0';
    in.len = len + 3;
    free(text);

    if (ninlines == inlines_cap) {
        inlines_cap = inlines_cap ? inlines_cap * 2 : 8;
        inlines = realloc(inlines, (size_t) inlines_cap * sizeof *inlines);
        if (!inlines)
            acc_error(no_room);
    }
    inlines[ninlines++] = in;
    sym_set_flags(body_fn, SYMF_INLINE);
none:
    body_fn = SYM_NONE;
}

/* The bodies being read in place, innermost last: each one's answer slot,
 * its type, and the jumps its returns made to after it. */
static struct {
    Type type;
    int  ext, slot, nholes, cap;
    int *holes;
} body_stack[INLINE_DEPTH_MAX];
static int body_top;

int inline_body_returning(void)
{
    return body_top > 0;
}

/* `return` in a body read in place: the answer converted and stored to the
 * slot, and a jump to after the body. */
void inline_body_return(void)
{
    int at = body_top - 1;

    next();
    if (tok != TK_SEMI) {
        comma_expr();
        if (body_stack[at].type == TY_VOID) {
            gen_discard();
        } else {
            vconvert(body_stack[at].type);
            vstore_local(body_stack[at].slot, body_stack[at].type);
            gen_discard();
        }
    }
    expect(TK_SEMI, "';'");
    gen_stmt_end();
    if (body_stack[at].nholes == body_stack[at].cap) {
        body_stack[at].cap = body_stack[at].cap ? body_stack[at].cap * 2 : 8;
        body_stack[at].holes = realloc(body_stack[at].holes,
                                       (size_t) body_stack[at].cap * sizeof (int));
        if (!body_stack[at].holes)
            acc_error(no_room);
    }
    body_stack[at].holes[body_stack[at].nholes++] = gen_jump();
}

/* The call, its `(` just read: the arguments stored to new locals as they
 * are read, the locals named as the parameters, the body read as a
 * compound statement, and its answer read from the slot its returns stored
 * to. The locals' room is given back after it (gen_local_scope). */
static void inline_body_expand(const struct Inline *in, int fn, int line,
                               const char *spot)
{
    int first = sym_params_first(fn), n = in->nparams, i, mark;
    int slots[INLINE_PARAMS_MAX];
    Type ret = sym_at(fn)->type;
    int ret_ext = sym_at(fn)->ext, at;

    mark = sym_scope_begin();
    gen_local_scope(1);
    for (i = 0; i != n; i++)
        slots[i] = gen_local(type_scalar_bytes(sym_param_type(first, i)));
    call_args_stored(fn, slots, n, line, spot);
    gen_stmt_end();
    for (i = 0; i != n; i++) {
        int s = push_here(in->params[i], SYM_LOCAL, slots[i]);

        sym_at(s)->type = sym_param_type(first, i);
        sym_at(s)->ext = (unsigned char) sym_param_ext(first, i);
    }

    at = body_top++;
    body_stack[at].type = ret;
    body_stack[at].ext = ret_ext;
    body_stack[at].slot = ret == TY_VOID ? 0 : gen_local(type_scalar_bytes(ret));
    body_stack[at].nholes = 0;

    inline_depth++;
    lex_push_record(in->text, in->len);
    next();
    inline_statement();
    inline_depth--;
    for (i = 0; i != body_stack[at].nholes; i++)
        gen_label(body_stack[at].holes[i]);
    body_top--;

    /* The answer read while its slot is the body's still, and into a
     * register: past the scope's end the slot may be another's. */
    if (ret == TY_VOID) {
        vpush_const(0, TY_INT);         /* what a void function gives */
        vcast(TY_VOID, 0, 0);
    } else {
        vpush_local(body_stack[at].slot, ret);
        vconvert(ret);
        vpush_reg(vpop_reg());
        vset_type(type_promote(ret), ret_ext);
    }
    gen_local_scope(0);                 /* its room, the next one's */
    sym_scope_end(mark);
    expect(TK_RPAREN, "')'");
}

/* Whether this call, its `(` just read, may have the body read in place:
 * one kept whole, of a function with a prototype, and nothing on the stack
 * for its arguments to go above, since the body's statements want it
 * empty. The arguments are read here. */
int inline_body_call(const struct Inline *in, int fn, int line, const char *spot)
{
    if (!in->body || gen_stack_depth() != 0 || !(sym_flags(fn) & SYMF_PARAMS)
        || is_refused(sym_at(current_fn)->name))
        return 0;
    if (inline_count_mode == CHILD_TRIAL)
        pair_add(sym_at(current_fn)->name, sym_at(fn)->name);
    inline_body_expand(in, fn, line, spot);

    return 1;
}
#endif
