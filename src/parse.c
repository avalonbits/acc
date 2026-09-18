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
static void deref_rest(void);
static void conditional_rest(void);
static void primary(void);
static void binary_rest(int min_prec);

/* The width the value being parsed is going straight into, when that is
 * narrower than an int -- the destination of an assignment or an initialiser.
 * Zero the rest of the time.
 *
 * It is what makes byte arithmetic legal: computing in eight bits is
 * indistinguishable from C's promote-then-truncate exactly when the result is
 * truncated to that width and nothing wider ever sees it. */
static Type narrow_dest;


/* A call, from just past the '(' to just past the ')'.
 *
 * Both places a name can be followed by one -- as an operand, and at the
 * start of a statement where the name might instead have begun an assignment
 * -- had the same dozen lines, nested four deep in a function that was
 * already long. They are here once. Measured on the Agon it is neither
 * faster nor slower than the two copies were.
 */
static void call_rest(NameRef name)
{
    int fn = sym_find(name);
    int nargs = 0;

    /* Not seen yet: assumed to be a function defined further down the file.
     * gen_finish reports it if it never is. */
    if (fn == SYM_NONE)
        fn = sym_push(name, SYM_FUNC, 0);
    else if (sym_at(fn)->kind != SYM_FUNC)
        acc_error_at(tok_line, "'%s' is not a function", name_text(name));

    if (tok != TK_RPAREN) {
        Type outer = narrow_dest;

        /* An argument is not the destination: it is passed at int width
         * whatever the parameter is declared as. */
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
}

/* A word C99 reserves and acc has not implemented. Named rather than
 * described, because which one it is is the whole of what the reader needs:
 * there is nothing acc can say about `switch` that is not also true of
 * `goto`. */
static void reserved_word(void)
{
    acc_error_at(tok_line, "'%s' is not supported yet", name_text(tok_name));
}

/* The local called `name`, as a value -- or, when `++` or `--` follows it,
 * as the value it had before they changed it.
 *
 * Postfix binds tighter than anything else in an expression, tighter even
 * than a unary `*`, so `*p++` is `*(p++)`: it reads through p and leaves p
 * pointing at the next one. That falls out of handling it here, where the
 * name is and before whatever the caller does with its value. */
static void local_value(NameRef name)
{
    int sym = sym_find(name);
    const Sym *local;

    if (sym == SYM_NONE)
        acc_error_at(tok_line, "'%s' is not declared", name_text(name));

    /* Fetched once. Each sym_at is a call and a multiply by the size of a
     * Sym, and this is every read of every variable; nothing below pushes a
     * symbol, so the pointer stays good. */
    local = sym_at(sym);
    if (local->kind != SYM_LOCAL)
        acc_error_at(tok_line, "'%s' is not a variable", name_text(name));

    if (tok == TK_INC || tok == TK_DEC) {
        vpostfix_local(local->val, local->type,
                       tok == TK_INC ? TK_PLUS : TK_MINUS);
        next();

        return;
    }

    vpush_local(local->val, local->type);
}

/* `++x`, `--x`, `++*p`, `--*p`: the operand changed, and the answer is its
 * new value. The operand has to be somewhere a value can be stored -- a local,
 * or what a pointer points at -- which is checked here rather than left to
 * produce a value and then find nowhere to put it back. */
static void prefix_step(void)
{
    int op = (tok == TK_INC) ? TK_PLUS : TK_MINUS;
    const char *spelling = tok_spelling(tok);

    next();

    if (tok == TK_IDENT) {
        int sym = sym_find(tok_name);
        const Sym *local;

        if (sym == SYM_NONE)
            acc_error_at(tok_line, "'%s' is not declared", name_text(tok_name));
        local = sym_at(sym);
        if (local->kind != SYM_LOCAL)
            acc_error_at(tok_line, "'%s' cannot be changed by %s",
                         name_text(tok_name), spelling);
        next();
        vprefix_local(local->val, local->type, op);

        return;
    }

    if (tok == TK_STAR) {
        int stars = 0;

        while (accept(TK_STAR))
            stars++;
        primary();
        while (--stars > 0)
            vderef();
        vprefix_indirect(op);

        return;
    }

    acc_error_at(tok_line, "%s needs a variable, or something a pointer points "
                           "at, and this is %s", spelling, tok_spelling(tok));
}

/* `(*p)++` and `(*p)--`, which is how a count kept behind a pointer is
 * stepped -- `*p++` would step the pointer instead. Tried when a parenthesis
 * opens on a star, and given back to the ordinary path unless the star's
 * operand is followed directly by the closing parenthesis and then `++` or
 * `--`. Giving it back means reading through the pointer, so what the caller
 * parses next continues from the value, exactly as if this had not looked.
 *
 * Returns whether it consumed the whole parenthesis. */
static int paren_deref_step(void)
{
    int stars = 0;

    while (accept(TK_STAR))
        stars++;
    primary();
    while (--stars > 0)
        vderef();

    if (tok == TK_RPAREN) {
        next();
        if (tok == TK_INC || tok == TK_DEC) {
            vpostfix_indirect(tok == TK_INC ? TK_PLUS : TK_MINUS);
            next();
        } else {
            vderef();
        }

        return 1;
    }

    /* Not `(*p)` alone: the rest of what is inside the parenthesis is an
     * expression that happens to begin at a pointer -- a store through it,
     * or a read and then whatever follows. */
    deref_rest();
    if (tok == TK_QUESTION)
        conditional_rest();
    expect(TK_RPAREN, "')'");

    return 1;
}

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
        if (tok == TK_STAR && paren_deref_step())
            return;
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

    if (tok == TK_INC || tok == TK_DEC) {
        prefix_step();

        return;
    }

    /* `!x` is `x == 0`, compared at x's own type -- so a float is false only
     * at zero of either sign, and a long is false only when all four of its
     * bytes are. */
    if (tok == TK_NOT) {
        next();
        primary();
        vtruth(TK_EQ);

        return;
    }

    if (tok == TK_PLUS) {       /* unary plus is the value unchanged */
        next();
        primary();

        return;
    }

    /* A `*` or a `&` where an expression was meant to start is a dereference
     * or an address-of, not the binary operator of the same spelling. Both
     * bind tighter than any binary operator, which is what putting them here
     * rather than in binary_rest says. */
    if (tok == TK_STAR) {
        next();
        primary();
        vderef();

        return;
    }

    if (tok == TK_AMP) {
        int sym;

        next();
        if (tok != TK_IDENT)
            acc_error_at(tok_line, "'&' takes the address of a variable, and "
                                   "this is %s", tok_spelling(tok));
        sym = sym_find(tok_name);
        if (sym == SYM_NONE)
            acc_error_at(tok_line, "'%s' is not declared", name_text(tok_name));
        {
            const Sym *local = sym_at(sym);

            if (local->kind != SYM_LOCAL)
                acc_error_at(tok_line, "'%s' is not a variable, so it has no "
                                       "address to take", name_text(tok_name));
            vaddr_local(local->val, local->type);
        }
        next();

        return;
    }

    if (tok == TK_IDENT) {
        NameRef name = tok_name;

        next();

        if (accept(TK_LPAREN)) {
            call_rest(name);

            return;
        }

        local_value(name);

        return;
    }

    if (tok == TK_KW_RESERVED)
        reserved_word();

    /* An operator acc has not got to yet, where an expression was meant to
     * start. Name the operator, as expect() does when one turns up where a
     * statement was meant to end; "expected an expression" is true and sends
     * the reader looking for the wrong thing. */
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
    PREC_NONE        = 0,       /* not a binary operator */
    PREC_LOGICAL_OR  = 1,       /* || */
    PREC_LOGICAL_AND = 2,       /* && */
    PREC_BIT_OR      = 3,       /* | */
    PREC_BIT_XOR     = 4,       /* ^ */
    PREC_BIT_AND     = 5,       /* & */
    PREC_EQUALITY    = 6,       /* ==  != */
    PREC_RELATIONAL  = 7,       /* <  >  <=  >= */
    PREC_SHIFT       = 8,       /* <<  >> */
    PREC_ADDITIVE    = 9,       /* +  - */
    PREC_MULTIPLY    = 10,      /* *  /  % */

    PREC_LOWEST      = PREC_LOGICAL_OR  /* where a whole expression starts */
};

