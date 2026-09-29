/*
 * Which local of a function lives in IY, chosen before its body is compiled.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * A local in IY is read and written in a register where every other one is
 * loaded from its frame slot at each use and stored back (see iy_local). It
 * has to be chosen before its first use is compiled, and acc compiles in one
 * pass, so the body is read here first as text: not compiled, not lexed as
 * the compiler lexes it, only walked from its `{` to the matching `}`
 * counting what each name is used for.
 *
 * Wanted: the local or parameter used most inside loops, a use in a loop
 * worth eight outside it and one in a loop inside a loop sixty-four -- and a
 * body with no loop is not walked at all. Ruled out: a name after an `&`
 * that is not `&&` or `&=`, since IY has no address; and every name in a
 * body that uses a macro that may take one (see prescan_macro), or defines
 * or includes anything. Every call the local would be live across saves IY
 * around it, so the calls are counted too, and a local used less than they
 * cost is not chosen. What a name is -- a local, a keyword, a function -- is
 * asked only of the few that might be chosen and the ones called; the walk
 * knows names by their text.
 *
 * This is opt-acc's alone (see docs/optimizer-plan.md): acc compiles in
 * one pass on the Agon, and a second reading of every body is more than
 * its budget allows, where opt-acc runs on the host and has none. acc's
 * register keyword still claims IY first. The text is the window the lexer
 * has (see source.c) and, past its end, the rest of the file read ahead, a
 * whole number of lines at a time, and then sought back to.
 */
#ifdef OPT_ACC                  /* opt-acc's alone; acc compiles it to nothing */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "lex_int.h"
#include "gen_int.h"

/* prescan_body's choices, best first, which prescan_claim takes as they
 * are declared. */
NameRef iy_wants[4];
int     iy_nwants;

/* ------------------------------------------------------------------ */
/* macros                                                              */

/* A name's hash, as the walk works it out: its bytes, doubled and added. */
static unsigned char ps_hash_of(const char *t, unsigned char *len)
{
    unsigned char h = 0, n = 0;

    while (t[n]) {
        h = (unsigned char) ((h << 1) + (unsigned char) t[n]);
        n++;
    }
    *len = n;

    return h;
}

/* Which hashes a macro that may take an address has: a bit for each. A
 * name whose bit is clear is not one of them, and costs the walk nothing
 * more; one whose bit is set is asked of the macro table. Bits are only
 * ever set. */
unsigned char ps_risk[32];

static void ps_risky(Macro *m)
{
    unsigned char len, h = ps_hash_of(name_text(m->name), &len);

    m->amp = 1;
    ps_risk[h >> 3] |= (unsigned char) (1 << (h & 7));
}

/* Whether a macro's text has an `&` that could take an address, or names a
 * macro that may. An `&` after a name, a number, `)` or `]` is an AND,
 * which takes nothing; one after anything else, or at the start, may. */
static int macro_text_amp(const char *t)
{
    char prev = '(';                    /* the last byte that was not a space */

    for (; *t; t++) {
        if (*t == '&') {
            if (t[1] == '&' || t[1] == '=') {
                t++;
            } else if (!(prev == ')' || prev == ']' || prev == '_'
                         || ((prev | 0x20) >= 'a' && (prev | 0x20) <= 'z')
                         || (prev >= '0' && prev <= '9'))) {
                return 1;
            }
            prev = '&';
        } else if (*t == '_' || ((*t | 0x20) >= 'a' && (*t | 0x20) <= 'z')) {
            const char *s = t;
            unsigned char h = 0;
            NameRef n;
            Macro *m;

            while (*t == '_' || ((*t | 0x20) >= 'a' && (*t | 0x20) <= 'z')
                   || (*t >= '0' && *t <= '9')) {
                h = (unsigned char) ((h << 1) + (unsigned char) *t);
                t++;
            }
            if (ps_risk[h >> 3] & (1 << (h & 7))) {
                n = name_intern(s, (int) (t - s));
                m = macro_find(n);
                if (m && m->amp)
                    return 1;
            }
            t--;
            prev = 'a';
        } else if (*t != ' ' && *t != '\t') {
            prev = *t;
        }
    }

    return 0;
}

/* Whether a macro's text names `name`, as a whole word. */
static int macro_names(const char *t, const char *name, size_t len)
{
    const char *at = t;

    while ((at = strstr(at, name)) != NULL) {
        char before = at == t ? ' ' : at[-1], after = at[len];

        if (!(before == '_' || ((before | 0x20) >= 'a' && (before | 0x20) <= 'z')
              || (before >= '0' && before <= '9'))
            && !(after == '_' || ((after | 0x20) >= 'a' && (after | 0x20) <= 'z')
                 || (after >= '0' && after <= '9')))
            return 1;
        at++;
    }

    return 0;
}

