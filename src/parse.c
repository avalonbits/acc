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
#include "timing.h"

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
        int s;

        next();

        if (tok == TK_LPAREN) {
            int nargs = 0;

            next();
            s = sym_find(name);
            if (s == SYM_NONE) {
                /* Not seen yet. Assumed to be a function defined further
                 * down; gen_finish reports it if it never is. */
                s = sym_push(name, SYM_FUNC, 0);
            } else if (sym_at(s)->kind != SYM_FUNC) {
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
        if (s == SYM_NONE)
            acc_error_at(tok_line, "'%s' is not declared", name_text(name));
        if (sym_at(s)->kind != SYM_LOCAL)
            acc_error_at(tok_line, "'%s' is not a variable", name_text(name));
        vpush_local(sym_at(s)->val);

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
        int s;

        /* Look one token ahead by remembering this one: an identifier
         * followed by '=' is an assignment, anything else is a value. */
        next();
        if (tok == TK_ASSIGN) {
            next();
            expr();
            s = sym_find(name);
            if (s == SYM_NONE || sym_at(s)->kind != SYM_LOCAL)
                acc_error_at(tok_line, "'%s' cannot be assigned to", name_text(name));
            vstore_local(sym_at(s)->val);

            return;
        }

        /* Not an assignment. Put the name back by handling it here rather
         * than by pushing the token back, which would need a queue. */
        if (tok == TK_LPAREN) {
            int nargs = 0;
            int fn = sym_find(name);

            next();
            if (fn == SYM_NONE)
                fn = sym_push(name, SYM_FUNC, 0);
            else if (sym_at(fn)->kind != SYM_FUNC)
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
            if (s == SYM_NONE)
                acc_error_at(tok_line, "'%s' is not declared", name_text(name));
            if (sym_at(s)->kind != SYM_LOCAL)
                acc_error_at(tok_line, "'%s' is not a variable", name_text(name));
            vpush_local(sym_at(s)->val);
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

static void statement(void);

/* `{ ... }` where a statement is expected. Declarations are not allowed in
 * one: sym_drop_locals drops every local a function has, so there is nothing
 * yet that could give an inner block a scope of its own, and a name declared
 * in one would outlive it. Saying so is better than letting it through and
 * being wrong about where the name ends. */
static void block(void)
{
    while (tok != TK_RBRACE && tok != TK_EOF) {
        if (tok == TK_KW_INT)
            acc_error_at(tok_line,
                         "a declaration has to be at the start of the function");
        statement();
    }
    expect(TK_RBRACE, "'}'");
}

static void condition(void)
{
    expect(TK_LPAREN, "'('");
    expr();
    expect(TK_RPAREN, "')'");
}

/* Dispatched on the token rather than tested against one keyword at a time.
 * Every statement in the program walks this, and a chain grows a comparison
 * for each form the language gains. */
static void statement(void)
{
    switch (tok) {
    case TK_KW_IF: {
        int to_else;

        next();
        condition();
        to_else = gen_jump_if_false();
        statement();

        /* `else if` needs nothing of its own: the else branch is a statement,
         * and an if is a statement. A dangling else binds to the nearest if
         * for the same reason -- the inner if consumes it first. */
        if (accept(TK_KW_ELSE)) {
            int to_end = gen_jump();

            gen_label(to_else);
            statement();
            gen_label(to_end);
        } else {
            gen_label(to_else);
        }

        return;
    }

    case TK_KW_WHILE: {
        int top, to_end;

        next();
        top = gen_here();
        condition();
        to_end = gen_jump_if_false();
        statement();
        gen_jump_to(top);
        gen_label(to_end);

        return;
    }

    case TK_LBRACE:
        next();
        block();

        return;

    case TK_KW_RETURN:
        next();
        if (tok != TK_SEMI)
            expr();
        expect(TK_SEMI, "';'");
        gen_return();

        return;

    case TK_SEMI:
        next();

        return;

    case TK_KW_ELSE:
        acc_error_at(tok_line, "'else' without an 'if'");

        return;

    default:
        expr();
        vdrop();                /* the value of a statement is discarded */
        expect(TK_SEMI, "';'");

        return;
    }
}

static void function(void)
{
    NameRef name;
    int fn;
    int nparams = 0, argoff;

    expect(TK_KW_INT, "'int'");
    if (tok != TK_IDENT)
        acc_error_at(tok_line, "expected a function name, found %s", tok_spelling(tok));
    name = tok_name;
    next();
    expect(TK_LPAREN, "'('");

    fn = sym_find(name);
    if (fn != SYM_NONE && sym_at(fn)->kind == SYM_FUNC && sym_at(fn)->val)
        acc_error_at(tok_line, "'%s' is defined twice", name_text(name));
    if (fn == SYM_NONE)
        fn = sym_push(name, SYM_FUNC, 0);

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
    block();
    gen_func_end();

    sym_drop_locals();
}

static void translation_unit(void)
{
    while (tok != TK_EOF)
        function();
}

/* ------------------------------------------------------------------ */

static void usage(void)
{
    fprintf(stderr,
        "usage: acc <source.c> -o <out.bin> [-x]\n"
        "\n"
        "  The program prints what main returned, as six hex digits.\n"
        "  -x  report it to IO port 0 instead, which stops an emulator\n"
        "      with the low byte as its exit status.\n");
    exit(2);
}

int main(int argc, char **argv)
{
    const char *in = NULL, *out = NULL;
    int by_exit = 0;
    int i;
    clock_t begin;
    unsigned cs;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'o') {
            if (argv[i][2])
                out = argv[i] + 2;
            else if (++i < argc)
                out = argv[i];
            else
                usage();
        } else if (argv[i][0] == '-' && argv[i][1] == 'x' && !argv[i][2]) {
            by_exit = 1;
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

    begin = clock();

    name_init();
    lex_init();
    sym_init();
    gen_init();
    out_open(out);
    gen_startup(by_exit);
    lex_open(in);
    translation_unit();
    gen_finish();
    lex_close();
    out_close();

    /* Reported the way zap reports it, down to the wording, so that the two
     * halves of a build can be read as one number. Measured from after the
     * arguments are checked to after the file is written: everything a
     * "how long did that take" is asking about, and nothing else. */
    cs = elapsed_cs(begin, clock());
    printf("Done in %u.%02u seconds\r\n", cs / 100, cs % 100);

    return 0;
}
