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
            return 0;           /* a directive, which the reading again would obey */
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
    for (i = 0; i != in->nids; i++)
        if (sym_find(in->ids[i]) != in->id_sym[i]
            || lex_macro_def(in->ids[i]) != in->id_macro[i])
            return NULL;

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
    vpop_reg();
    vpush_reg(R_HL);
    vset_type(type_promote(ret), ret_ext);
    gen_inline_end(lock);
    expect(TK_RPAREN, "')'");
}
