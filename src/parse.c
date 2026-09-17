/*
 * The parser, which is also the code generator's caller.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 *
 * There is no syntax tree. Each production emits as it is recognised, which
 * is what makes the compiler one pass and what keeps it small enough to run
 * on the machine it compiles for.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "acc.h"

const char *lex_path(void);

/* ------------------------------------------------------------------ */
/* diagnostics                                                         */

void acc_error_at(int line, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "%s:%d: error: ", lex_path() ? lex_path() : "acc", line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

void acc_error(const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "acc: error: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

/* ------------------------------------------------------------------ */
/* expressions                                                         */

static void expr(void);

/* A name used as a value: a local read, or a call. */
static void primary(void)
{
    if (tok == TK_INT) {
        vpush_const(tok_val);
        next();

        return;
    }

    if (tok == TK_LPAREN) {
        next();
        expr();
        expect(TK_RPAREN, "')'");

        return;
    }

    if (tok == TK_MINUS) {
        next();
        primary();
        vneg();

        return;
    }

    if (tok == TK_TILDE) {
        next();
        primary();
        vnot();

        return;
    }

    if (tok == TK_PLUS) {       /* unary plus is the value unchanged */
        next();
        primary();

        return;
    }

    if (tok == TK_IDENT) {
        NameRef name = tok_name;
        Sym *s;

        next();

        if (tok == TK_LPAREN) {
            int nargs = 0;

            next();
            s = sym_find(name);
            if (!s) {
                /* Not seen yet. Assumed to be a function defined further
                 * down; gen_finish reports it if it never is. */
                s = sym_push(name, SYM_FUNC, 0);
            } else if (s->kind != SYM_FUNC) {
                acc_error_at(tok_line, "'%s' is not a function", name_text(name));
            }

            if (tok != TK_RPAREN) {
                for (;;) {
                    expr();
                    nargs++;
                    if (!accept(TK_COMMA))
                        break;
                }
            }
            expect(TK_RPAREN, "')'");
            gen_call(s, nargs);

            return;
        }

        s = sym_find(name);
        if (!s)
            acc_error_at(tok_line, "'%s' is not declared", name_text(name));
        if (s->kind != SYM_LOCAL)
            acc_error_at(tok_line, "'%s' is not a variable", name_text(name));
        vpush_local(s->val);

        return;
    }

    acc_error_at(tok_line, "expected an expression, found %s", tok_spelling(tok));
}

static void additive(void)
{
    primary();
    while (tok == TK_PLUS || tok == TK_MINUS) {
        int op = tok;

        next();
        primary();
        vbinop(op);
    }
}

/* Assignment is right associative and its left side has to be a name, which
 * is the whole of what a milestone with no pointers can assign to. */
static void expr(void)
{
    if (tok == TK_IDENT) {
        NameRef name = tok_name;
        Sym *s;

        /* Look one token ahead by remembering this one: an identifier
         * followed by '=' is an assignment, anything else is a value. */
        next();
        if (tok == TK_ASSIGN) {
            next();
            expr();
            s = sym_find(name);
            if (!s || s->kind != SYM_LOCAL)
                acc_error_at(tok_line, "'%s' cannot be assigned to", name_text(name));
            vstore_local(s->val);

            return;
        }

        /* Not an assignment. Put the name back by handling it here rather
         * than by pushing the token back, which would need a queue. */
        if (tok == TK_LPAREN) {
            int nargs = 0;
            Sym *fn = sym_find(name);

            next();
            if (!fn)
                fn = sym_push(name, SYM_FUNC, 0);
            else if (fn->kind != SYM_FUNC)
                acc_error_at(tok_line, "'%s' is not a function", name_text(name));
            if (tok != TK_RPAREN) {
                for (;;) {
                    expr();
                    nargs++;
                    if (!accept(TK_COMMA))
                        break;
                }
            }
            expect(TK_RPAREN, "')'");
            gen_call(fn, nargs);
        } else {
            s = sym_find(name);
            if (!s)
                acc_error_at(tok_line, "'%s' is not declared", name_text(name));
            if (s->kind != SYM_LOCAL)
                acc_error_at(tok_line, "'%s' is not a variable", name_text(name));
            vpush_local(s->val);
        }

        while (tok == TK_PLUS || tok == TK_MINUS) {
            int op = tok;

            next();
            primary();
            vbinop(op);
        }

        return;
    }

    additive();
}

/* ------------------------------------------------------------------ */
/* statements and declarations                                         */

static void declaration(void)
{
    expect(TK_KW_INT, "'int'");
    for (;;) {
        NameRef name;
        int off;

        if (tok != TK_IDENT)
            acc_error_at(tok_line, "expected a name, found %s", tok_spelling(tok));
        name = tok_name;
        next();

        off = gen_local();
        if (accept(TK_ASSIGN)) {
            expr();
            vstore_local(off);
            vdrop();            /* a declaration is not an expression */
        }
        /* Pushed after the initialiser, so `int x = x;` does not see itself. */
        sym_push(name, SYM_LOCAL, off);

        if (!accept(TK_COMMA))
            break;
    }
    expect(TK_SEMI, "';'");
}

static void statement(void)
{
    if (accept(TK_KW_RETURN)) {
        if (tok != TK_SEMI)
            expr();
        expect(TK_SEMI, "';'");
        gen_return();

        return;
    }

    if (accept(TK_SEMI))
        return;

    expr();
    vdrop();                    /* the value of a statement is discarded */
    expect(TK_SEMI, "';'");
}

static void function(void)
{
    NameRef name;
    Sym *fn;
    int mark, nparams = 0, argoff;

    expect(TK_KW_INT, "'int'");
    if (tok != TK_IDENT)
        acc_error_at(tok_line, "expected a function name, found %s", tok_spelling(tok));
    name = tok_name;
    next();
    expect(TK_LPAREN, "'('");

    fn = sym_find(name);
    if (fn && fn->kind == SYM_FUNC && fn->val)
        acc_error_at(tok_line, "'%s' is defined twice", name_text(name));
    if (!fn)
        fn = sym_push(name, SYM_FUNC, 0);

    mark = sym_mark();

    /* The first argument sits above the saved ix and the return address. */
    argoff = 2 * ACC_PTR_SIZE;
    if (tok == TK_KW_VOID) {
        next();
    } else if (tok != TK_RPAREN) {
        for (;;) {
            expect(TK_KW_INT, "'int'");
            if (tok != TK_IDENT)
                acc_error_at(tok_line, "expected a parameter name, found %s",
                             tok_spelling(tok));
            sym_push(tok_name, SYM_LOCAL, argoff);
            argoff += ACC_INT_SIZE;
            nparams++;
            next();
            if (!accept(TK_COMMA))
                break;
        }
    }
    expect(TK_RPAREN, "')'");
    expect(TK_LBRACE, "'{'");

    gen_func_begin(fn, nparams);
    while (tok == TK_KW_INT)
        declaration();
    while (tok != TK_RBRACE && tok != TK_EOF)
        statement();
    expect(TK_RBRACE, "'}'");
    gen_func_end();

    sym_release(mark);
}

static void translation_unit(void)
{
    while (tok != TK_EOF)
        function();
}

/* ------------------------------------------------------------------ */

static void usage(void)
{
    fprintf(stderr, "usage: acc <source.c> -o <out.bin>\n");
    exit(2);
}

int main(int argc, char **argv)
{
    const char *in = NULL, *out = NULL;
    int i;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'o') {
            if (argv[i][2])
                out = argv[i] + 2;
            else if (++i < argc)
                out = argv[i];
            else
                usage();
        } else if (argv[i][0] == '-') {
            usage();
        } else if (!in) {
            in = argv[i];
        } else {
            usage();
        }
    }
    if (!in || !out)
        usage();

    name_init();
    lex_init();
    sym_init();
    gen_init();
    out_open(out);
    gen_startup();
    lex_open(in);
    translation_unit();
    gen_finish();
    lex_close();
    out_close();

    return 0;
}