/* A macro that may take an address: and so every macro already defined
 * that names it may too, found by their text -- and every one that names
 * one of those. Rare enough to be worth nothing: macros that take
 * addresses are few. */
static void ps_spread(Macro *m)
{
    const char *name = name_text(m->name);
    size_t len = strlen(name);
    unsigned i;

    ps_risky(m);
    for (i = 0; i != nmacro_slots; i++) {
        Macro *o = macros[i];

        if (o && !o->amp && o->text && macro_names(o->text, name, len))
            ps_spread(o);
    }
}

/* A macro just defined: whether its expansion may take an address, which
 * rules out every body that uses it. */
void prescan_macro(Macro *m)
{
    m->amp = 0;
    if (m->text && macro_text_amp(m->text))
        ps_spread(m);
}

/* ------------------------------------------------------------------ */
/* names                                                               */

#define PS_NAMES 96             /* distinct names a function may use */
#define PS_SLOTS 128
#define PS_ARENA 1024           /* bytes of their text */
#define PS_DEPTH 48             /* braces deep */
#define PS_WANTS 4              /* candidates handed back */

/* What a name was used for. */
enum {
    U_TAKEN = 1,                /* its address may be taken */
    U_CAN   = 4                 /* a parameter IY can hold */
};

/* A name the walk has seen, known by its text: a copy in ps_arena, since
 * the text walked may be a piece read ahead and gone by the end. */
typedef struct {
    const char   *text;
    int           score;        /* uses, weighted by loop depth */
    int           calls;        /* uses with ( after them, weighted too */
    unsigned char len, hash;
    unsigned char use;          /* U_* */
    unsigned char offset;       /* a parameter's, or 0 */
    NameRef       name;         /* a parameter's; 0 until asked */
} PsName;


PsName  ps_names[PS_NAMES];
PsName *ps_end;                 /* past the last in use */
/* Where each name is, by its hash: its entry, or none, and the next slot
 * along when two want the same one. */
PsName *ps_slot[PS_SLOTS];
char    ps_arena[PS_ARENA];
char   *ps_arena_end;
unsigned char ps_bad;           /* something rules the function out */

/* The slot a hash starts at. */
static PsName **ps_slot_of(unsigned char hash)
{
    return ps_slot + (hash & (PS_SLOTS - 1));
}

/* The entry for a name, made the first time. With the table or its text
 * full, a new name has one of its own for the moment, its text where the
 * walk has it: whether it is a risky macro is still asked, at once, but
 * what it was used for is not kept, and a name not kept is never chosen. */
PsName ps_spare;

static PsName *ps_find(const char *p, unsigned char len, unsigned char hash)
{
    PsName **slot = ps_slot_of(hash), *e;

    while ((e = *slot) != NULL) {
        if (e->hash == hash && e->len == len && !memcmp(e->text, p, len))
            return e;
        if (++slot == ps_slot + PS_SLOTS)
            slot = ps_slot;
    }
    if (ps_end == ps_names + PS_NAMES
        || ps_arena_end + len > ps_arena + PS_ARENA) {
        e = &ps_spare;
        e->text = p;
    } else {
        e = ps_end++;
        *slot = e;
        memcpy(ps_arena_end, p, len);
        e->text = ps_arena_end;
        ps_arena_end += len;
    }
    e->score = 0;
    e->calls = 0;
    e->len = len;
    e->hash = hash;
    e->use = 0;
    e->offset = 0;
    e->name = NAME_NONE;

    return e;
}

/* A new function: only the slots the last one used are cleared, by
 * rehashing its names. */
static unsigned char ps_nparams;

void prescan_begin(void)
{
    PsName *e;

    if (!ps_end)
        ps_end = ps_names;
    for (e = ps_names; e != ps_end; e++) {
        PsName **slot = ps_slot_of(e->hash);

        while (*slot != e) {
            if (++slot == ps_slot + PS_SLOTS)
                slot = ps_slot;
        }
        *slot = NULL;
    }
    ps_end = ps_names;
    ps_arena_end = ps_arena;
    ps_bad = 0;
    ps_nparams = 0;
    iy_nwants = 0;
}