static const unsigned char prec[TK_COUNT] = {
    [TK_OROR]   = PREC_LOGICAL_OR,
    [TK_ANDAND] = PREC_LOGICAL_AND,
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
    return tok_pair(tok, TK_SEMI) || tok == TK_RPAREN;
}

/* The right-hand side of `&&` or `||`, with the left already on the stack.
 *
 * Unlike every other binary operator the left side is dealt with before the
 * right is parsed, because whether the right is evaluated at all depends on
 * it: that is the short circuit, and C guarantees it. The right operand is
 * parsed with no narrow destination whatever the assignment around it: its
 * value is only ever compared with zero, so truncating it to a byte first
 * would make 256 false.
 */
static void binary_rest(int min_prec);

static void logical_rest(int op, int op_prec)
{
    int settles = (op == TK_OROR);      /* the truth that decides it early */
    Type outer = narrow_dest;
    int early;

    early = gen_logic_left(settles);

    narrow_dest = 0;
    primary();
    binary_rest(op_prec + 1);
    narrow_dest = outer;

    gen_logic_right(settles, early);
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

        if (tok_pair(op, TK_ANDAND)) {
            logical_rest(op, op_prec);

            continue;
        }

        primary();
        binary_rest(op_prec + 1);       /* everything binding tighter first */

        /* Which of the six ways to apply an operator this is depends on the
         * types of the two values, which the generator has and this does not.
         * All this contributes is the one thing it knows and the generator
         * cannot: whether the text after the operator lets the result be
         * truncated as it goes rather than at the end. */
        vapply(op, (narrow_dest && transparent_op(op) && expression_ends_here())
                   ? narrow_dest : 0);
    }
}

