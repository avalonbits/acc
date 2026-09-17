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

/* The width the value being parsed is going straight into, when that is
 * narrower than an int -- the destination of an assignment or an initialiser.
 * Zero the rest of the time.
 *
 * It is what makes byte arithmetic legal: computing in eight bits is
 * indistinguishable from C's promote-then-truncate exactly when the result is
 * truncated to that width and nothing wider ever sees it. */
static Type narrow_dest;


/* A name used as a value: a local read, or a call. */
static void primary(void)
{
    if (tok == TK_FLOAT) {
        vpush_const_float(tok_fval);
        next();

        return;
    }

    if (tok == TK_INT) {
        if (type_wide(tok_type))
            vpush_const_long(tok_val, tok_type);
        else
            vpush_const((int) tok_val, tok_type);
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
        int sym;

        next();

        if (tok == TK_LPAREN) {
            int nargs = 0;

            next();
            sym = sym_find(name);
            if (sym == SYM_NONE) {
                /* Not seen yet. Assumed to be a function defined further
                 * down; gen_finish reports it if it never is. */
                sym = sym_push(name, SYM_FUNC, 0);
            } else if (sym_at(sym)->kind != SYM_FUNC) {
                acc_error_at(tok_line, "'%s' is not a function", name_text(name));
            }

            if (tok != TK_RPAREN) {
                Type outer = narrow_dest;

                /* An argument is not the destination: it is passed at int
                 * width whatever the parameter is declared as. */
                narrow_dest = 0;
                for (;;) {
                    expr();
                    nargs++;
                    if (!accept(TK_COMMA))
                        break;
                }
                narrow_dest = outer;
            }
            expect(TK_RPAREN, "')'");
            gen_call(sym, nargs, sym_params_first(sym), sym_nparams(sym));

            return;
        }

        sym = sym_find(name);
        if (sym == SYM_NONE)
            acc_error_at(tok_line, "'%s' is not declared", name_text(name));
        if (sym_at(sym)->kind != SYM_LOCAL)
            acc_error_at(tok_line, "'%s' is not a variable", name_text(name));
        vpush_local(sym_at(sym)->val, sym_at(sym)->type);

        return;
    }

    /* A `*` or `&` where an expression was meant to start is a dereference or
     * an address-of, not the binary operator of the same spelling. Say which
     * feature is missing rather than which character was unexpected. */
    if (tok == TK_STAR || tok == TK_AMP)
        acc_error_at(tok_line, "pointers are not supported yet");

    /* An operator acc has not got to yet, where an expression was meant to
     * start. Name the operator, as expect() does when one turns up where a
     * statement was meant to end; "expected an expression" is true and sends
     * the reader looking for the wrong thing. */
    if (tok_is_unimplemented_op(tok))
        acc_error_at(tok_line, "%s is not supported yet", tok_spelling(tok));

    acc_error_at(tok_line, "expected an expression, found %s", tok_spelling(tok));
}

/* Binary operators, and how tightly each binds. Zero means it is not one.
 *
 * One loop over a table rather than a function per level. The chain this
 * replaces -- expr calling equality calling relational calling additive
 * calling primary -- meant three calls and three loop tests on every
 * expression in the program whether or not it held a comparison, and cost
 * 1.2% on a benchmark containing none. A level costs nothing here until an
 * operator at that level actually turns up.
 */
/* C's precedence, loosest first. The numbers are relative and only their
 * order matters; they are C's levels rather than acc's, so that adding an
 * operator is a row in the table and not a decision. */
enum {
    PREC_NONE       = 0,        /* not a binary operator */
    PREC_BIT_OR     = 1,        /* | */
    PREC_BIT_XOR    = 2,        /* ^ */
    PREC_BIT_AND    = 3,        /* & */
    PREC_EQUALITY   = 4,        /* ==  != */
    PREC_RELATIONAL = 5,        /* <  >  <=  >= */
    PREC_SHIFT      = 6,        /* <<  >> */
    PREC_ADDITIVE   = 7,        /* +  - */
    PREC_MULTIPLY   = 8         /* *  /  % */
};