/* A parameter, as its list is read: its offset and whether IY can hold it.
 * Only noted: it goes into the table if the body turns out to be walked,
 * which most are not. A parameter past the first few is not a candidate. */
#define PS_PARAMS 8

static NameRef       ps_param_name[PS_PARAMS];
static unsigned char ps_param_at[PS_PARAMS], ps_param_can[PS_PARAMS];

void prescan_param(NameRef name, int offset, int can)
{
    if (ps_nparams == PS_PARAMS || offset > 255)
        return;
    ps_param_name[ps_nparams] = name;
    ps_param_at[ps_nparams] = (unsigned char) offset;
    ps_param_can[ps_nparams++] = (unsigned char) (can != 0);
}

/* The parameters noted, into the table, once the body is to be walked. */
static void ps_params(void)
{
    unsigned char i;

    for (i = 0; i != ps_nparams; i++) {
        const char *t = name_text(ps_param_name[i]);
        unsigned char len, hash = ps_hash_of(t, &len);
        PsName *e = ps_find(t, len, hash);

        if (e == &ps_spare)
            return;
        e->offset = ps_param_at[i];
        e->use = ps_param_can[i] ? U_CAN : 0;
        e->name = ps_param_name[i];
    }
}

/* ------------------------------------------------------------------ */
/* the text                                                            */

/* What each byte is: a name's, a space, or the end of what is in hand. */
#define C_NAME  1
#define C_SPACE 2
#define C_END   4

static unsigned char ps_class[256];

/* Past the window: the rest of the file, a whole number of lines at a
 * time. Each piece ends in a 0, as the window does, and what is after the
 * last newline -- a line not yet whole -- waits one byte past the 0 until
 * the next piece brings it to the front. The buffer is the heap's only for
 * as long as the walk. */
#define PS_PIECE 1024

static char *ps_buf;
static int   ps_part;           /* bytes of that part line */
static int   ps_stage;          /* 0 in the window, 1 in ps_buf */
static long  ps_seek;

/* At the 0 that ends what is in hand: the next piece, or NULL at the end
 * of the text. */
static const unsigned char *ps_next_piece(const unsigned char *p)
{
    int keep, total, cut;

    if (!ps_buf && !(ps_buf = malloc(PS_PIECE + 1)))
        return NULL;
    if (ps_stage == 0) {
        /* The byte the window's sentinel hides, and the rest it has read
         * but not made a line of. */
        ps_stage = 1;
        keep = (int) (src_raw - src_end);
        if (keep > PS_PIECE)
            return NULL;
        if (keep) {
            memcpy(ps_buf, src_end, (size_t) keep);
            ps_buf[0] = src_held;
        }
    } else {
        keep = ps_part;
        memmove(ps_buf, p + 1, (size_t) keep);
    }
    total = keep;
    if (src_file) {
        if (ps_seek < 0)
            ps_seek = ftell(src_file);
        total += (int) fread(ps_buf + keep, 1, (size_t) (PS_PIECE - keep),
                             src_file);
    }
    if (total == 0)
        return NULL;

    /* Whole lines, so that no name is cut in two -- unless the file has
     * ended, or one line fills the piece. */
    cut = total;
    if (total == PS_PIECE)
        while (cut > 0 && ps_buf[cut - 1] != '\n')
            cut--;
    if (cut == 0)
        cut = total;
    ps_part = total - cut;
    memmove(ps_buf + cut + 1, ps_buf + cut, (size_t) ps_part);
    ps_buf[cut] = '\0';

    return (const unsigned char *) ps_buf;
}

/* Past the rest of a comment, a string or a character constant, or a
 * directive's line, from p: where it ends, or NULL if the text does.
 * `until` is the closing quote, '*' for a block comment, or '\n' for the
 * rest of the line; a backslash escapes what follows, a newline too. */
static const unsigned char *ps_skip(const unsigned char *p, int until)
{
    int star = 0;

    for (;;) {
        int c = *p;

        if (c == 0) {
            p = ps_next_piece(p);
            if (!p)
                return NULL;
            continue;
        }
        p++;
        if (until == '*') {                     /* a block comment */
            if (star && c == '/')
                return p;
            star = c == '*';
        } else if (c == '\\') {
            if (*p == 0)
                continue;
            p++;
        } else if (c == until || c == '\n') {
            return p;           /* a line's end ends a quote too, as an error */
        }
    }
}

/* ------------------------------------------------------------------ */
/* the walk                                                            */