static void binary(int min_prec)
{
    primary();
    binary_rest(min_prec);
}

/* Assignment is right associative and its left side has to be a name, which
 * is the whole of what a milestone with no pointers can assign to. */
/* `x op= y` is `x = x op y` with x evaluated once -- which for a local is no
 * restriction at all, and for `*p op= y` means the address is worked out
 * once and used twice, to read and then to write.
 *
 * The right side is parsed whole and with no narrow destination: it is one
 * operand of op, and truncating it to the width of x before op had seen it
 * would give `c += 200 + 100` the wrong answer. The operator itself may still
 * run at x's width, for the same reason `c = c + y` may -- the result goes
 * straight into x and nothing wider ever sees it.
 */
static const unsigned char compound_op[TK_COUNT] = {
    [TK_ADD_ASSIGN] = TK_PLUS,    [TK_SUB_ASSIGN] = TK_MINUS,
    [TK_MUL_ASSIGN] = TK_STAR,    [TK_DIV_ASSIGN] = TK_SLASH,
    [TK_MOD_ASSIGN] = TK_PERCENT,
    [TK_AND_ASSIGN] = TK_AMP,     [TK_OR_ASSIGN]  = TK_PIPE,
    [TK_XOR_ASSIGN] = TK_CARET,
    [TK_SHL_ASSIGN] = TK_SHL,     [TK_SHR_ASSIGN] = TK_SHR
};

static Type compound_narrow(int op, Type dest)
{
    return (type_size(dest) < ACC_INT_SIZE && transparent_op(op)) ? dest : 0;
}

static void compound_local(NameRef name)
{
    int op = compound_op[tok];
    int sym = sym_find(name);
    Type outer = narrow_dest;
    Type type;
    int off;

    if (sym == SYM_NONE)
        acc_error_at(tok_line, "'%s' is not declared", name_text(name));
    if (sym_at(sym)->kind != SYM_LOCAL)
        acc_error_at(tok_line, "'%s' cannot be assigned to", name_text(name));

    /* Taken now rather than looked up again after the right side: a call in
     * it to a function not yet seen pushes a symbol and moves the locals
     * along, so the index would be stale. The frame offset is not. */
    {
        const Sym *local = sym_at(sym);

        type = local->type;
        off = local->val;
    }
    next();

    vpush_local(off, type);
    narrow_dest = 0;
    expr();
    narrow_dest = outer;
    vapply(op, compound_narrow(op, type));
    vstore_local(off, type);
}

static void compound_indirect(void)
{
    int op = compound_op[tok];
    Type outer = narrow_dest;
    Type target = type_pointer(vtype()) ? type_deref(vtype()) : 0;

    next();
    vdup();                     /* the address: once to read, once to write */
    vderef();
    narrow_dest = 0;
    expr();
    narrow_dest = outer;
    vapply(op, target ? compound_narrow(op, target) : 0);
    vstore_indirect();
}

static void assignment(void)
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
            if (dest != SYM_NONE) {
                const Sym *local = sym_at(dest);

                if (local->kind == SYM_LOCAL
                    && type_size(local->type) < ACC_INT_SIZE)
                    narrow_dest = local->type;
            }
            expr();
            narrow_dest = outer;

            /* Looked up again, since the right side may have pushed a symbol
             * and moved this one -- but only once. */
            sym = sym_find(name);
            {
                const Sym *local = (sym == SYM_NONE) ? NULL : sym_at(sym);

                if (!local || local->kind != SYM_LOCAL)
                    acc_error_at(tok_line, "'%s' cannot be assigned to",
                                 name_text(name));
                vstore_local(local->val, local->type);
            }

            return;
        }

        if (compound_op[tok]) {
            compound_local(name);

            return;
        }

        /* Not an assignment. Put the name back by handling it here rather
         * than by pushing the token back, which would need a queue. */
        if (accept(TK_LPAREN)) {
            call_rest(name);
        } else {
            local_value(name);
        }

        binary_rest(PREC_LOWEST);

        return;
    }

    /* A dereference, which may be what is being assigned to. The stars are
     * counted here rather than left to primary() because the address has to
     * reach the stack before the value does: `*p = v` evaluates p, then v,
     * then stores. All but the last star is a read; the last one is either
     * the store or, if no `=` follows, a read like the others. */
    if (tok == TK_STAR) {
        int stars = 0;

        while (accept(TK_STAR))
            stars++;
        primary();
        while (--stars > 0)
            vderef();
        deref_rest();

        return;
    }

    binary(PREC_LOWEST);
}