static const unsigned char prec[TK_COUNT] = {
    [TK_PIPE]  = PREC_BIT_OR,
    [TK_CARET] = PREC_BIT_XOR,
    [TK_AMP]   = PREC_BIT_AND,
    [TK_EQ] = PREC_EQUALITY,   [TK_NE] = PREC_EQUALITY,
    [TK_LT] = PREC_RELATIONAL, [TK_GT] = PREC_RELATIONAL,
    [TK_LE] = PREC_RELATIONAL, [TK_GE] = PREC_RELATIONAL,
    [TK_SHL] = PREC_SHIFT,     [TK_SHR] = PREC_SHIFT,
    [TK_PLUS] = PREC_ADDITIVE, [TK_MINUS] = PREC_ADDITIVE,
    [TK_STAR] = PREC_MULTIPLY, [TK_SLASH] = PREC_MULTIPLY,
    [TK_PERCENT] = PREC_MULTIPLY
};

/* The operators that leave a 0 or 1 behind rather than a number. Named one at
 * a time rather than as a range of precedences: the range was right only by
 * accident of where the comparisons sat, and it stopped being right the
 * moment the bitwise operators went in below them. */
static int is_comparison(int token)
{
    switch (token) {
    case TK_EQ: case TK_NE:
    case TK_LT: case TK_GT: case TK_LE: case TK_GE:
        return 1;
    }

    return 0;
}

/* Operators for which truncating as you go gives what truncating at the end
 * would. Division and the comparisons are not among them, and neither is a
 * call, which is why the fast path also insists the expression ends here. */
static int transparent_op(int token)
{
    switch (token) {
    case TK_PLUS: case TK_MINUS:
    case TK_AMP: case TK_PIPE: case TK_CARET:
    case TK_SHL: case TK_SHR:
        return 1;
    }

    return 0;
}

/* Nothing follows that could want the wider value. A single token is enough
 * to know: the operator being applied is the last one in the expression, so
 * its result goes to the destination and nowhere else.
 *
 * `c = a + b > 3` is why this is checked rather than assumed -- the `+` is
 * applied before the `>` is parsed, and narrowing it would truncate a value
 * the comparison is entitled to see in full. In a longer chain only the last
 * operation takes this path, which is correct and still the whole win for
 * the one-operator statements that byte code is mostly made of. */
static int expression_ends_here(void)
{
    return tok == TK_SEMI || tok == TK_COMMA || tok == TK_RPAREN;
}

/* The operator loop, with the left operand already on the stack. `min_prec`
 * is the loosest binding this call will take: an operator looser than that
 * belongs to the caller. */
static void binary_rest(int min_prec)
{
    for (;;) {
        int op = tok;
        int op_prec = prec[op];

        if (op_prec < min_prec)         /* PREC_NONE included: not an operator */
            return;
        next();
        primary();
        binary_rest(op_prec + 1);       /* everything binding tighter first */

        if (vlong_pair()) {
            /* C converts both sides to long when either is one, and the
             * result is a long -- or, for a comparison, an int taken from a
             * long-wide comparison. */
            /* C's conversions: floating wins over integer, and among the
             * integers unsigned wins. */
            Type wide;

            if (type_float(vtype_at(1)) || type_float(vtype_at(0)))
                wide = TY_FLOAT;
            else if (type_unsigned(vtype_at(1)) || type_unsigned(vtype_at(0)))
                wide = TY_ULONG;
            else
                wide = TY_LONG;

            if (is_comparison(op))
                vcmp_long(op, wide);
            else
                vbinop_long(op, wide);
        } else if (is_comparison(op))
            vcmp(op);
        else if (narrow_dest && transparent_op(op) && expression_ends_here()
                 && vnarrow_ready(op, narrow_dest))
            vbinop_narrow(op, narrow_dest);
        else
            vbinop(op);
    }
}

static void binary(int min_prec)
{
    primary();
    binary_rest(min_prec);
}

/* Assignment is right associative and its left side has to be a name, which
 * is the whole of what a milestone with no pointers can assign to. */