/* The walk's state. */
unsigned char ps_loop_brace[PS_DEPTH];  /* whether each brace open is a loop's */
unsigned char ps_braces, ps_loops, ps_parens;
unsigned char ps_header, ps_header_parens;  /* in a loop's ( ), and how deep */
unsigned char ps_stmt;          /* in a loop's statement that has no braces */
unsigned char ps_loop_next;     /* a loop's body comes next */
unsigned char ps_member, ps_amp, ps_level;
int           ps_weight;        /* what a use counts: 8 in a loop */
const unsigned char *ps_wp;     /* where the walk is */
PsName       *ps_risk_e;        /* a name the walk stopped for */
unsigned char ps_tk;            /* a quote ps_walk stopped at */

/* What ps_walk stops for. */
#define W_END   0               /* the 0 at the end of the text in hand */
#define W_RISK  1               /* a name whose hash a risky macro has */
#define W_QUOTE 2               /* a string or a character: ps_tk is which */
#define W_SLASH 3               /* a comment: ps_wp at its * or second / */
#define W_HASH  4               /* a directive */
#define W_WIDE  5               /* L' or L": ps_wp at the quote */
#define W_DONE  6               /* the body's closing } */
#define W_BAD   7               /* braces deeper than it keeps */

/* The walk from ps_wp, token by token, until something it leaves to the C
 * below. A name is a use of its entry, weighed by the loops round it; one
 * after an `&` has its address taken, and one with a `(` after it is a
 * call, if it turns out to be something called. for, while and do are
 * known by their spelling. */
static int ps_walk(void)
{
    const unsigned char *p = ps_wp;

    for (;;) {
        unsigned char c, hash = 0, len = 0;
        const unsigned char *s, *q;
        PsName *e, *before;

        while (ps_class[*p] & C_SPACE)
            p++;
        c = *p;
        if (ps_class[c] & C_END) {
            ps_wp = p;
            return W_END;
        }

        /* A loop's body that is not braced is the statement that starts
         * here, up to its `;`. */
        if (ps_loop_next && c != '{') {
            ps_loop_next = 0;
            ps_stmt = 1;
        }
        if (ps_level != (unsigned char) (ps_loops + (ps_header || ps_stmt))) {
            ps_level = (unsigned char) (ps_loops + (ps_header || ps_stmt));
            ps_weight = ps_level == 0 ? 1 : ps_level == 1 ? 8
                        : ps_level == 2 ? 64 : 512;
        }

        if (ps_class[c] & C_NAME) {
            if (c >= '0' && c <= '9') {         /* a number */
                do
                    p++;
                while (ps_class[*p] & C_NAME || *p == '.');
                ps_amp = 0;
                continue;
            }
            s = p;
            while (ps_class[*p] & C_NAME) {
                hash = (unsigned char) ((hash << 1) + *p);
                len++;
                p++;
            }
            if (len == 1 && c == 'L' && (*p == '\'' || *p == '"')) {
                ps_wp = p;
                return W_WIDE;
            }
            if (ps_member) {                    /* s.n and s->n */
                ps_member = 0;
                continue;
            }
            if ((len == 3 && s[0] == 'f' && s[1] == 'o' && s[2] == 'r')
                || (len == 5 && s[0] == 'w' && s[1] == 'h' && s[2] == 'i'
                    && s[3] == 'l' && s[4] == 'e')) {
                ps_header = 1;
                ps_header_parens = ps_parens;
                continue;
            }
            if (len == 2 && s[0] == 'd' && s[1] == 'o') {
                ps_loop_next = 1;
                continue;
            }
            before = ps_end;
            e = ps_find((const char *) s, len, hash);
            e->score += ps_weight;
            if (ps_amp)
                e->use |= U_TAKEN;
            ps_amp = 0;
            for (q = p; ps_class[*q] & C_SPACE; q++)
                ;
            if (*q == '(')
                e->calls += ps_weight;
            /* A name new to the table, with a hash a risky macro has: the C
             * asks whether it is that macro. Only new ones -- what a name
             * is does not change inside a body. */
            if ((e == &ps_spare || ps_end != before)
                && ps_risk[hash >> 3] & (1 << (hash & 7))) {
                ps_wp = p;
                ps_risk_e = e;
                return W_RISK;
            }
            continue;
        }

        p++;
        switch (c) {
        case '&':
            if (*p == '&' || *p == '=') {
                p++;
                break;
            }
            ps_amp = 1;
            continue;
        case '-':
            if (*p != '>')
                break;
            p++;
            /* fall through: -> is . */
        case '.':
            ps_member = 1;
            break;
        case '"':
        case '\'':
            ps_tk = c;
            ps_wp = p;
            return W_QUOTE;
        case '/':
            if (*p != '*' && *p != '/')
                break;
            ps_wp = p;
            return W_SLASH;
        case '#':
            ps_wp = p;
            return W_HASH;
        case '(':
            ps_parens++;
            break;
        case ')':
            ps_parens--;
            if (ps_header && ps_parens == ps_header_parens) {
                ps_header = 0;
                ps_loop_next = 1;
            }
            break;
        case ';':
            if (!ps_header && ps_parens == 0)
                ps_stmt = 0;
            break;
        case '{':
            if (ps_braces == PS_DEPTH) {
                ps_wp = p;
                return W_BAD;
            }
            c = (unsigned char) (ps_loop_next || ps_stmt);
            ps_loop_brace[ps_braces++] = c;
            ps_loops += c;
            ps_loop_next = 0;
            ps_stmt = 0;
            break;
        case '}':
            if (--ps_braces == 0) {
                ps_wp = p;
                return W_DONE;
            }
            ps_loops -= ps_loop_brace[ps_braces];
            break;
        default:
            break;
        }
        ps_amp = 0;
    }
}