/* What follows `*operand`, with the address on the stack and the last star
 * not yet applied to it: a store through it, a compound store, or -- if no
 * assignment follows -- a read, and then the rest of an expression that
 * began with that read. */
static void deref_rest(void)
{
    if (accept(TK_ASSIGN)) {
        Type outer = narrow_dest;
        Type target = type_pointer(vtype()) ? type_deref(vtype()) : 0;

        narrow_dest = (target && type_size(target) < ACC_INT_SIZE) ? target : 0;
        expr();
        narrow_dest = outer;
        vstore_indirect();

        return;
    }

    if (compound_op[tok]) {
        compound_indirect();

        return;
    }

    vderef();
    binary_rest(PREC_LOWEST);
}

/* An expression, and then whatever the expression did not consume, checked
 * for the two things that end up here by mistake.
 *
 * An assignment operator left over means its left side was not something
 * that can be assigned to -- `3 = x`, `a + b += 1` -- which is a better thing
 * to say than that a semicolon was expected.
 *
 * A `?` left over is where a conditional starts. It is taken here, after the
 * assignment, because it binds more loosely than everything but assignment:
 * in `a = b ? c : d` the assignment has already consumed the whole of it as
 * its right side, and in `a + b ? c : d` the condition is `a + b`. */
static void expr(void)
{
    assignment();

    if (tok == TK_QUESTION)
        conditional_rest();
    if (tok == TK_ASSIGN || compound_op[tok] || tok == TK_INC || tok == TK_DEC)
        acc_error_at(tok_line, "the left of %s is not something that can be "
                               "assigned to", tok_spelling(tok));
}

/* The third operand of `?:`, which may itself be a conditional -- `a ? b : c ?
 * d : e` groups to the right -- but not an assignment: C reads `a ? b : c = d`
 * as an assignment to the whole conditional, which cannot be assigned to. */
static void conditional(void)
{
    binary(PREC_LOWEST);
    if (tok == TK_QUESTION)
        conditional_rest();
}

/* `cond ? middle : third`, with the condition on the stack.
 *
 * Only one of the two is evaluated, and which is decided at run time, so each
 * is generated on its own path -- and the type the answer has is decided by
 * both of them together, which is only known once the second has been parsed.
 * The first path therefore parks its value and jumps forward to a stub the
 * generator writes afterwards, where its conversion to the common type can
 * finally be emitted. Neither side is narrowed on the way: which type wins is
 * not known while either is being parsed. */
static void conditional_rest(void)
{
    Type outer = narrow_dest;
    Type middle;
    int slot, to_third, to_stub, middle_null;

    next();
    to_third = gen_cond_begin(&slot);

    narrow_dest = 0;
    expr();
    expect(TK_COLON, "':'");
    to_stub = gen_cond_middle(slot, &middle, &middle_null);

    gen_label(to_third);
    conditional();
    gen_cond_end(to_stub, slot, middle, middle_null);
    narrow_dest = outer;
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

/* The keywords at the front of a declaration, which say what the type is
 * before any star has narrowed it down. `int *p, q;` has one of these and two
 * declarators, and q is an int: a star belongs to the name it is written next
 * to and not to the type at the head of the line. */
static Type base_type(void)
{
    int line = tok_line;

    if (tok == TK_KW_RESERVED)
        reserved_word();
    if (!starts_type(tok))
        acc_error_at(line, "expected a type, found %s", tok_spelling(tok));

    return type_specifier();
}

/* The stars in front of one name, and then the check that what is left is a
 * type a value can have. That check has to come after them: there is no value
 * of type void, but `void *` is an ordinary pointer to something unsaid. */
static Type declarator_type(Type base, const char *what)
{
    int line = tok_line;

    while (accept(TK_STAR)) {
        if (type_ptr_depth(base) == TY_PTR_MAX)
            acc_error_at(line, "a pointer can be %d deep and this is deeper",
                         TY_PTR_MAX);
        base = type_ptr_to(base);
    }

    if (base == TY_VOID)
        acc_error_at(line, "'void' is not a type %s can have", what);

    return base;
}

/* A type and one declarator together, for the places that have only one. */
static Type object_type(const char *what)
{
    return declarator_type(base_type(), what);
}

/* ------------------------------------------------------------------ */
/* statements and declarations                                         */

static void declaration(void)
{
    Type base = base_type();

    for (;;) {
        Type type = declarator_type(base, "a variable");
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

    /* Before anything else, because most of what is missing from acc is a
     * statement: for, do, break, continue, switch, goto. Left to fall through
     * they lex as names and the complaint is that the name is not declared. */
    if (tok == TK_KW_RESERVED)
        reserved_word();

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