static void expr(void)
{
    if (tok == TK_IDENT) {
        NameRef name = tok_name;
        int sym;

        /* Look one token ahead by remembering this one: an identifier
         * followed by '=' is an assignment, anything else is a value. */
        next();
        if (tok == TK_ASSIGN) {
            int dest = sym_find(name);
            Type outer = narrow_dest;

            next();
            /* The destination's width, for the expression about to be parsed.
             * Looked up before rather than after so the operator loop can use
             * it; the check that it is assignable stays below, where the
             * diagnostic belongs. */
            narrow_dest = 0;
            if (dest != SYM_NONE && sym_at(dest)->kind == SYM_LOCAL
                && type_size(sym_at(dest)->type) < ACC_INT_SIZE)
                narrow_dest = sym_at(dest)->type;
            expr();
            narrow_dest = outer;
            sym = sym_find(name);
            if (sym == SYM_NONE || sym_at(sym)->kind != SYM_LOCAL)
                acc_error_at(tok_line, "'%s' cannot be assigned to", name_text(name));
            vstore_local(sym_at(sym)->val, sym_at(sym)->type);

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
                Type outer = narrow_dest;

                /* An argument is not the destination: it is passed at int
                 * width whatever the parameter is declared as. */
                narrow_dest = 0;
                for (;;) {
                    expr();
                    nargs++;
                    if (!accept(TK_COMMA))
                        break;
                }
                narrow_dest = outer;
            }
            expect(TK_RPAREN, "')'");
            gen_call(fn, nargs, sym_params_first(fn), sym_nparams(fn));
        } else {
            sym = sym_find(name);
            if (sym == SYM_NONE)
                acc_error_at(tok_line, "'%s' is not declared", name_text(name));
            if (sym_at(sym)->kind != SYM_LOCAL)
                acc_error_at(tok_line, "'%s' is not a variable", name_text(name));
            vpush_local(sym_at(sym)->val, sym_at(sym)->type);
        }

        binary_rest(PREC_BIT_OR);

        return;
    }

    binary(PREC_BIT_OR);
}

/* ------------------------------------------------------------------ */
/* types                                                               */

/* A declaration begins with one or more specifier keywords in any order:
 * `unsigned short int` and `int short unsigned` are the same type. They are
 * counted rather than matched against a list of spellings, which is what
 * keeps the combinations from turning into a table. */
/* Which keywords can begin a type, and what each means on its own.
 *
 * A table rather than a switch because block() asks "does a type start here"
 * before every statement in the program, and type_specifier runs for every
 * declaration, every parameter and every function. Written as a switch, those
 * were seven comparisons each and cost 4.7% of a compile.
 *
 * The entry is the type biased by one, so that void -- which is zero -- is
 * still distinguishable from "not a specifier".
 */
static const unsigned char spec_alone[TK_COUNT] = {
    [TK_KW_VOID]     = TY_VOID + 1,
    [TK_KW_CHAR]     = TY_CHAR + 1,
    [TK_KW_SHORT]    = TY_SHORT + 1,
    [TK_KW_INT]      = TY_INT + 1,
    [TK_KW_SIGNED]   = TY_INT + 1,
    [TK_KW_UNSIGNED] = TY_UINT + 1,
    [TK_KW_LONG]     = TY_LONG + 1,
    [TK_KW_FLOAT]    = TY_FLOAT + 1,
    [TK_KW_DOUBLE]   = TY_FLOAT + 1
};

static int starts_type(int token)
{
    return spec_alone[token] != 0;
}

/* The several-keyword case: `unsigned short int` and `int short unsigned` are
 * the same type, so they are counted rather than matched against a list. */
static Type type_specifier_slow(int first, int line)
{
    int is_void = 0, is_char = 0, is_short = 0, is_int = 0;
    int is_long = 0, is_signed = 0, is_unsigned = 0, is_float = 0;
    int token = first;

    for (;;) {
        switch (token) {
        case TK_KW_VOID:     is_void++;     break;
        case TK_KW_CHAR:     is_char++;     break;
        case TK_KW_SHORT:    is_short++;    break;
        case TK_KW_INT:      is_int++;      break;
        case TK_KW_LONG:     is_long++;     break;
        case TK_KW_SIGNED:   is_signed++;   break;
        case TK_KW_UNSIGNED: is_unsigned++; break;
        case TK_KW_FLOAT:    is_float++;    break;
        case TK_KW_DOUBLE:   is_float++;    break;
        }
        if (!starts_type(tok))
            break;
        token = tok;
        next();
    }

    if (is_long > 1 && !is_float)
        acc_error_at(line, "'long long' is not supported yet");
    if (is_signed && is_unsigned)
        acc_error_at(line, "'signed' and 'unsigned' together");
    if (is_void && (is_char || is_short || is_int || is_long || is_signed || is_unsigned))
        acc_error_at(line, "'void' with another type");
    if (is_char && (is_short || is_long))
        acc_error_at(line, "'char' with another width");
    if (is_short && is_long)
        acc_error_at(line, "'short' and 'long' together");

    if (is_float) {
        if (is_char || is_short || is_int || is_signed || is_unsigned)
            acc_error_at(line, "a floating type with an integer one");
        if (is_long)
            acc_error_at(line, "'long double' is not supported: it is eight "
                               "bytes and agondev has no arithmetic for it");

        return TY_FLOAT;
    }
    if (is_void)
        return TY_VOID;
    if (is_char)
        return is_unsigned ? TY_UCHAR : TY_CHAR;
    if (is_short)
        return is_unsigned ? TY_USHORT : TY_SHORT;
    if (is_long)
        return is_unsigned ? TY_ULONG : TY_LONG;

    return is_unsigned ? TY_UINT : TY_INT;
}

