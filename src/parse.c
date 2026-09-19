/*
 * The parser, which is also the code generator's caller.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
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

/* A global, as its address: the pointer `*` would take to reach it.
 *
 * A global is at an address fixed when it is declared, so reading one is
 * reading through a pointer that is a constant, and everything that works on
 * `*p` -- a store, `+=`, `++` either side, a value that is four bytes wide --
 * works on a global by pushing this and carrying on exactly as it would after
 * a star. */
static void global_address(const Sym *global)
{
    if (type_ptr_depth(global->type) == TY_PTR_MAX)
        acc_error_at(tok_line, "a pointer can be %d deep and this is deeper",
                     TY_PTR_MAX);
    vpush_const(global->val, type_ptr_to(global->type));
}

/* The object whose address is on the stack, as an operand: its value, or --
 * when `++` or `--` follows -- the value it had before they changed it. */
static void object_value(void)
{
    if (tok == TK_INC || tok == TK_DEC) {
        vpostfix_indirect(tok == TK_INC ? TK_PLUS : TK_MINUS);
        next();

        return;
    }
    vderef();
}

/* `[index]`, with a pointer on the stack: the address of the element it
 * picks, which is `pointer + index` with the step C gives it. */
static void subscript(void)
{
    Type outer = narrow_dest;

    if (!type_pointer(vtype()))
        acc_error_at(tok_line, "'[' needs an array or a pointer, and this is %s",
                     type_float(vtype()) ? "a floating-point value"
                                         : "an integer");
    next();
    narrow_dest = 0;            /* the index is not the destination */
    expr();
    narrow_dest = outer;
    expect(TK_RBRACKET, "']'");
    vapply(TK_PLUS, 0);
}

/* What a name turned out to be, once it and any subscripts after it have been
 * read. */
enum {
    NAME_LOCAL,     /* a local that is not an array, not subscripted: nothing
                     * is pushed, and the caller has its own ways with one */
    NAME_OBJECT,    /* the address of something that can be assigned to: a
                     * global, or an element */
    NAME_VALUE      /* a value that is not an object: an array, which is the
                     * address of its first element */
};

/* A name that has been looked up and read, and the subscripts after it.
 *
 * Everything but a plain local is reached through an address, which is what
 * lets all of it share the code that `*p` already has: a global is at a
 * constant address, an array is the address of its first element, and
 * `a[i]` is the address `a + i`. A second subscript reads the element first,
 * since it has to be a pointer to be subscripted again: that is `p[i][j]` on
 * an array of pointers. Arrays of arrays are not here yet. */
__attribute__((noinline))
static int name_operand(int sym, NameRef name)
{
    const Sym *s;

    if (sym == SYM_NONE)
        acc_error_at(tok_line, "'%s' is not declared", name_text(name));
    s = sym_at(sym);

    switch (s->kind) {
    case SYM_LOCAL:
        if (tok != TK_LBRACKET)
            return NAME_LOCAL;
        vpush_local(s->val, s->type);
        break;
    case SYM_GLOBAL:
        global_address(s);
        if (tok != TK_LBRACKET)
            return NAME_OBJECT;
        vderef();
        break;
    case SYM_LOCAL_ARRAY:
        vaddr_array(s->val, s->type);
        if (tok != TK_LBRACKET)
            return NAME_VALUE;
        break;
    case SYM_GLOBAL_ARRAY:
        if (type_ptr_depth(s->type) == TY_PTR_MAX)
            acc_error_at(tok_line, "a pointer can be %d deep and this is deeper",
                         TY_PTR_MAX);
        vpush_const(s->val, type_ptr_to(s->type));
        if (tok != TK_LBRACKET)
            return NAME_VALUE;
        break;
    default:
        acc_error_at(tok_line, "'%s' is not a variable", name_text(name));
    }

    for (;;) {
        subscript();
        if (tok != TK_LBRACKET)
            return NAME_OBJECT;
        vderef();
    }
}

/* Subscripts after a value that is not a name -- `(p + 1)[i]`, `f()[i]` --
 * and then the element's value. */
__attribute__((noinline))
static void subscript_value(void)
{
    for (;;) {
        subscript();
        if (tok != TK_LBRACKET)
            break;
        vderef();
    }
    object_value();
}

/* Anything but a plain local, as an operand. Out of line, like the other path
 * these take below, so that reading a local -- which is most of what a
 * program does -- carries none of it. */
__attribute__((noinline))
static void object_operand(int sym, NameRef name)
{
    if (name_operand(sym, name) == NAME_OBJECT)
        object_value();
}

/* The variable called `name`, as a value -- or, when `++` or `--` follows it,
 * as the value it had before they changed it.
 *
 * Postfix binds tighter than anything else in an expression, tighter even
 * than a unary `*`, so `*p++` is `*(p++)`: it reads through p and leaves p
 * pointing at the next one. That falls out of handling it here, where the
 * name is and before whatever the caller does with its value.
 *
 * With the name already looked up. Inlined into both callers: as a call of
 * its own it was one more on the path of every local read. */