/* Whether a body has a loop in it, looked for as quickly as it can be: the
 * text from its `{` to the `}` that matches it, as braces counted without
 * regard to strings or comments, has one of the names for, while and do.
 * A local only earns IY by being used in a loop, so a body without one is
 * not walked at all -- and most bodies have none. Wrong either way only
 * costs time: a brace or a keyword in a string or a comment walks a body
 * for nothing, or leaves one unwalked. Text that ends before the braces do
 * says yes, so that the walk, which reads past it, decides. */
static int ps_has_loop(const unsigned char *p)
{
    int braces = 0;

    for (;;) {
        unsigned char c = *p;

        if (ps_class[c] & C_NAME) {
            const unsigned char *s = p;

            while (ps_class[*p] & C_NAME)
                p++;
            if ((p - s == 3 && s[0] == 'f' && s[1] == 'o' && s[2] == 'r')
                || (p - s == 5 && s[0] == 'w' && s[1] == 'h' && s[2] == 'i'
                    && s[3] == 'l' && s[4] == 'e')
                || (p - s == 2 && s[0] == 'd' && s[1] == 'o'))
                return 1;
            continue;
        }
        if (c == 0)
            return 1;
        if (c == '{')
            braces++;
        else if (c == '}' && --braces == 0)
            return 0;
        p++;
    }
}

/* A name's NameRef, asked for the first time it is needed. The spare's
 * is asked each time: it is a different name each time. */
static NameRef ps_name_of(PsName *e)
{
    if (!e->name || e == &ps_spare)
        e->name = name_intern(e->text, e->len);

    return e->name;
}

/* Whether a called name is something a call is made to, so that a local
 * in IY would be saved around it: a function not compiled in place, a
 * pointer to one in a local, or a name not declared yet. Not a keyword --
 * `if (`, `sizeof (` -- or a macro. */
static int ps_is_call(PsName *e)
{
    NameRef n = ps_name_of(e);
    int s;

    if (n < kw_limit || (name_is_macro(n) & NAME_MACRO))
        return 0;
    s = sym_find(n);

    return s == SYM_NONE || sym_at(s)->kind != SYM_FUNC || !inline_has(s);
}

/* Whether a name may be a local of this body -- not a keyword, a macro, or
 * anything declared before the body began -- or is one of its parameters. */
static int ps_may_be_local(PsName *e)
{
    NameRef n = ps_name_of(e);
    int s;

    if (n < kw_limit || (name_is_macro(n) & NAME_MACRO))
        return 0;
    s = sym_find(n);

    return s == SYM_NONE || sym_at(s)->kind == SYM_LOCAL;
}

/* The body, from its `{`, which is where `at` points in the window: up to
 * PS_WANTS names, best first, of locals the function could hold in IY, and
 * how many -- or none. If the best is a parameter, *param is its offset.
 *
 * Which of them is a local, and of what type, the compiler knows as it
 * declares each one, so that is left to it (gen_iy_claim). */