static Type type_specifier(void)
{
    int line = tok_line;
    int first = tok;
    unsigned char alone = spec_alone[first];

    next();
    if (!starts_type(tok))
        return (Type) (alone - 1);      /* one keyword, which is most of them */

    return type_specifier_slow(first, line);
}

/* A type in a place that has to hold a value. */
static Type object_type(const char *what)
{
    int line = tok_line;
    Type type;

    if (!starts_type(tok))
        acc_error_at(line, "expected a type, found %s", tok_spelling(tok));
    type = type_specifier();
    if (type == TY_VOID)
        acc_error_at(line, "'void' is not a type %s can have", what);

    return type;
}

/* ------------------------------------------------------------------ */
/* statements and declarations                                         */

static void declaration(void)
{
    Type type = object_type("a variable");

    for (;;) {
        NameRef name;
        int off, sym;

        if (tok != TK_IDENT)
            acc_error_at(tok_line, "expected a name, found %s", tok_spelling(tok));
        name = tok_name;
        next();

        off = gen_local(type_size(type));
        if (accept(TK_ASSIGN)) {
            Type outer = narrow_dest;

            narrow_dest = (type_size(type) < ACC_INT_SIZE) ? type : 0;
            expr();
            narrow_dest = outer;
            vstore_local(off, type);
            vdrop();            /* a declaration is not an expression */
        }
        /* Pushed after the initialiser, so `int x = x;` does not see itself. */
        sym = sym_push(name, SYM_LOCAL, off);
        sym_at(sym)->type = type;

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
        if (starts_type(tok))
            acc_error_at(tok_line,
                         "a declaration has to be at the start of the function");
        statement();
    }
    expect(TK_RBRACE, "'}'");
}

static void condition(void)
{
    /* narrow_dest is not cleared here, and does not need to be: it is only
     * set while an assignment's right-hand side is being parsed, and a
     * condition belongs to an if or a while, which are statements. There is
     * no way to reach one from inside an expression. The same goes for a
     * return. If `?:` or a statement expression ever arrives, both become
     * reachable and will need it. */
    expect(TK_LPAREN, "'('");
    expr();
    expect(TK_RPAREN, "')'");
}

/* Dispatched on the token rather than tested against one keyword at a time.
 * Every statement in the program walks this, and a chain grows a comparison
 * for each form the language gains. */
static void statement(void)
{
    /* Whatever the last statement left in the frame's scratch area is done
     * with. This is the only place that is true: a long result lives there
     * and outlives the values it was computed from. */
    gen_stmt_end();

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
    int nparams = 0, argoff, params_first;
    Type ret_type;

    ret_type = object_type("a function");
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
    sym_at(fn)->type = ret_type;

    /* The first argument sits above the saved ix and the return address. */
    params_first = sym_params_begin();
    argoff = 2 * ACC_PTR_SIZE;
    if (tok == TK_KW_VOID) {
        next();
    } else if (tok != TK_RPAREN) {
        for (;;) {
            Type ptype = object_type("a parameter");
            int psym;

            if (tok != TK_IDENT)
                acc_error_at(tok_line, "expected a parameter name, found %s",
                             tok_spelling(tok));
            psym = sym_push(tok_name, SYM_LOCAL, argoff);
            sym_at(psym)->type = ptype;
            sym_param_add(ptype);

            /* Every argument occupies whole slots: one however narrow it is,
             * two for a long. That is what agondev does, and the narrow ones
             * are read from the low bytes of their slot. */
            argoff += type_wide(ptype) ? 2 * ACC_INT_SIZE : ACC_INT_SIZE;
            nparams++;
            next();
            if (!accept(TK_COMMA))
                break;
        }
    }
    expect(TK_RPAREN, "')'");
    expect(TK_LBRACE, "'{'");

    sym_set_params(fn, params_first, nparams);
    gen_func_begin(fn, nparams, ret_type);
    while (starts_type(tok))
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