static inline __attribute__((always_inline))
void symbol_value(int sym, NameRef name)
{
    const Sym *local;

    if (sym == SYM_NONE)
        acc_error_at(tok_line, "'%s' is not declared", name_text(name));

    /* Fetched once, so the fields below are read through one pointer held in
     * a register; nothing below pushes a symbol, so it stays good. */
    local = sym_at(sym);
    if (local->kind != SYM_LOCAL) {
        object_operand(sym, name);

        return;
    }

    /* `++`, `--` or `[` after it, tested as one range: the bracket was a test
     * of its own on every local read, and cost 0.5% of a compile. */
    if ((unsigned char) (tok - TK_INC) < 3u) {
        if (tok_is(TK_LBRACKET)) {
            object_operand(sym, name);

            return;
        }
        vpostfix_local(local->val, local->type,
                       tok == TK_INC ? TK_PLUS : TK_MINUS);
        next();

        return;
    }

    vpush_local(local->val, local->type);
}

static void local_value(NameRef name)
{
    symbol_value(sym_find(name), name);
}

/* `++x`, `--x`, `++*p`, `--*p`: the operand changed, and the answer is its
 * new value. The operand has to be somewhere a value can be stored -- a local,
 * or what a pointer points at -- which is checked here rather than left to
 * produce a value and then find nowhere to put it back.
 *
 * Out of line, which it stopped being when it grew: inlined, its locals gave
 * primary() -- which every operand goes through -- a larger frame. */