int prescan_body(const char *at, NameRef *wants, int *param)
{
    PsName *e;
    int n = 0, calls = 0;

    *param = 0;
    if (!at || at < src || at >= src_end || *at != '{')
        return 0;
    if (!ps_class['_']) {
        int c;

        for (c = 0; c != 256; c++)
            ps_class[c] = (unsigned char) (c == 0 ? C_END
                                           : c == ' ' || c == '\t' || c == '\n'
                                             || c == '\r' || c == '\f'
                                             || c == '\v' ? C_SPACE
                                           : c == '_' || c >= 0x80
                                             || ((c | 0x20) >= 'a'
                                                 && (c | 0x20) <= 'z')
                                             || (c >= '0' && c <= '9') ? C_NAME
                                           : 0);
    }
    if (!ps_has_loop((const unsigned char *) at))
        return 0;
    ps_params();
    ps_stage = 0;
    ps_seek = -1;
    ps_part = 0;
    ps_braces = ps_loops = ps_parens = ps_header = ps_header_parens = 0;
    ps_stmt = ps_loop_next = ps_member = ps_amp = ps_level = 0;
    ps_weight = 1;
    ps_wp = (const unsigned char *) at;

    while (!ps_bad) {
        const unsigned char *p;
        Macro *m;

        switch (ps_walk()) {
        case W_END:
            p = ps_next_piece(ps_wp);
            if (!p)
                goto out;
            ps_wp = p;
            break;
        case W_RISK:
            /* A name a risky macro's hash has: whether it is that macro. */
            m = macro_find(ps_name_of(ps_risk_e));
            if (m && m->amp)
                ps_bad = 1;
            break;
        case W_WIDE:
            p = ps_skip(ps_wp + 1, *ps_wp);     /* L'x' and L"x" */
            if (!p)
                goto out;
            ps_wp = p;
            ps_amp = 0;
            break;
        case W_QUOTE:
            p = ps_skip(ps_wp, ps_tk);
            if (!p)
                goto out;
            ps_wp = p;
            ps_amp = 0;
            break;
        case W_SLASH:
            p = ps_skip(ps_wp + 1, *ps_wp == '*' ? '*' : '\n');
            if (!p)
                goto out;
            ps_wp = p;
            break;
        case W_HASH:
            /* A directive: #if and its kind only choose what is compiled,
             * and counting both sides costs nothing. One that defines or
             * brings in text could hide an `&`. */
            p = ps_wp;
            while (*p == ' ' || *p == '\t')
                p++;
            if (*p == 'd' || *p == 'u' || (p[0] == 'i' && p[1] == 'n'))
                ps_bad = 1;
            p = ps_skip(p, '\n');
            if (!p)
                goto out;
            ps_wp = p;
            break;
        case W_DONE:
            goto done;
        default:
            ps_bad = 1;
            break;
        }
    }
out:
    ps_bad = 1;                         /* the text ran out before the `}` */

done:
    if (ps_seek >= 0)
        fseek(src_file, ps_seek, SEEK_SET);
    free(ps_buf);
    ps_buf = NULL;
    if (ps_bad)
        return 0;

    /* The calls a local in IY would be saved around. */
    for (e = ps_names; e != ps_end; e++)
        if (e->calls && ps_is_call(e))
            calls += e->calls;

    /* The best few, used inside a loop and more than those calls cost -- a
     * push and a pop each, about what two of its uses save. A parameter is
     * only one if IY can hold it; a local is asked that as it is declared. */
    for (;;) {
        PsName *best = NULL;

        for (e = ps_names; e != ps_end; e++)
            if (!(e->use & U_TAKEN) && (!e->offset || e->use & U_CAN)
                && (!best || e->score > best->score))
                best = e;
        if (!best || best->score < 8 || best->score <= 2 * calls)
            break;
        best->use |= U_TAKEN;           /* not chosen twice */
        if (!best->offset && !ps_may_be_local(best))
            continue;
        if (n == 0 && best->offset)
            *param = best->offset;
        wants[n++] = ps_name_of(best);
        if (n == PS_WANTS || *param)
            break;
    }

    return n;
}

/* A local as it is declared: whether it is the one prescan_body chose -- the
 * best of its choices still in the running. One declared with a type IY
 * cannot hold drops out and the next is the best; while a better one has
 * not been declared yet, this one waits. A local declared register has
 * already had its chance (gen_iy_claim). */
void prescan_claim(int offset, Type type, NameRef name)
{
    int i;

    if (iy_local != 0)
        return;
    for (i = 0; i != iy_nwants; i++) {
        if (iy_wants[i] == NAME_NONE)
            continue;                   /* out of the running */
        if (iy_wants[i] != name)
            return;                     /* a better one is still to come */
        if (gen_iy_can(type)) {
            iy_local = offset;
            return;
        }
        iy_wants[i] = NAME_NONE;
    }
}

#endif