__attribute__((noinline))
static void prefix_step(void)
{
    int op = (tok == TK_INC) ? TK_PLUS : TK_MINUS;
    const char *spelling = tok_spelling(tok);

    next();

    if (tok == TK_IDENT) {
        NameRef name = tok_name;
        int sym = sym_find(name);
        int line = tok_line;

        next();
        switch (name_operand(sym, name)) {
        case NAME_LOCAL: {
            const Sym *local = sym_at(sym);

            vprefix_local(local->val, local->type, op);

            return;
        }
        case NAME_OBJECT:
            vprefix_indirect(op);

            return;
        }
        acc_error_at(line, "'%s' cannot be changed by %s", name_text(name),
                     spelling);
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

/* `&name`, `&name[i]`: the address of a variable or an element. Out of line:
 * inlined, its locals gave primary() -- which every operand goes through -- a
 * larger frame. */
__attribute__((noinline))
static void address_of(void)
{
    NameRef name;
    int sym, line;

    next();
    if (tok != TK_IDENT)
        acc_error_at(tok_line, "'&' takes the address of a variable, and "
                               "this is %s", tok_spelling(tok));
    name = tok_name;
    line = tok_line;
    sym = sym_find(name);
    if (sym != SYM_NONE && sym_at(sym)->kind == SYM_FUNC)
        acc_error_at(line, "'%s' is not a variable, so it has no address to "
                           "take", name_text(name));
    next();
    switch (name_operand(sym, name)) {
    case NAME_LOCAL: {
        const Sym *local = sym_at(sym);

        vaddr_local(local->val, local->type);

        return;
    }
    case NAME_OBJECT:
        return;                 /* the address is what is wanted */
    }
    acc_error_at(line, "the address of a whole array is not supported yet; "
                       "&%s[0] is the address of its first element",
                 name_text(name));
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

    /* A parenthesis is an operand of whatever surrounds it, so nothing in it
     * is the last operation before the destination, whatever the `)` that
     * closes it looks like to expression_ends_here: `c = (x ^ y) & z` ran
     * the `^` in A, as if its result were going straight into c, and the
     * load of z then overwrote it. An assignment inside sets its own. */
    if (tok == TK_LPAREN) {
        Type outer = narrow_dest;

        next();
        narrow_dest = 0;
        if (tok == TK_STAR && paren_deref_step()) {
            narrow_dest = outer;

            return;
        }
        expr();
        narrow_dest = outer;
        expect(TK_RPAREN, "')'");
        if (tok == TK_LBRACKET)
            subscript_value();

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
        address_of();

        return;
    }

    if (tok == TK_IDENT) {
        NameRef name = tok_name;

        next();

        if (accept(TK_LPAREN)) {
            call_rest(name);
            if (tok == TK_LBRACKET)
                subscript_value();

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

/* Anything but a plain local at the start of an expression, where it may be
 * assigned to: an object's address, and then what follows it handled as what
 * follows `*p` is -- or, for an array, its value and the rest of the
 * expression, which will refuse an `=` after it. Out of line for the reason
 * object_operand is. */
__attribute__((noinline))
static void object_statement(int sym, NameRef name)
{
    if (name_operand(sym, name) != NAME_OBJECT) {
        binary_rest(PREC_LOWEST);

        return;
    }
    if (tok == TK_INC || tok == TK_DEC) {
        object_value();
        binary_rest(PREC_LOWEST);

        return;
    }
    deref_rest();
}

static void assignment(void)
{
    if (tok == TK_IDENT) {
        NameRef name = tok_name;
        int sym;

        /* Look one token ahead by remembering this one: an identifier
         * followed by '=' is an assignment, anything else is a value. */
        next();
        if (accept(TK_LPAREN)) {
            call_rest(name);
            binary_rest(PREC_LOWEST);

            return;
        }

        /* A global or an element is reached through its address, so what
         * follows it is handled as what follows `*p` is: a store, a compound
         * store, a step, or a read and the rest of the expression. */
        sym = sym_find(name);
        if (sym != SYM_NONE
            && (sym_at(sym)->kind != SYM_LOCAL || tok_is(TK_LBRACKET))) {
            object_statement(sym, name);

            return;
        }

        if (tok == TK_ASSIGN) {
            int dest = sym;
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
        symbol_value(sym, name);
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

/* The stars in front of one name. Inlined, as not_void is: they were one
 * function before the split, and as two calls each they cost 1% of a compile
 * of a program that declares a lot of names. */
static inline __attribute__((always_inline))
Type declarator_stars(Type base)
{
    int line = tok_line;

    while (accept(TK_STAR)) {
        if (type_ptr_depth(base) == TY_PTR_MAX)
            acc_error_at(line, "a pointer can be %d deep and this is deeper",
                         TY_PTR_MAX);
        base = type_ptr_to(base);
    }

    return base;
}

/* That what the stars left is a type a value can have. The check has to come
 * after them: there is no value of type void, but `void *` is an ordinary
 * pointer to something unsaid. */
static inline __attribute__((always_inline))
void not_void(Type type, const char *what, int line)
{
    if (type == TY_VOID)
        acc_error_at(line, "'void' is not a type %s can have", what);
}

/* The stars in front of one name, and then that check. */
static Type declarator_type(Type base, const char *what)
{
    int line = tok_line;

    base = declarator_stars(base);
    not_void(base, what, line);

    return base;
}

/* A type and one declarator together, for the places that have only one. */
static Type object_type(const char *what)
{
    return declarator_type(base_type(), what);
}

/* ------------------------------------------------------------------ */
/* statements and declarations                                         */

/* A constant integer a declaration needs now: an array's size. Parsed as an
 * expression, which has to fold to a constant with nothing emitted, as a
 * global's initial value does. */
static int constant_folded(const char *what, int line, int before)
{
    int val;
    Type type;

    if (!vconst_top(&val, &type) || out_here() != before
        || type_pointer(type) || type_float(type))
        acc_error_at(line, "%s has to be a constant integer", what);
    vdrop();

    return val;
}

static int constant_int(const char *what, int line)
{
    int before = out_here();
    Type outer = narrow_dest;

    narrow_dest = 0;
    binary(PREC_LOWEST);
    narrow_dest = outer;

    return constant_folded(what, line, before);
}

/* `[N]` or `[]` after a name being declared: the number of elements, -1 for
 * `[]`, and 0 when there are no brackets at all. */
static int array_suffix(Type elem)
{
    int line = tok_line, n = -1;

    if (!accept(TK_LBRACKET))
        return 0;
    if (tok != TK_RBRACKET) {
        n = constant_int("an array's size", line);
        if (n <= 0)
            acc_error_at(line, "an array needs at least one element");
        if (n > 0x7fffff / type_size(elem))
            acc_error_at(line, "an array this large does not fit in memory");
    }
    expect(TK_RBRACKET, "']'");
    if (tok == TK_LBRACKET)
        acc_error_at(tok_line, "arrays of arrays are not supported yet");

    return n;
}

/* A local array, from just past its brackets.
 *
 * The elements an initialiser gives are stored one at a time, each at the
 * array's address plus its offset, and any it does not give are zeroed
 * after them -- which is what C says they start as, and which leaves an
 * array with no initialiser as whatever the stack held, as C also says. A
 * `[]` array is as long as its initialiser, which is only known once the
 * brace closes; its size is given then, the elements having been stored
 * against an address that is only filled in when the function ends. */
__attribute__((noinline))
static void local_array(Type elem, NameRef name, int count, int line)
{
    int array = gen_local_array();
    int step = type_size(elem), n = 0, sym;

    if (count > 0)
        gen_local_array_size(array, count * step);

    if (accept(TK_ASSIGN)) {
        expect(TK_LBRACE, "'{'");
        while (tok != TK_RBRACE) {
            Type outer = narrow_dest;

            if (n == count)
                acc_error_at(tok_line, "more initial values than the array has "
                                       "elements");
            vaddr_array(array, elem);
            if (n) {
                vpush_const(n, TY_INT);
                vapply(TK_PLUS, 0);
            }
            narrow_dest = (step < ACC_INT_SIZE) ? elem : 0;
            expr();
            narrow_dest = outer;
            vstore_indirect();
            vdrop();
            gen_stmt_end();
            n++;
            if (!accept(TK_COMMA))
                break;
        }
        expect(TK_RBRACE, "'}'");

        if (count < 0) {
            if (n == 0)
                acc_error_at(line, "an array needs at least one element");
            count = n;
            gen_local_array_size(array, count * step);
        } else if (n < count) {
            gen_zero_array(array, n * step, (count - n) * step);
        }
    } else if (count < 0) {
        acc_error_at(line, "an array declared with [] needs initial values to "
                           "say how long it is");
    }

    sym = sym_push(name, SYM_LOCAL_ARRAY, array);
    sym_at(sym)->type = elem;
}

/* Inlined into both callers, the function body and a for's first clause:
 * it was inlined into the first when it had only that one, and as a call it
 * is one more on every declaration in the program. */
static inline __attribute__((always_inline))
void declaration(void)
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

        if (tok == TK_LBRACKET) {
            int line = tok_line;

            local_array(type, name, array_suffix(type), line);
            if (!accept(TK_COMMA))
                break;
            continue;
        }

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
static void condition(void);

/* ------------------------------------------------------------------ */
/* break, continue, and the cases of a switch                          */

/* Jumps waiting for an address that is not known yet: a `break` for the end
 * of the loop or switch it leaves, and a `continue` in a do-while for the
 * condition, which comes after the body. Each construct notes how many there
 * were when it began and fills in the ones above that when it ends, so
 * nesting takes care of itself: the inner one has always finished with its
 * own before the outer one looks. */
typedef struct {
    int *at;
    int  count, cap;
} Holes;

static Holes breaks, continues;

static void hole_push(Holes *h, int hole)
{
    if (h->count == h->cap) {
        h->cap = h->cap ? h->cap * 2 : 16;
        h->at = realloc(h->at, (size_t) h->cap * sizeof *h->at);
        if (!h->at)
            acc_error("out of memory for jumps");
    }
    h->at[h->count++] = hole;
}

/* Every hole above `mark`, filled in with here. */
static void holes_land(Holes *h, int mark)
{
    while (h->count > mark)
        gen_label(h->at[--h->count]);
}

/* What `break` and `continue` mean where the parser is now. A loop sets all
 * three; a switch sets only where a break goes, which is why a `continue` in
 * a switch in a loop continues the loop. */
typedef struct {
    int break_mark;     /* breaks from here up are this construct's; -1: none */
    int continue_mark;  /* the same for continues; -1: not in a loop */
    int continue_to;    /* where a continue goes, when that is already known;
                         * -1 when it is further on, as in a do-while */
} Jumps;

static Jumps jumps = { -1, -1, -1 };

/* A switch the parser is inside, for the case labels in it -- which may be
 * anywhere in its body, inside loops and blocks, and in Duff's device are. */
typedef struct {
    int  case_mark;     /* its cases are the ones from here up; -1: none */
    int  default_at;    /* where `default:` is, -1 if there is none yet */
    Type type;          /* what the cases are converted to */
} Switch;

static Switch in_switch = { -1, -1, TY_INT };

static long *case_value;
static int  *case_at;
static int   ncases, cases_cap;

/* The contexts of the loops and switches the parser is inside, outermost
 * first, three ints apiece on a stack walked by a pointer.
 *
 * Kept here rather than in the frames of the functions that parse them: one
 * in for_statement's frame made it nine bytes larger and a compile of a
 * program full of for loops 3% slower on the Agon. And kept as ints through
 * a pointer rather than as an array of Jumps, which cost a multiply by the
 * size of one -- a call on this target -- and a copy of it, each way: about
 * 1100 cycles a loop. */
static int *jump_stack, *jump_top, *jump_limit;

__attribute__((noinline))
static void jumps_grow(void)
{
    int used = (int) (jump_top - jump_stack);
    int cap = used ? used * 2 : 24;

    jump_stack = realloc(jump_stack, (size_t) cap * sizeof *jump_stack);
    if (!jump_stack)
        acc_error("out of memory for nested loops");
    jump_top = jump_stack + used;
    jump_limit = jump_stack + cap;
}

static inline __attribute__((always_inline)) void jumps_push(void)
{
    if (jump_top == jump_limit)
        jumps_grow();
    jump_top[0] = jumps.break_mark;
    jump_top[1] = jumps.continue_mark;
    jump_top[2] = jumps.continue_to;
    jump_top += 3;
}

static inline __attribute__((always_inline)) void jumps_pop(void)
{
    jump_top -= 3;
    jumps.break_mark = jump_top[0];
    jumps.continue_mark = jump_top[1];
    jumps.continue_to = jump_top[2];
}

static void loop_begin(int continue_to)
{
    jumps_push();
    jumps.break_mark = breaks.count;
    jumps.continue_mark = continues.count;
    jumps.continue_to = continue_to;
}

/* The end of a loop: its breaks land here, which is past everything in it. */
static void loop_end(void)
{
    holes_land(&breaks, jumps.break_mark);
    jumps_pop();
}

__attribute__((noinline))
static void break_statement(void)
{
    int line = tok_line;

    next();
    expect(TK_SEMI, "';'");
    if (jumps.break_mark < 0)
        acc_error_at(line, "'break' is not inside a loop or a switch");
    hole_push(&breaks, gen_jump());
}

__attribute__((noinline))
static void continue_statement(void)
{
    int line = tok_line;

    next();
    expect(TK_SEMI, "';'");
    if (jumps.continue_mark < 0)
        acc_error_at(line, "'continue' is not inside a loop");
    if (jumps.continue_to >= 0)
        gen_jump_to(jumps.continue_to);
    else
        hole_push(&continues, gen_jump());
}

/* `while (condition) body`, out of statement() for the reason for_statement
 * is: its saved jumps would otherwise be in the frame of every statement. */
__attribute__((noinline))
static void while_statement(void)
{
    int top, to_end;

    next();
    top = gen_here();
    condition();
    to_end = gen_jump_if_false();
    loop_begin(top);
    statement();
    gen_jump_to(top);
    gen_label(to_end);
    loop_end();
}

/* `do body while (condition);` -- the body first, then the test, and back to
 * the top while it holds. A continue goes to the test, which is only reached
 * once the body has been read, so it waits in the list with the breaks. */
__attribute__((noinline))
static void do_statement(void)
{
    int top = gen_here();

    next();
    loop_begin(-1);
    statement();
    expect(TK_KW_WHILE, "'while' after the body of a do");
    holes_land(&continues, jumps.continue_mark);
    condition();
    expect(TK_SEMI, "';'");
    gen_jump_if_true_to(top);
    loop_end();
}

/* The constant a case label names, converted to the type the switch compares
 * at. A literal, with a sign or without, is read directly, which is the only
 * way to get one wider than an int -- the value stack holds constants at int
 * width; anything else has to fold to a constant, as an array's size does. */
static long case_constant(void)
{
    int line = tok_line;
    long value;

    if (tok == TK_INT && type_wide(tok_type)) {
        value = tok_val;
        next();
    } else if (tok == TK_MINUS) {
        /* A sign binds to what follows it and no further, so `-3 + 2` is
         * -1: a wide literal after it is negated here, and anything else
         * goes back into the expression the way a unary minus does. */
        int before = out_here();

        next();
        if (tok == TK_INT && type_wide(tok_type)) {
            value = -tok_val;
            next();
        } else {
            Type outer = narrow_dest;

            narrow_dest = 0;
            primary();
            vneg();
            binary_rest(PREC_LOWEST);
            narrow_dest = outer;
            value = constant_folded("a case label", line, before);
        }
    } else {
        value = constant_int("a case label", line);
    }

    if (type_wide(in_switch.type))
        return (long) (uint32_t) value;

    return value & 0xffffff;
}

/* `case constant:` -- where the code for it starts, noted for the tests at
 * the end of the switch. The statement after it is parsed by the caller. */
__attribute__((noinline))
static void case_label(void)
{
    int line = tok_line, i;
    long value;

    next();
    if (in_switch.case_mark < 0)
        acc_error_at(line, "'case' is not inside a switch");
    value = case_constant();
    expect(TK_COLON, "':' after a case");

    for (i = in_switch.case_mark; i < ncases; i++)
        if (case_value[i] == value)
            acc_error_at(line, "this switch already has a case for %ld",
                         type_unsigned(in_switch.type) || type_wide(in_switch.type)
                         ? value : (long) ((value ^ 0x800000) - 0x800000));

    if (ncases == cases_cap) {
        cases_cap = cases_cap ? cases_cap * 2 : 16;
        case_value = realloc(case_value, (size_t) cases_cap * sizeof *case_value);
        case_at = realloc(case_at, (size_t) cases_cap * sizeof *case_at);
        if (!case_value || !case_at)
            acc_error("out of memory for case labels");
    }
    case_value[ncases] = value;
    case_at[ncases] = gen_here();
    ncases++;
}

__attribute__((noinline))
static void default_label(void)
{
    int line = tok_line;

    next();
    expect(TK_COLON, "':' after default");
    if (in_switch.case_mark < 0)
        acc_error_at(line, "'default' is not inside a switch");
    if (in_switch.default_at >= 0)
        acc_error_at(line, "this switch already has a default");
    in_switch.default_at = gen_here();
}

/* `switch (value) body`.
 *
 * One pass, so the body is compiled where it is read, and a case label is
 * only an address noted on the way: which is what lets one sit anywhere in
 * the body -- inside a loop, as Duff's device has them -- and be jumped to
 * from outside. The tests come after the body, where every case is known:
 *
 *         value to a frame slot
 *         goto tests
 *         body, with the cases in it
 *         goto end
 *   tests: if value == case 1 goto it ... else goto default, or end
 *   end:
 *
 * The value is compared at its promoted type, as C says: a char is compared
 * as the int it becomes, and each case is converted to that type. */
__attribute__((noinline))
static void switch_statement(void)
{
    Switch saved_switch = in_switch;
    Type type;
    int line, slot, to_tests, i;

    next();
    expect(TK_LPAREN, "'('");
    line = tok_line;
    expr();
    expect(TK_RPAREN, "')'");
    type = vtype();
    if (type_pointer(type) || type_float(type))
        acc_error_at(line, "a switch needs an integer, and this is %s",
                     type_pointer(type) ? "a pointer" : "a floating-point value");
    type = type_promote(type);
    vconvert(type);
    slot = gen_local(type_size(type));
    vstore_local(slot, type);
    vdrop();
    to_tests = gen_jump();

    in_switch.case_mark = ncases;
    in_switch.default_at = -1;
    in_switch.type = type;
    jumps_push();
    jumps.break_mark = breaks.count;

    statement();

    hole_push(&breaks, gen_jump());
    gen_label(to_tests);
    gen_stmt_end();
    gen_switch_load(slot, type);
    for (i = in_switch.case_mark; i < ncases; i++)
        gen_switch_case(case_value[i], type, case_at[i]);
    if (in_switch.default_at >= 0)
        gen_jump_to(in_switch.default_at);
    else
        hole_push(&breaks, gen_jump());

    ncases = in_switch.case_mark;
    holes_land(&breaks, jumps.break_mark);
    in_switch = saved_switch;
    jumps_pop();
}

/* ------------------------------------------------------------------ */
/* goto and labels                                                     */

/* The labels of the function being compiled. They are a namespace of their
 * own -- a label may share its name with a variable -- and they belong to the
 * whole function, so a goto may name one further down. That one is not known
 * yet, so its jump is left as a hole and filled in when the label is
 * reached; a label never reached is an error once the function ends, blamed
 * on the first goto that named it. */
typedef struct {
    NameRef name;
    int     at;             /* its address, or -1 until it is reached */
    int     line;           /* where it was first named */
} Label;

static Label *labels;
static int    nlabels, labels_cap;

/* The gotos still waiting for their label: the hole, and which label. */
static int *goto_hole, *goto_label;
static int  ngotos, gotos_cap;

static int label_find(NameRef name, int line)
{
    int i;

    for (i = 0; i < nlabels; i++)
        if (labels[i].name == name)
            return i;

    if (nlabels == labels_cap) {
        labels_cap = labels_cap ? labels_cap * 2 : 8;
        labels = realloc(labels, (size_t) labels_cap * sizeof *labels);
        if (!labels)
            acc_error("out of memory for labels");
    }
    labels[nlabels].name = name;
    labels[nlabels].at = -1;
    labels[nlabels].line = line;

    return nlabels++;
}

__attribute__((noinline))
static void goto_statement(void)
{
    int line = tok_line, label;

    next();
    if (tok != TK_IDENT)
        acc_error_at(tok_line, "'goto' needs a label, and this is %s",
                     tok_spelling(tok));
    label = label_find(tok_name, line);
    next();
    expect(TK_SEMI, "';'");

    if (labels[label].at >= 0) {
        gen_jump_to(labels[label].at);

        return;
    }
    if (ngotos == gotos_cap) {
        gotos_cap = gotos_cap ? gotos_cap * 2 : 8;
        goto_hole = realloc(goto_hole, (size_t) gotos_cap * sizeof *goto_hole);
        goto_label = realloc(goto_label, (size_t) gotos_cap * sizeof *goto_label);
        if (!goto_hole || !goto_label)
            acc_error("out of memory for gotos");
    }
    goto_hole[ngotos] = gen_jump();
    goto_label[ngotos] = label;
    ngotos++;
}

/* `name: statement` -- the label is here, and every goto that was waiting for
 * it lands here too. */
__attribute__((noinline))
static void label_statement(void)
{
    int line = tok_line, label = label_find(tok_name, line), i, kept = 0;

    if (labels[label].at >= 0)
        acc_error_at(line, "the label '%s' is defined twice",
                     name_text(tok_name));
    next();
    expect(TK_COLON, "':'");
    labels[label].at = gen_here();

    for (i = 0; i < ngotos; i++) {
        if (goto_label[i] == label) {
            gen_label(goto_hole[i]);
            continue;
        }
        goto_hole[kept] = goto_hole[i];
        goto_label[kept] = goto_label[i];
        kept++;
    }
    ngotos = kept;

    statement();
}

/* The end of a function: every goto has to have found its label. */
static void labels_end(void)
{
    if (ngotos)
        acc_error_at(labels[goto_label[0]].line, "the label '%s' is used but "
                     "never defined", name_text(labels[goto_label[0]].name));
    nlabels = 0;
}

/* `for (init; condition; step) body`.
 *
 * One pass, so the code comes out in the order it is read, and the step --
 * written before the body, run after it -- has to be jumped around to get
 * there:
 *
 *         init
 *   top:  if (!condition) goto end
 *         goto body
 *   step: step
 *         goto top
 *   body: body
 *         goto step
 *   end:
 *
 * Two jumps a trip where a compiler that could reorder would have one. A
 * missing step leaves out its block and the body goes straight back to the
 * top; a missing condition leaves out the test, which is C's `for (;;)`.
 *
 * The init may declare, as C99 lets it, and what it declares ends with the
 * loop: `for (int i = 0; ...)` twice in one function is two variables, and
 * neither is visible after its loop.
 *
 * Out of line: inlined into statement(), which every statement in the
 * program goes through, it gave that a larger frame and cost 1.5% of a
 * compile of programs with no for loop in them. */
__attribute__((noinline))
static void for_statement(void)
{
    int mark = sym_scope_begin();
    int top, to_end = -1, to_body, again;

    next();
    expect(TK_LPAREN, "'('");

    if (starts_type(tok)) {
        declaration();                  /* and its semicolon */
    } else {
        if (tok != TK_SEMI) {
            expr();
            vdrop();
        }
        expect(TK_SEMI, "';'");
    }

    gen_stmt_end();
    top = gen_here();
    if (tok != TK_SEMI) {
        expr();
        to_end = gen_jump_if_false();
    }
    expect(TK_SEMI, "';'");

    again = top;
    if (tok != TK_RPAREN) {
        to_body = gen_jump();
        again = gen_here();
        gen_stmt_end();
        expr();
        vdrop();
        gen_jump_to(top);
        gen_label(to_body);
    }
    expect(TK_RPAREN, "')'");

    loop_begin(again);
    statement();
    gen_jump_to(again);
    if (to_end >= 0)
        gen_label(to_end);
    loop_end();

    sym_scope_end(mark);
}

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

    /* Before anything else, because what is missing from acc is mostly a
     * statement -- goto, for one. Left to fall through they lex as names and
     * the complaint is that the name is not declared. */
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

    case TK_KW_WHILE:
        while_statement();

        return;

    case TK_KW_DO:
        do_statement();

        return;

    case TK_KW_SWITCH:
        switch_statement();

        return;

    case TK_KW_BREAK:
        break_statement();

        return;

    case TK_KW_CONTINUE:
        continue_statement();

        return;

    /* A label and then the statement it labels, which may be another. */
    case TK_KW_CASE:
        case_label();
        statement();

        return;

    case TK_KW_DEFAULT:
        default_label();
        statement();

        return;

    case TK_LBRACE:
        next();
        block();

        return;

    case TK_KW_FOR:
        for_statement();

        return;

    case TK_KW_RETURN: {
        int line = tok_line;

        next();
        if (tok != TK_SEMI)
            expr();
        expect(TK_SEMI, "';'");
        gen_return(line);

        return;
    }

    case TK_SEMI:
        next();

        return;

    case TK_KW_ELSE:
        acc_error_at(tok_line, "'else' without an 'if'");

        return;

    case TK_KW_GOTO:
        goto_statement();

        return;

    default:
        if (tok == TK_IDENT && lex_colon_follows()) {
            label_statement();

            return;
        }
        expr();
        vdrop();                /* the value of a statement is discarded */
        expect(TK_SEMI, "';'");

        return;
    }
}

/* A function's definition, from just past its name. */
static void function_rest(Type ret_type, NameRef name)
{
    int fn;
    int nparams = 0, argoff, params_first;

    expect(TK_LPAREN, "'('");

    fn = sym_find(name);
    if (fn != SYM_NONE && sym_at(fn)->kind == SYM_GLOBAL)
        acc_error_at(tok_line, "'%s' is already a variable", name_text(name));
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
            NameRef pname;
            int psym;

            if (tok != TK_IDENT)
                acc_error_at(tok_line, "expected a parameter name, found %s",
                             tok_spelling(tok));
            pname = tok_name;
            next();

            /* `int a[]` and `int a[10]` declare a parameter that is a
             * pointer, as C says: an array is passed as the address of its
             * first element, and the size, if there is one, is not kept. */
            if (tok == TK_LBRACKET) {
                array_suffix(ptype);
                if (type_ptr_depth(ptype) == TY_PTR_MAX)
                    acc_error_at(tok_line, "a pointer can be %d deep and this "
                                           "is deeper", TY_PTR_MAX);
                ptype = type_ptr_to(ptype);
            }

            psym = sym_push(pname, SYM_LOCAL, argoff);
            sym_at(psym)->type = ptype;
            sym_param_add(ptype);

            /* Every argument occupies whole slots: one however narrow it is,
             * two for a long. That is what agondev does, and the narrow ones
             * are read from the low bytes of their slot. */
            argoff += type_wide(ptype) ? 2 * ACC_INT_SIZE : ACC_INT_SIZE;
            nparams++;
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
    labels_end();
    gen_func_end();

    sym_drop_locals();
}

/* A global's initial value, as the bytes it starts with.
 *
 * It has to be known now, because it is written into the image here, so it
 * has to be a constant -- which is what C says too. A literal, with a sign
 * or without, is read directly, which is the only way to get a long or a
 * float: those are too wide for the value stack to hold as a constant.
 * Anything else is parsed as an ordinary expression and has to fold to a
 * constant without the code generator emitting anything: `3 * 4 + 1`, `-5`,
 * `&counter`. */
static void global_initializer(Type type, unsigned char *bytes, int line)
{
    int size = type_size(type);
    int negative = 0;
    uint32_t value;
    int i;

    if (tok == TK_MINUS || tok == TK_PLUS) {
        negative = (tok == TK_MINUS);
        next();
        if (tok != TK_INT && tok != TK_FLOAT)
            goto expression;
    }

    if (tok == TK_FLOAT) {
        uint32_t bits;

        if (!type_float(type))
            acc_error_at(line, "a floating-point initial value for an integer "
                               "or a pointer is not supported yet");
        memcpy(&bits, &tok_fval, sizeof bits);
        value = bits ^ (negative ? 0x80000000u : 0);
        next();
        goto store;
    }

    if (tok == TK_INT && type_wide(tok_type)) {
        value = (uint32_t) tok_val;
        if (type_float(type))
            value = float_from_int(value, negative);
        else if (negative)
            value = 0u - value;
        next();
        goto store;
    }

    if (negative) {
        /* A sign in front of something narrower: put it back into the
         * expression by negating what that comes to. */
        primary();
        vneg();
        binary_rest(PREC_LOWEST);
        goto folded;
    }

expression:
    binary(PREC_LOWEST);

folded:
    {
        int before = out_here();
        int val;
        Type from;

        if (!type_wide(type) && !type_float(type))
            vconvert(type);
        if (!vconst_top(&val, &from) || out_here() != before)
            acc_error_at(line, "a global's initial value has to be a constant");
        vdrop();

        if (type_float(type)) {
            if (type_unsigned(from))
                value = float_from_int((uint32_t) val & 0xffffff, 0);
            else if (val < 0)
                value = float_from_int((uint32_t) -(long) val, 1);
            else
                value = float_from_int((uint32_t) val, 0);
        } else if (type_unsigned(from)) {
            value = (uint32_t) val & 0xffffff;
        } else {
            value = (uint32_t) (long) val;      /* sign-extended */
        }
    }

store:
    for (i = 0; i < size; i++) {
        bytes[i] = (unsigned char) value;
        value >>= 8;
    }
}

/* A file-scope array, from just past its brackets: the elements written into
 * the image as they are read, each a constant as a global's value has to be,
 * and zeros for the rest. A `[]` array is as long as its initialiser. */
static void global_array(Type elem, NameRef name, int count, int line)
{
    int at = out_here(), step = type_size(elem), n = 0, sym, i;

    if (accept(TK_ASSIGN)) {
        expect(TK_LBRACE, "'{'");
        while (tok != TK_RBRACE) {
            unsigned char bytes[ACC_LONG_SIZE] = { 0 };

            if (n == count)
                acc_error_at(tok_line, "more initial values than the array has "
                                       "elements");
            global_initializer(elem, bytes, tok_line);
            for (i = 0; i < step; i++)
                out_byte(bytes[i]);
            n++;
            if (!accept(TK_COMMA))
                break;
        }
        expect(TK_RBRACE, "'}'");
        if (count < 0) {
            if (n == 0)
                acc_error_at(line, "an array needs at least one element");
            count = n;
        }
    } else if (count < 0) {
        acc_error_at(line, "an array declared with [] needs initial values to "
                           "say how long it is");
    }

    for (i = n * step; i < count * step; i++)
        out_byte(0);

    sym = sym_push(name, SYM_GLOBAL_ARRAY, at);
    sym_at(sym)->type = elem;
}

/* One file-scope variable: its bytes written into the image where it is
 * declared, and its name bound to where they went.
 *
 * Between two functions is as good a place as any: nothing runs into it,
 * since every function ends in a return, and the address is known the moment
 * it is written, so nothing that refers to it ever needs patching. A global
 * with no initial value is zero, as C says, and takes its bytes in the image
 * like any other -- there is no separate zeroed area yet. */
static void global_variable(Type type, NameRef name, int line)
{
    unsigned char bytes[ACC_LONG_SIZE] = { 0 };
    int size = type_size(type), sym, i, at;

    /* C lets `int x;` be said twice at file scope, as long as at most one of
     * them gives a value. acc takes one declaration of a global for now. */
    sym = sym_find(name);
    if (sym != SYM_NONE)
        acc_error_at(line, sym_at(sym)->kind == SYM_FUNC
                           ? "'%s' is already a function"
                           : "'%s' is already declared, and a second "
                             "declaration of a global is not supported yet",
                     name_text(name));

    if (tok == TK_LBRACKET) {
        global_array(type, name, array_suffix(type), line);

        return;
    }

    if (accept(TK_ASSIGN))
        global_initializer(type, bytes, line);

    at = out_here();
    for (i = 0; i < size; i++)
        out_byte(bytes[i]);

    sym = sym_push(name, SYM_GLOBAL, at);
    sym_at(sym)->type = type;
}

/* What is at file scope: a function's definition, or a list of variables.
 * Which one shows only once the name has been read, by whether a '(' comes
 * next -- the type and the stars in front of the name are the same for
 * both. */
static void external_declaration(void)
{
    Type base = base_type();
    int line = tok_line;
    Type type = declarator_stars(base);
    NameRef name;

    if (tok != TK_IDENT)
        acc_error_at(tok_line, "expected a name, found %s", tok_spelling(tok));
    name = tok_name;
    next();

    if (tok == TK_LPAREN) {
        function_rest(type, name);

        return;
    }

    for (;;) {
        not_void(type, "a variable", line);
        global_variable(type, name, line);
        if (!accept(TK_COMMA))
            break;

        line = tok_line;
        type = declarator_stars(base);
        if (tok != TK_IDENT)
            acc_error_at(tok_line, "expected a name, found %s",
                         tok_spelling(tok));
        name = tok_name;
        next();
    }
    expect(TK_SEMI, "';'");
}

static void translation_unit(void)
{
    while (tok != TK_EOF)
        external_declaration();
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
