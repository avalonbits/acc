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
static void comma_expr(void);

/* The function being compiled, for __func__ and for the variadic forms;
 * SYM_NONE between functions. */
static int current_fn = SYM_NONE;
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
static void global_address(int sym);

/* A call's arguments, from just past its `(` to just past its `)`: pushed
 * in order. Returns how many. */
static int call_args(void)
{
    int nargs = 0;

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

    return nargs;
}

/* A call through the pointer to a function on the stack, from just past
 * its `(`. */
__attribute__((noinline))
static void call_value(void)
{
    int x = vext(), nargs;

    if (vtype() != type_ptr_to(TY_FUNC))
        acc_error_at(tok_line, "only a function or a pointer to one can be "
                               "called");
    nargs = call_args();
    if (ext_func_declared(x) && nargs != ext_func_count(x)
        && !(nargs > ext_func_count(x) && ext_func_variadic(x)))
        acc_error_at(tok_line, "this function takes %d argument%s, and this "
                               "call gives it %d", ext_func_count(x),
                     ext_func_count(x) == 1 ? "" : "s", nargs);
    gen_call_indirect(nargs);
}

/* A variable's value, for a call through it: `fp(3)`. */
__attribute__((noinline))
static void call_variable(int sym, NameRef name)
{
    const Sym *v = sym_at(sym);

    switch (v->kind) {
    case SYM_LOCAL:
    case SYM_LOCAL_CONST:
        vpush_local(v->val, v->type);
        break;
    case SYM_GLOBAL:
    case SYM_GLOBAL_CONST:
        global_address(sym);
        vderef();
        break;
    default:
        acc_error_at(tok_line, "'%s' is not a function", name_text(name));
    }
    vset_ext(sym_at(sym)->ext);
    call_value();
}

static void call_rest(NameRef name)
{
    int fn = sym_find(name);
    int nargs, nparams;

    /* Not seen yet: assumed to be a function defined further down the file.
     * gen_finish reports it if it never is. */
    if (fn == SYM_NONE) {
        fn = sym_push(name, SYM_FUNC, 0);
    } else if (sym_at(fn)->kind != SYM_FUNC) {
        call_variable(fn, name);

        return;
    }

    nargs = call_args();
    nparams = sym_nparams(fn);
    if (nargs != nparams && (sym_flags(fn) & SYMF_PARAMS)
        && !(nargs > nparams && (sym_flags(fn) & SYMF_VARIADIC)))
        acc_error_at(tok_line, "'%s' takes %s%d argument%s, and this call gives "
                               "it %d", name_text(name),
                     sym_flags(fn) & SYMF_VARIADIC ? "at least " : "",
                     nparams, nparams == 1 ? "" : "s", nargs);
    gen_call(fn, nargs, sym_params_first(fn), nparams);
}

/* A function's own type, as the extension its address carries. */
static int function_ext(int fn)
{
    const Sym *f = sym_at(fn);

    return ext_func(f->type, f->ext, sym_params_first(fn), sym_nparams(fn),
                    ((sym_flags(fn) & SYMF_PARAMS) != 0)
                    | ((sym_flags(fn) & SYMF_VARIADIC) ? 2 : 0));
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
static void global_address(int sym)
{
    const Sym *global = sym_at(sym);

    if (type_ptr_depth(global->type) == TY_PTR_MAX)
        acc_error_at(tok_line, "a pointer can be %d deep and this is deeper",
                     TY_PTR_MAX);

    /* In the bss, whose start is not known until the image is finished --
     * but where in it this variable is, is. So the offset is what goes on
     * the value stack, and it folds like any other constant: `a[3]` is one
     * load with nine in it, and the start of the bss is added to the nine
     * wherever it is written out.
     *
     * -1 is what a declaration that only said extern left, and what one
     * whose size is not known yet leaves: nothing here knows where the
     * variable is at all, so the address is written down for gen_finish or
     * the linker to fill in.
     *
     * Against the file-scope symbol, not against this one. An `extern` in a
     * block is a copy of the file-scope symbol taken when the declaration
     * was read, and a copy made before the address was known does not have
     * it -- and is dropped at the end of the block, long before gen_finish
     * comes looking. For a symbol that is already the file-scope one this
     * finds the same symbol again. */
    if (sym_in_bss(global->val)) {
        vpush_bss(sym_bss_at(global->val), type_ptr_to(global->type));
    } else if (global->val < 0) {
        int g = name_global(global->name);

        vpush_global_addr(g == SYM_NONE ? sym : g);
    } else {
        vpush_const(global->val, type_ptr_to(global->type));
        vset_addr();
    }
    global = sym_at(sym);
    if (global->ext)
        vset_ext(global->ext);
    if (global->quals)
        vset_quals(global->quals);
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
    comma_expr();
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
    NAME_VALUE,     /* a value that is not an object: an array, which is the
                     * address of its first element */
    NAME_CONST,     /* an enum constant, as a constant int */
    NAME_READONLY,  /* a const variable's value, which is not an object:
                     * nothing can be stored to it */
    NAME_RESULT,    /* what a call through a pointer came to: a value */
    NAME_FUNC       /* a function's name alone: its address */
};

static const char *record_name(int x);
static void        func_suffix(Type *type, int *ext);
static void        record_complete(int x, int line);

/* `.name` or `->name`, with the struct's address on the stack and the
 * operator not yet read: the member's address in its place. */
__attribute__((noinline))
static void member(void)
{
    int line = tok_line, arrow = (tok == TK_ARROW), x = vext(), m;
    Type type = vtype();
    NameRef name;

    next();
    if (type != type_ptr_to(TY_STRUCT))
        acc_error_at(line, arrow ? "'->' needs a pointer to a struct or union"
                                 : "'.' needs a struct or union");
    record_complete(x, line);
    if (tok != TK_IDENT)
        acc_error_at(tok_line, "expected a member's name, found %s",
                     tok_spelling(tok));
    name = tok_name;
    m = member_find(x, name);
    if (m < 0)
        acc_error_at(tok_line, "'%s' has no member '%s'", record_name(x),
                     name_text(name));
    next();
    vmember(member_offset(m), member_type(m), member_ext(m), member_quals(m));
    if (member_bits(m))
        vset_bits(member_bits(m));
}

/* The subscripts and members after an operand: `a[i]`, `s.m`, `p->m`, in any
 * number and order. `object` says whether what is on the stack is an
 * object's address -- a global, an element, a member -- or a value, and so
 * whether the address has to be read first to have a pointer: `p[i]` on a
 * global p reads p, and on an array does not. Each of them leaves an object.
 * Returns whether what is on the stack is one. */
enum { POST_VALUE, POST_OBJECT, POST_CALL };

static int postfix_chain(int object)
{
    for (;;) {
        if (tok == TK_LBRACKET) {
            if (object == POST_OBJECT)
                vderef();
            subscript();
        } else if (tok == TK_DOT) {
            /* A struct as a value -- what a call returned, or `(*p)` -- is
             * its address, which is the object's. */
            if (object != POST_OBJECT) {
                if (!type_is_struct(vtype()))
                    acc_error_at(tok_line, "'.' needs a struct or union");
                vset_type(type_ptr_to(TY_STRUCT), vext());
            }
            member();
        } else if (tok == TK_ARROW) {
            if (object == POST_OBJECT)
                vderef();
            member();
        } else if (tok == TK_LPAREN) {
            /* A call through a pointer to a function: `s.op(1)`,
             * `table[i](2)`, `get()(3)`. Its result is a value. */
            if (object == POST_OBJECT)
                vderef();
            next();
            call_value();
            object = POST_CALL;
            continue;
        } else {
            return object;
        }
        object = POST_OBJECT;
    }
}

/* Whether a subscript or a member follows. */
#define tok_postfix()  ((unsigned char) (tok_low - TK_LBRACKET) < 3u)

/* A name that has been looked up and read, and the subscripts and members
 * after it.
 *
 * Everything but a plain local is reached through an address, which is what
 * lets all of it share the code that `*p` already has: a global is at a
 * constant address, an array is the address of its first element, and
 * `a[i]` is the address `a + i`. A second subscript reads the element first,
 * since it has to be a pointer to be subscripted again: that is `p[i][j]` on
 * an array of pointers. */
/* __func__, which C99 says is declared in every function body as
 *
 *     static const char __func__[] = "the function's name";
 *
 * Made where it is first used and not before: a program that never asks for
 * it carries neither the bytes nor the symbol, and the test is one strcmp
 * on the path that was about to report an undeclared name anyway.
 *
 * The bytes go into the image the way a string literal's do, and the symbol
 * is pushed in the function's own scope, so it goes when the function does
 * and the next one makes its own. SYM_GLOBAL_ARRAY is what it is: an array
 * at an address that is known, which is what a string literal leaves too.
 * The const is what makes `__func__[0] = 'x'` the error C says it is. */
static int func_name_symbol(NameRef name)
{
    const char *text;
    int len, at, sym;

    if (current_fn == SYM_NONE || strcmp(name_text(name), "__func__") != 0)
        return SYM_NONE;

    text = name_text(sym_at(current_fn)->name);
    len = (int) strlen(text);
    at = gen_data(text, len);
    sym = sym_push_local(name, SYM_GLOBAL_ARRAY, at);
    sym_at(sym)->type = TY_CHAR;        /* char[], so its extension is the
                                         * element's, which char has none of */
    sym_at(sym)->quals = SQ_CONST;
    sym_set_count(sym, len + 1);        /* the terminator counts, as sizeof
                                         * of a string literal does */

    return sym;
}

/* A name being read, and the symbol it stands for: __func__ is made here if
 * that is what it is, so that the caller holds the symbol and not the
 * SYM_NONE it looked up. */
static int sym_find_value(NameRef name)
{
    int sym = sym_find(name);

    return sym == SYM_NONE ? func_name_symbol(name) : sym;
}

__attribute__((noinline))
static int name_operand(int sym, NameRef name)
{
    const Sym *s;
    int object;

    if (sym == SYM_NONE)
        acc_error_at(tok_line, "'%s' is not declared", name_text(name));
    s = sym_at(sym);

    switch (s->kind) {
    case SYM_LOCAL:
        if (!tok_postfix())
            return NAME_LOCAL;
        vpush_local(s->val, s->type);
        vset_ext(s->ext);
        vset_quals(s->quals);
        object = 0;
        break;
    case SYM_GLOBAL:
        global_address(sym);
        object = 1;
        break;
    case SYM_LOCAL_ARRAY:
        vaddr_array(s->val, s->type);
        vset_ext(s->ext);
        vset_quals(s->quals);
        object = 0;
        break;
    case SYM_LOCAL_VLA:
        /* The pointer the declaration left: an array is its first
         * element's address wherever it is used, and here that address is
         * a value the program worked out. */
        vpush_local(s->val, s->type);
        vset_ext(s->ext);
        vset_quals(s->quals);
        object = 0;
        break;
    case SYM_LOCAL_STRUCT:
        vaddr_array(s->val, TY_STRUCT);
        vset_ext(s->ext);
        vset_quals(s->quals);
        object = 1;
        break;
    case SYM_GLOBAL_ARRAY:
        global_address(sym);
        object = 0;
        break;
    case SYM_CONST:
        vpush_const(s->val, TY_INT);

        return NAME_CONST;
    case SYM_FUNC:
        vpush_function(sym);
        vset_ext(function_ext(sym));
        if (!tok_postfix() && tok != TK_LPAREN)
            return NAME_FUNC;
        object = POST_VALUE;
        break;
    case SYM_LOCAL_CONST:
        vpush_local(s->val, s->type);
        vset_ext(s->ext);
        vset_quals(s->quals);
        if (!tok_postfix())
            return NAME_READONLY;
        object = 0;
        break;
    case SYM_GLOBAL_CONST:
        global_address(sym);
        if (!tok_postfix()) {
            vderef();

            return NAME_READONLY;
        }
        object = 1;
        break;
    default:
        acc_error_at(tok_line, "'%s' is not a variable", name_text(name));
    }

    object = postfix_chain(object);

    return object == POST_OBJECT ? NAME_OBJECT
         : object == POST_CALL ? NAME_RESULT : NAME_VALUE;
}

/* Subscripts and members after a value that is not a name -- `(p + 1)[i]`,
 * `f()[i]`, `f().m` -- and then the element's value. */
__attribute__((noinline))
static void subscript_value(void)
{
    if (postfix_chain(POST_VALUE) == POST_OBJECT)
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

    /* `++`, `--`, `[`, `.` or `->` after it, tested as one range: the
     * bracket was a test of its own on every local read, and cost 0.5% of a
     * compile. */
    if ((unsigned char) (tok - TK_INC) < 5u) {
        if (tok_postfix()) {
            object_operand(sym, name);

            return;
        }
        vpostfix_local(local->val, local->type, local->ext,
                       tok == TK_INC ? TK_PLUS : TK_MINUS);
        vset_quals(local->quals);
        next();

        return;
    }
    vpush_local(local->val, local->type);
    if (local->ext)
        vset_ext(local->ext);
    if (local->quals)
        vset_quals(local->quals);
}

static void local_value(NameRef name)
{
    symbol_value(sym_find_value(name), name);
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

            vprefix_local(local->val, local->type, local->ext, op);

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
        while (--stars != 0)
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
static inline __attribute__((always_inline))
int paren_deref_step(void)
{
    int stars = 0;

    while (accept(TK_STAR))
        stars++;
    primary();
    while (--stars != 0)
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

/* A const variable's address, which its name alone does not give: it reads
 * as its value. */
__attribute__((noinline))
static void readonly_address(int sym)
{
    const Sym *v = sym_at(sym);

    /* The variable is const, so what its address points at is. */
    if (v->kind == SYM_GLOBAL_CONST) {
        global_address(sym);
        vset_quals(VQ_CONST);

        return;
    }
    vaddr_local(v->val, v->type);
    vset_ext(v->ext);
    vset_quals(VQ_CONST);
}

/* `&name`, `&name[i]`: the address of a variable or an element. Out of line:
 * inlined, its locals gave primary() -- which every operand goes through -- a
 * larger frame. */
__attribute__((noinline))
static void address_of_literal(int line);

static void address_of(void)
{
    NameRef name;
    int sym, line;

    next();

    /* `&(struct s){ ... }`: a compound literal is an object, and this is
     * the address of the one it makes. Out of line, next to the rest of the
     * literals: starts_decl has to stay inlined into its callers, and a
     * call to it from here is one more of those on a path that is rare. */
    if (tok == TK_LPAREN) {
        address_of_literal(tok_line);

        return;
    }

    if (tok != TK_IDENT)
        acc_error_at(tok_line, "'&' takes the address of a variable, and "
                               "this is %s", tok_spelling(tok));
    name = tok_name;
    line = tok_line;
    sym = sym_find(name);
    next();
    switch (name_operand(sym, name)) {
    case NAME_LOCAL: {
        const Sym *local = sym_at(sym);

        if (local->quals & SQ_REGISTER)
            acc_error_at(line, "'%s' is declared register, so it has no "
                               "address to take", name_text(name));
        vaddr_local(local->val, local->type);
        vset_ext(local->ext);
        vset_quals(local->quals);

        return;
    }
    case NAME_OBJECT:
        if (vbits())
            acc_error_at(line, "a bit-field has no address to take");

        return;                 /* the address is what is wanted */
    case NAME_CONST:
        acc_error_at(line, "'%s' is a constant, which has no address",
                     name_text(name));
    case NAME_READONLY:
        vdrop();
        readonly_address(sym);

        return;
    case NAME_FUNC:
        return;                 /* `&f` is f's address, as `f` is */
    case NAME_RESULT:
        acc_error_at(line, "what a call comes to has no address to take");
    }

    /* An array whose length was worked out as the program ran is already
     * its own address: `&a` and `a` are the same three bytes, and the type
     * C gives the first -- a pointer to an array of n -- is not one acc has
     * a way of writing down. */
    if (sym_at(sym)->kind == SYM_LOCAL_VLA)
        return;

    /* A whole array: the same address as its first element, as a pointer to
     * the array, which steps over all of it at once. */
    {
        const Sym *array = sym_at(sym);

        if (sym_count(sym) < 0)
            acc_error_at(line, "'%s' has no size yet, so there is no whole "
                               "array to take the address of", name_text(name));

        vset_type(type_ptr_to(TY_EXT),
                  ext_array(array->type, array->ext, sym_count(sym)));
    }
}

/* Adjacent string literals, joined as C joins them: "ab" "cd" is "abcd".
 * The lexer's buffer is its own and reused for each token, so they are
 * gathered here. */
static char *str_joined;
static int   str_joined_cap;

static int string_gather(void)
{
    int len = 0;

    while (tok == TK_STRING) {
        if (len + tok_str_len >= str_joined_cap) {
            while (len + tok_str_len >= str_joined_cap)
                str_joined_cap = str_joined_cap ? str_joined_cap * 2 : 128;
            str_joined = realloc(str_joined, (size_t) str_joined_cap);
            if (!str_joined)
                acc_error("out of memory for a string");
        }
        /* Asked, because `""` is a string of no bytes and the lexer has
         * nothing to point at for it -- and memcpy from a null pointer is
         * undefined even when it is told to copy nothing. */
        if (tok_str_len)
            memcpy(str_joined + len, tok_str, (size_t) tok_str_len);
        len += tok_str_len;
        next();
    }

    return len;
}

/* A string literal as an operand: an array of char, somewhere in the image,
 * which is the address of its first character -- a constant pointer, since
 * where it went is known the moment it is written. */
__attribute__((noinline))
static void string_value(void)
{
    int len = string_gather();

    vpush_const(gen_data(str_joined, len), type_ptr_to(TY_CHAR));
    vset_addr();
    if (tok_postfix())
        subscript_value();
}

static void cast_rest(void);
static void cast_operand(Type to, int x, int quals);
static void sizeof_value(void);
static void compound_literal(Type type, int x, int count, Type elem,
                             int elem_x, int line, int address, int *countp);
static int  local_array_object(Type elem, int elem_x, int *countp, int line,
                               int braced);
static int  local_struct_object(int x, int line, int braced);
static void literal_bytes(Type type, int x, int count, Type elem, int elem_x,
                          int line, int address, int *countp);
static void va_form(void);
static inline __attribute__((always_inline)) int starts_decl(void);

/* A name used as a value: a local read, or a call. */
static void primary(void)
{
    if (tok == TK_FLOAT) {
        vpush_const_float(tok_fval);
        next();

        return;
    }

    if (tok == TK_INT) {
        if (type_eight(tok_type))
            vpush_const_wide((uint32_t) tok_val, tok_val_hi, tok_type);
        else if (type_wide(tok_type))
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
        if (starts_decl()) {
            cast_rest();

            return;
        }
        narrow_dest = 0;
        if (tok == TK_STAR && paren_deref_step()) {
            narrow_dest = outer;
            if (tok_postfix() || tok == TK_LPAREN)  /* (*p)[i], (*fp)(x) */
                subscript_value();

            return;
        }
        comma_expr();
        narrow_dest = outer;
        expect(TK_RPAREN, "')'");
        if (tok_postfix() || tok == TK_LPAREN)
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
            if (tok_postfix() || tok == TK_LPAREN)
                subscript_value();

            return;
        }

        local_value(name);

        return;
    }

    if (tok == TK_STRING) {
        string_value();

        return;
    }

    if (tok == TK_KW_SIZEOF) {
        sizeof_value();

        return;
    }

    /* va_start, va_arg, va_end, va_copy. */
    if ((unsigned char) (tok_low - TK_KW_VA_START) < 4u) {
        va_form();

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
    return transparent_op(op) ? type_narrow(dest) : 0;
}

static void compound_local(NameRef name)
{
    int op = compound_op[tok];
    int sym = sym_find(name);
    Type outer = narrow_dest;
    Type type;
    int off, ext;

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
        ext = local->ext;
    }
    next();

    vpush_local(off, type);
    vset_ext(ext);
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
    int what = name_operand(sym, name);

    if (what != NAME_OBJECT) {
        if (what == NAME_READONLY && (tok == TK_ASSIGN || compound_op[tok]
                                      || tok == TK_INC || tok == TK_DEC))
            acc_error_at(tok_line, "'%s' is const, so it cannot be changed",
                         name_text(name));
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

/* An expression that starts with a parenthesis, where it may be assigned to:
 * when subscripts follow the parenthesis, what they pick is an element, and
 * `(*p)[i] = v` stores into it -- p being a pointer to an array, whose star
 * needs the parenthesis to bind before the subscript does. Without
 * subscripts it is an ordinary value, and the rest of the expression
 * follows. */
__attribute__((noinline))
static void postfix_statement(void);

static void paren_statement(void)
{
    Type outer = narrow_dest;

    next();
    if (starts_decl()) {
        cast_rest();
        binary_rest(PREC_LOWEST);

        return;
    }
    narrow_dest = 0;
    if (!(tok == TK_STAR && paren_deref_step())) {
        comma_expr();
        expect(TK_RPAREN, "')'");
    }
    narrow_dest = outer;

    postfix_statement();
}

/* After a value at the start of an expression -- a parenthesis, a call --
 * the subscripts and members that make it an object, and then what may
 * follow an object: a store, a step, or a read and the rest. */
static void postfix_statement(void)
{
    if ((!tok_postfix() && tok != TK_LPAREN)
        || postfix_chain(POST_VALUE) != POST_OBJECT) {
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
    if (tok == TK_LPAREN) {
        paren_statement();

        return;
    }

    if (tok == TK_IDENT) {
        NameRef name = tok_name;
        int sym;

        /* Look one token ahead by remembering this one: an identifier
         * followed by '=' is an assignment, anything else is a value. */
        next();
        if (accept(TK_LPAREN)) {
            call_rest(name);
            postfix_statement();

            return;
        }

        /* A global or an element is reached through its address, so what
         * follows it is handled as what follows `*p` is: a store, a compound
         * store, a step, or a read and the rest of the expression. */
        sym = sym_find_value(name);
        if (sym != SYM_NONE
            && (sym_at(sym)->kind != SYM_LOCAL || tok_postfix())) {
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

                if (local->kind == SYM_LOCAL)
                    narrow_dest = type_narrow(local->type);
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
        while (--stars != 0)
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

        narrow_dest = target ? type_narrow(target) : 0;
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

/* `a, b`: the left side is evaluated for what it does and thrown away, and
 * the answer is the right side. The loosest binding there is, which is why
 * it is a level above expr rather than an entry in the precedence table.
 *
 * Most of the commas in a program are not this operator: the ones between a
 * call's arguments, between the values in braces and between the declarators
 * of one declaration are separators, and each of those parses expr and not
 * this. Where C says `expression` -- a statement, the clauses of a for, what
 * is inside `( )` or `[ ]`, what follows return -- comma_expr is what runs.
 *
 * vdrop is all that discarding the left side takes: whatever it did has been
 * emitted already, and the value stack entry is what is left of it. */
static void comma_expr(void)
{
    expr();
    while (tok == TK_COMMA) {
        next();
        vdrop();
        expr();
    }
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
    int slot, to_third, to_stub, middle_null, middle_ext;

    next();
    to_third = gen_cond_begin(&slot);

    narrow_dest = 0;
    comma_expr();
    expect(TK_COLON, "':'");
    to_stub = gen_cond_middle(&slot, &middle, &middle_ext, &middle_null);

    gen_label(to_third);
    conditional();
    gen_cond_end(to_stub, slot, middle, middle_ext, middle_null);
    narrow_dest = outer;
}

/* ------------------------------------------------------------------ */
/* types                                                               */

/* Whether the type base_type last read was const. Kept for the variable a
 * declaration names, which is refused as the target of an assignment; what
 * a pointer points at being const is taken and not checked. */
static unsigned char base_const;

/* Whether the token is const, volatile or restrict, which are next to each
 * other among the tokens. */
#define tok_qualifier()  ((unsigned char) (tok_low - TK_KW_CONST) < 3u)
typedef char qualifiers_are_adjacent[(TK_KW_VOLATILE == TK_KW_CONST + 1
                                      && TK_KW_RESTRICT == TK_KW_CONST + 2)
                                     ? 1 : -1];

/* The qualifiers, as many as there are, with the base_type's const marked:
 * see base_const.
 *
 * volatile asks that every access in the program be made, and acc makes
 * them all: it keeps nothing in a register from one statement to the next,
 * and reads a variable again each time an expression names it. restrict
 * promises something acc makes no use of. So neither changes the code. */
__attribute__((noinline))
static void qualifiers(void)
{
    while (tok_qualifier()) {
        if (tok == TK_KW_CONST)
            base_const = 1;
        next();
    }
}

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
    [TK_KW_DOUBLE]   = TY_FLOAT + 1,
    [TK_KW_BOOL]     = TY_BOOL + 1,
    [TK_KW_VA_LIST]  = type_ptr_to(TY_CHAR) + 1  /* walked along the slots */
};

static int starts_type(int token)
{
    return spec_alone[token] != 0;
}

/* The several-keyword case: `unsigned short int` and `int short unsigned` are
 * the same type, so they are counted rather than matched against a list. */
__attribute__((noinline))
static Type type_specifier_slow(int first, int line)
{
    int is_void = 0, is_char = 0, is_short = 0, is_int = 0;
    int is_long = 0, is_signed = 0, is_unsigned = 0, is_float = 0, is_bool = 0;
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
        case TK_KW_BOOL:     is_bool++;     break;
        }
        if (tok_qualifier())
            qualifiers();
        if (!starts_type(tok))
            break;
        token = tok;
        next();
    }

    if (is_bool) {
        if (is_bool > 1 || is_void || is_char || is_short || is_int || is_long
            || is_signed || is_unsigned || is_float)
            acc_error_at(line, "'_Bool' with another type");

        return TY_BOOL;
    }
    if (is_long > 2)
        acc_error_at(line, "'long long long' is not a type");
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
            acc_error_at(line, "'long double' is not supported: agondev's "
                               "library has no arithmetic for one, so a "
                               "program that asks for it does not link");

        return TY_FLOAT;
    }
    if (is_long > 1) {
        if (is_char || is_short)
            acc_error_at(line, "'long long' with another width");

        return is_unsigned ? TY_ULLONG : TY_LLONG;
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
    if (!starts_type(tok)) {
        if (!tok_qualifier())
            return (Type) (alone - 1);  /* one keyword, which is most of them */
        qualifiers();                   /* `int const`, `unsigned const int` */
        if (!starts_type(tok))
            return (Type) (alone - 1);
    }

    return type_specifier_slow(first, line);
}

/* ------------------------------------------------------------------ */
/* enums, tags and scopes                                              */

/* Whether the parser is inside a function body, where an enum constant, a
 * typedef or a tag belongs to the block it is declared in. */
static int in_body;

/* The function whose body is being read, for va_start to check it has a
 * `...` to start after. */


static NameRef declared_name(void);
static int     constant_int(const char *what, int line);

/* The mark of the innermost block, for telling a redeclaration in the same
 * scope -- an error -- from one that shadows an outer name. -1 at file
 * scope. */
static int scope_mark = -1;

static int push_here(NameRef name, int kind, int val)
{
    return in_body ? sym_push_local(name, kind, val) : sym_push(name, kind, val);
}

/* That `name` is not already declared in the scope being declared into. */
static void not_redeclared(NameRef name, int line)
{
    int sym = sym_find(name);

    if (sym != SYM_NONE && sym_declared_in(sym, scope_mark))
        acc_error_at(line, "'%s' is already declared", name_text(name));
}

/* A tag, as the name it is looked up by: the tag's text behind a `{`, which
 * no identifier can begin with. `struct s` and a variable `s` are different
 * things in C, and this keeps them apart with the one symbol table. */
static char *tag_text;
static int   tag_text_cap;

static NameRef tag_name(NameRef name)
{
    const char *text = name_text(name);
    int len = (int) strlen(text);

    if (len + 1 > tag_text_cap) {
        tag_text_cap = len + 32;
        tag_text = realloc(tag_text, (size_t) tag_text_cap);
        if (!tag_text)
            acc_error("out of memory for a tag");
    }
    tag_text[0] = '{';
    memcpy(tag_text + 1, text, (size_t) len);   /* copied: interning may move
                                                 * the arena it came from */

    return name_intern(tag_text, len + 1);
}

enum { TAG_ENUM, TAG_STRUCT, TAG_UNION };

static const char *const tag_keyword[] = { "enum", "struct", "union" };

/* The tag's symbol, if it is visible, checked to be the kind the keyword
 * said; SYM_NONE if it is not declared at all. */
static int tag_find(NameRef tag, int kind, NameRef name, int line)
{
    int sym = sym_find(tag);

    if (sym != SYM_NONE && sym_at(sym)->val != kind)
        acc_error_at(line, "'%s' is declared as a %s tag, not a %s one",
                     name_text(name), tag_keyword[sym_at(sym)->val],
                     tag_keyword[kind]);

    return sym;
}

/* `enum`, and a tag, a list of constants or both. Each constant is an int,
 * declared from the moment its name has been read, so a later one may be
 * defined in terms of an earlier. The type is unsigned int when none of them
 * is negative and int otherwise, which is what agondev makes of it. */
__attribute__((noinline))
static Type enum_specifier(void)
{
    int line = tok_line, negative = 0, val = 0, sym;
    NameRef name = NAME_NONE, tag = NAME_NONE;
    Type type;

    next();
    if (tok == TK_IDENT) {
        name = tok_name;
        tag = tag_name(name);
        next();
    }
    if (!accept(TK_LBRACE)) {
        if (!tag)
            acc_error_at(line, "'enum' needs a name or a list of constants");
        sym = tag_find(tag, TAG_ENUM, name, line);
        if (sym == SYM_NONE)
            acc_error_at(line, "'enum %s' is not defined", name_text(name));

        return sym_at(sym)->type;
    }

    while (tok != TK_RBRACE) {
        int cline = tok_line;
        NameRef constant = declared_name();

        if (accept(TK_ASSIGN))
            val = constant_int("an enum constant's value", cline);
        if (val > 0x7fffff)
            acc_error_at(cline, "an enum constant has to fit in an int");
        not_redeclared(constant, cline);
        sym_at(push_here(constant, SYM_CONST, val))->type = TY_INT;
        if (val < 0)
            negative = 1;
        val++;
        if (!accept(TK_COMMA))
            break;
    }
    expect(TK_RBRACE, "'}'");

    type = negative ? TY_INT : TY_UINT;
    if (tag) {
        not_redeclared(tag, line);
        sym = push_here(tag, SYM_TAG, TAG_ENUM);
        sym_at(sym)->type = type;
    }

    return type;
}

static int  base_ext;
static Type base_type(void);
static Type declarator_stars_out(Type base);
static NameRef direct_declarator_out(Type t, int tx, Type *type, int *ext,
                                     int *count);

/* The name a record is called by in a diagnostic: `struct point`, or
 * `struct` alone when it has no tag. */
static const char *record_name(int x)
{
    static char buf[80];
    NameRef tag = ext_tag(x);
    const char *kw = ext_is_union(x) ? "union" : "struct";

    if (!tag)
        return kw;
    snprintf(buf, sizeof buf, "%s %s", kw, name_text(tag) + 1);

    return buf;
}

/* That a record's members are known: it cannot be declared, sized or looked
 * into before they are. */
static void record_complete(int x, int line)
{
    if (!ext_complete(x))
        acc_error_at(line, "'%s' is declared but its members are not given",
                     record_name(x));
}

/* A record's members, from just past its `{`. Each is laid where the last
 * ended, with no padding: every type on this machine is aligned to a byte,
 * which is how agondev lays them out too. A union's are all at its start,
 * and it is as big as the biggest. */
/* A bit-field's width, from just past its `:`: an integer type, and no more
 * bits than it has -- one for a _Bool. Zero, which ends the byte the last
 * was in, only without a name. */
static int bitfield_width(Type type, NameRef name, int line)
{
    int width = constant_int("a bit-field's width", line);
    int most = type == TY_BOOL ? 1 : type_size(type) * 8;

    if (type_pointer(type) || type_float(type) || type_is_struct(type)
        || type_is_array(type) || type == TY_VOID)
        acc_error_at(line, "a bit-field has to have an integer type");
    if (type_eight(type))
        acc_error_at(line, "a bit-field of 'long long' is not supported: C99 "
                           "leaves the types past 'int' to the implementation");
    if (width < 0 || width > most)
        acc_error_at(line, "a bit-field of this type is 0 to %d bits wide",
                     most);
    if (!width && name)
        acc_error_at(line, "a bit-field of no bits cannot have a name");

    return width;
}

static void record_members(int x, int is_union, int line)
{
    /* Where the next member goes: a byte, and a bit within it, for a
     * bit-field to continue from. */
    int first = -1, last = -1, size = 0, bit = 0, has_bits = 0, flexible = 0;

    while (tok != TK_RBRACE) {
        Type base = base_type();
        int bx = base_ext;
        unsigned char bc = base_const;

        if (accept(TK_SEMI))
            continue;               /* a nested struct declared, nothing more */
        for (;;) {
            int mline = tok_line, count = 0, ext = bx, bytes, m, width = -1;
            int at, pos;
            Type type = base;
            NameRef name = NAME_NONE;

            if (tok != TK_COLON)        /* `int : 3` has no name */
                name = direct_declarator_out(declarator_stars_out(base),
                                             bx, &type, &ext, &count);
            if (!count)
                func_suffix(&type, &ext);
            if (type_is_func(type))
                acc_error_at(mline, "a member cannot be a function; a pointer "
                                    "to one can");
            if (accept(TK_COLON))
                width = bitfield_width(type, name, mline);
            /* `char b[];` last in a struct: C99's flexible array member,
             * which takes no room and stands for whatever was allocated
             * past the struct. Only last, only in a struct, and only where
             * something else came first -- a struct of nothing but one has
             * no size to allocate from. */
            if (count < 0) {
                if (is_union)
                    acc_error_at(mline, "a union's member that is an array "
                                        "needs its size");
                if (first < 0)
                    acc_error_at(mline, "an array with no size has to come "
                                        "after another member");
                if (width >= 0 || !name)
                    acc_error_at(mline, "an array with no size cannot be a "
                                        "bit-field");
                flexible = 1;
                count = 0;              /* no room, and no elements of its
                                         * own: the room is the caller's */
            }
            if (count || flexible) {
                ext = ext_array(type, ext, count);
                type = TY_EXT;
            }
            if (type == TY_VOID)
                acc_error_at(mline, "'void' is not a type a member can have");
            if (type_is_struct(type)) {
                record_complete(ext, mline);
                if (ext_has_flex(ext))
                    acc_error_at(mline, "'%s' ends in an array with no size, "
                                        "so it cannot be a member",
                                 record_name(ext));
            }
            for (m = first; name && m >= 0; m = member_next(m))
                if (member_name(m) == name)
                    acc_error_at(mline, "'%s' is already a member of '%s'",
                                 name_text(name), record_name(x));

            if (width >= 0) {
                /* A bit-field: where the last ended, if it fits in a unit
                 * of its type from the byte that is in, and from the next
                 * byte if not -- every type is aligned to a byte, so a
                 * unit can start at any of them. Measured against agondev
                 * on a thousand and a half structs made at random. */
                int unit = type == TY_BOOL ? 8 : type_size(type) * 8;

                has_bits = 1;
                if (is_union) {
                    size = (width + 7) / 8 > size ? (width + 7) / 8 : size;
                } else if (!width || bit + width > unit) {
                    size += bit ? 1 : 0;        /* the next byte */
                    bit = 0;
                }
                at = is_union ? 0 : size;
                pos = is_union ? 0 : bit;
                if (!is_union) {
                    bit += width;
                    size += bit / 8;
                    bit %= 8;
                }
                if (!name)
                    goto placed;                /* padding, and nothing else */
                m = member_add(name, type, ext, at, bc ? SQ_CONST : 0);
                member_set_bits(m, bitfield_intern(pos, width,
                                                   !type_unsigned(type)));
            } else {
                if (!is_union && bit) {         /* after bit-fields */
                    size++;
                    bit = 0;
                }
                bytes = type_bytes(type, ext);
                m = member_add(name, type, ext, is_union ? 0 : size,
                               bc ? SQ_CONST : 0);
                if (is_union) {
                    if (bytes > size)
                        size = bytes;
                } else {
                    size += bytes;
                }
            }
            if (last >= 0)
                member_link(last, m);
            else
                first = m;
            last = m;
            if (flexible && !(tok == TK_SEMI && lex_rbrace_follows()))
                acc_error_at(mline, "an array with no size has to be the last "
                                    "member");
          placed:
            if (size > 0x7fffff)
                acc_error_at(mline, "a struct this large does not fit in "
                                    "memory");
            if (!accept(TK_COMMA))
                break;
        }
        expect(TK_SEMI, "';'");
    }
    if (bit)
        size++;
    if (first < 0)
        acc_error_at(line, "a struct or union needs at least one member");
    ext_record_done(x, first, size);
    if (has_bits)
        ext_set_bits(x);
    if (flexible)
        ext_set_flex(x);
}

/* `struct` or `union`, and a tag, a list of members or both. A tag with no
 * members refers to the record already declared by that name, or declares
 * one whose members come later -- enough for a pointer to it, which is how
 * a record points at another of its own kind. */
__attribute__((noinline))
static Type struct_specifier(void)
{
    int line = tok_line, is_union = (tok == TK_KW_UNION), kind, sym, x;
    NameRef name = NAME_NONE, tag = NAME_NONE;

    kind = is_union ? TAG_UNION : TAG_STRUCT;
    next();
    if (tok == TK_IDENT) {
        name = tok_name;
        tag = tag_name(name);
        next();
    }

    if (tag) {
        sym = tag_find(tag, kind, name, line);
        if (sym != SYM_NONE && tok == TK_LBRACE
            && !sym_declared_in(sym, scope_mark))
            sym = SYM_NONE;                 /* a new one, shadowing it */
        if (sym == SYM_NONE) {
            x = ext_record(is_union, tag);
            sym = push_here(tag, SYM_TAG, kind);
            sym_at(sym)->type = TY_STRUCT;
            sym_at(sym)->ext = (unsigned char) x;
        }
        x = sym_at(sym)->ext;
    } else {
        if (tok != TK_LBRACE)
            acc_error_at(line, "'%s' needs a name or a list of members",
                         is_union ? "union" : "struct");
        x = ext_record(is_union, NAME_NONE);
    }

    if (accept(TK_LBRACE)) {
        if (ext_complete(x))
            acc_error_at(line, "'%s' is defined twice", record_name(x));
        record_members(x, is_union, line);
        expect(TK_RBRACE, "'}'");
    }
    base_ext = x;

    return TY_STRUCT;
}

/* The keywords at the front of a declaration, which say what the type is
 * before any star has narrowed it down. `int *p, q;` has one of these and two
 * declarators, and q is an int: a star belongs to the name it is written next
 * to and not to the type at the head of the line.
 *
 * base_ext is the type's extension. It is a global rather than a second
 * result because nearly every caller wants only the type -- and it is
 * overwritten by the next type read, which may be inside the declaration
 * (a cast in an initialiser), so a caller that wants it copies it at once. */

static Type base_type(void);

__attribute__((noinline))
static Type base_type_other(void)
{
    int line = tok_line;

    switch (tok) {
    case TK_KW_CONST:                   /* `const int`, `const struct s` */
    case TK_KW_VOLATILE:
    case TK_KW_RESTRICT: {
        Type t;
        unsigned char was_const;

        qualifiers();
        was_const = base_const;         /* base_type starts it afresh */
        t = base_type();
        base_const |= was_const;

        return t;
    }
    case TK_KW_ENUM: {
        Type t = enum_specifier();

        qualifiers();

        return t;
    }
    case TK_KW_STRUCT:
    case TK_KW_UNION: {
        Type t = struct_specifier();

        qualifiers();

        return t;
    }
    case TK_IDENT: {
        int sym = sym_find(tok_name);

        if (sym == SYM_NONE || sym_at(sym)->kind != SYM_TYPEDEF)
            break;
        next();
        base_ext = sym_at(sym)->ext;
        base_const = sym_at(sym)->quals & SQ_CONST;
        qualifiers();

        return sym_at(sym)->type;
    }
    case TK_KW_RESERVED:
        reserved_word();
    }
    acc_error_at(line, "expected a type, found %s", tok_spelling(tok));
}

static Type base_type(void)
{
    base_ext = 0;
    base_const = 0;
    if (!starts_type(tok))
        return base_type_other();
    return type_specifier();            /* the keywords, which is most */
}

/* Whether the current token begins a declaration: a specifier keyword, or
 * one that introduces a tagged type or a typedef. */
/* A name is in it too, once the program has declared a typedef: a name can
 * only be told to be one by looking it up, and a program with none -- most
 * of them -- is spared the lookup at the start of every statement, since
 * until then the table says a name never begins one. */
static unsigned char decl_start[TK_COUNT] = {
    [TK_KW_VOID] = 1, [TK_KW_CHAR] = 1, [TK_KW_SHORT] = 1, [TK_KW_INT] = 1,
    [TK_KW_SIGNED] = 1, [TK_KW_UNSIGNED] = 1, [TK_KW_LONG] = 1,
    [TK_KW_FLOAT] = 1, [TK_KW_DOUBLE] = 1,
    [TK_KW_ENUM] = 1, [TK_KW_STRUCT] = 1, [TK_KW_UNION] = 1,
    [TK_KW_TYPEDEF] = 1, [TK_KW_STATIC] = 1, [TK_KW_EXTERN] = 1,
    [TK_KW_AUTO] = 1, [TK_KW_REGISTER] = 1, [TK_KW_CONST] = 1,
    [TK_KW_VOLATILE] = 1, [TK_KW_INLINE] = 1, [TK_KW_BOOL] = 1,
    [TK_KW_VA_LIST] = 1
};

__attribute__((noinline))
static int is_typedef_name(NameRef name)
{
    int sym = sym_find(name);

    return sym != SYM_NONE && sym_at(sym)->kind == SYM_TYPEDEF;
}

/* Inlined: it is asked before every statement in the program, and as a call
 * it cost more than the table lookup it makes. */
static inline __attribute__((always_inline))
int starts_decl(void)
{
    if (!decl_start[tok])
        return 0;

    return tok != TK_IDENT || is_typedef_name(tok_name);
}

/* Whether the last star read was followed by `const`: `int *const p`, which
 * makes p itself const, where `const int *p` does not. */
static unsigned char stars_const;

__attribute__((noinline))
static unsigned char star_qualifiers(void)
{
    unsigned char is_const = 0;

    while (tok_qualifier()) {
        if (tok == TK_KW_CONST)
            is_const = 1;
        next();
    }

    return is_const;
}

/* The stars in front of one name. Inlined, as not_void is: they were one
 * function before the split, and as two calls each they cost 1% of a compile
 * of a program that declares a lot of names. */
static inline __attribute__((always_inline))
Type declarator_stars(Type base)
{
    int line = tok_line;

    stars_const = 0;
    while (accept(TK_STAR)) {
        if (type_ptr_depth(base) == TY_PTR_MAX)
            acc_error_at(line, "a pointer can be %d deep and this is deeper",
                         TY_PTR_MAX);
        base = type_ptr_to(base);
        stars_const = tok_qualifier() ? star_qualifiers() : 0;
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

/* Whether the array just read has a length the program works out: its
 * value is on the value stack, waiting for the declaration to take the room
 * for it. */
static int vla_length;

/* Where the stack was when the block being compiled started taking room for
 * arrays whose lengths it works out, or -1 when it has taken none.
 *
 * The room goes back at the end of the block, so that a loop whose body
 * declares one does not take it again on every turn, and before a break or
 * a continue, which leave the block without reaching its end. A return
 * needs nothing: the epilogue puts the stack back where the frame says.
 *
 * A goto out of such a block is the one way left that does not give the
 * room back. It is not wrong -- nothing is overwritten, and the function's
 * return frees it -- but a goto in a loop can take the room again on every
 * turn, and acc does not stop it.
 *
 * One mark a block, taken before the first of its arrays: everything after
 * it goes back at once. */
static int vla_mark = -1;

/* The block's mark, made if this is the first array in it to need one. */
static void block_vla_mark(void)
{
    if (vla_mark >= 0)
        return;
    vla_mark = gen_local(ACC_PTR_SIZE);
    gen_stack_mark(vla_mark);
}

/* Whether a parameter list is being read, which is the only place C99 lets
 * `static` and the qualifiers stand inside an array's brackets. */
static int in_params;

/* An array's size from inside its brackets: the number if it is a constant,
 * and -1 with the length left on the value stack if it is not, which is
 * C99's variable-length array.
 *
 * A length that is not a constant is only an array inside a function. At
 * file scope there is nothing to work it out with, and in a parameter it
 * says nothing -- the parameter is a pointer either way -- so there the
 * expression is read for its syntax and thrown away with the code it
 * emitted. */
static int array_size(const char *what, int line, int *variable)
{
    GenMark mark;
    int before = out_here();
    Type outer = narrow_dest;
    int val;
    Type type;

    *variable = 0;
    gen_mark(&mark);
    narrow_dest = 0;
    binary(PREC_LOWEST);
    narrow_dest = outer;

    if (vconst_top(&val, &type) && out_here() == before
        && !type_pointer(type) && !type_float(type)) {
        vdrop();

        return val;
    }
    if (!in_body || in_params) {
        gen_rollback(&mark);
        if (in_params)
            return -1;                  /* `int a[n]` is `int *a` */
        acc_error_at(line, "%s has to be a constant integer", what);
    }
    if (type_pointer(vtype()) || type_float(vtype()))
        acc_error_at(line, "an array's length has to be an integer");
    vconvert(TY_INT);
    *variable = 1;

    return -1;
}

/* What may follow a `[` in a parameter: `int a[static 3]`, which promises
 * the caller passes at least three, and qualifiers, which belong to the
 * pointer the parameter becomes -- `char s[const]` is `char *const s`.
 * Both are promises to the compiler rather than a change of type, and acc
 * keeps neither: the parameter is the pointer it always was, and the
 * promise is the program's to keep.
 *
 * Anywhere else they are a constraint violation, and someone who writes
 * `int a[static 3]` for a local has said something that does not mean what
 * they think, so it is refused rather than ignored. */
static void array_brackets(int line)
{
    int had_static = 0, had_qualifier = 0;

    for (;;) {
        if (tok == TK_KW_STATIC) {
            if (had_static)
                acc_error_at(line, "'static' twice inside []");
            had_static = 1;
        } else if (tok_qualifier()) {
            had_qualifier = 1;
        } else {
            break;
        }
        next();
    }
    if (!had_static && !had_qualifier)
        return;
    if (!in_params)
        acc_error_at(line, "'static' and qualifiers inside [] say something "
                           "about a parameter, and this is not one");
    if (had_static && tok == TK_RBRACKET)
        acc_error_at(line, "'static' inside [] needs the number the caller "
                           "passes at least");
}

/* The dimensions after a name being declared: `[N]`, `[]`, `[N][M]` and so
 * on. Returns how many there were. When there were any, *count is the number
 * of elements -- -1 for `[]`, which only the first may be -- and *elem their
 * type, which for an array of arrays is itself an array type: `int m[3][4]`
 * is three elements of int[4]. */
static int array_dims(Type base, int base_x, Type *elem, int *elem_x,
                      int *count)
{
    int dims[8], n = 0, i;

    vla_length = 0;
    while (tok == TK_LBRACKET) {
        int line = tok_line, d = -1;

        next();
        array_brackets(line);
        if (n == 8)
            acc_error_at(line, "an array may have at most 8 dimensions");
        if (tok != TK_RBRACKET) {
            int variable;

            d = array_size("an array's size", line, &variable);
            if (variable) {
                /* Only the first: the ones after it are how far an element
                 * steps, and a step the compiler cannot work out is a
                 * multiply on every subscript. */
                if (n > 0)
                    acc_error_at(line, "only an array's first dimension may "
                                       "be worked out as it runs");
                vla_length = 1;
            } else if (d < 0) {
                /* A parameter's `[n]`, which array_size threw away: the
                 * parameter is a pointer, as `[]` would have made it. */
                if (n > 0)
                    acc_error_at(line, "only an array's first dimension may "
                                       "be left out");
            } else if (d == 0) {
                acc_error_at(line, "an array needs at least one element");
            }
        } else if (n > 0) {
            acc_error_at(line, "only an array's first dimension may be left "
                               "out");
        }
        expect(TK_RBRACKET, "']'");
        dims[n++] = d;
    }
    if (n == 0)
        return 0;

    if (base == TY_VOID)
        acc_error_at(tok_line, "an array of void has no elements to hold");
    if (type_is_struct(base)) {
        record_complete(base_x, tok_line);
        if (ext_has_flex(base_x))
            acc_error_at(tok_line, "'%s' ends in an array with no size, so "
                                   "there is no saying where the next element "
                                   "would start", record_name(base_x));
    }
    *elem = base;
    *elem_x = base_x;
    for (i = n - 1; i >= 1; i--) {
        *elem_x = ext_array(*elem, *elem_x, dims[i]);
        *elem = TY_EXT;
    }
    *count = dims[0];
    if (*count > 0 && (long) *count * type_bytes(*elem, *elem_x) > 0x7fffff)
        acc_error_at(tok_line, "an array this large does not fit in memory");

    return n;
}

/* Whether a declarator may leave its name out, as a prototype's parameter
 * may: `int f(char *, int [4])`. */
static int abstract_ok;

static NameRef declared_name(void)
{
    NameRef name;

    if (tok != TK_IDENT) {
        if (abstract_ok && (tok == TK_COMMA || tok == TK_RPAREN
                            || tok == TK_LBRACKET))
            return NAME_NONE;
        acc_error_at(tok_line, "expected a name, found %s", tok_spelling(tok));
    }
    name = tok_name;
    next();

    return name;
}

/* ------------------------------------------------------------------ */
/* type names, casts and sizeof                                        */

/* A type with no name in it, as a cast and sizeof take one: `int`, `char *`,
 * `int [4]`, `int (*)[4]`. *x is its extension. */
/* A type name, with how many elements it has if it is an array: -1 for
 * `[]`, which only a compound literal may write, since its initialiser is
 * there to say. `*elem` and `*elem_x` are the element's type for an array
 * and the whole type otherwise. */
static Type type_name_elem(int *x, int *count, Type *elem, int *elem_x)
{
    Type t = declarator_stars(base_type()), type;
    int tx = base_ext, saved = abstract_ok;

    abstract_ok = 1;
    if (tok == TK_IDENT)
        acc_error_at(tok_line, "a type name has no name in it, and this has "
                               "'%s'", name_text(tok_name));
    direct_declarator_out(t, tx, &type, x, count);
    abstract_ok = saved;
    *elem = type;
    *elem_x = *x;
    if (*count) {
        if (*count > 0)
            *x = ext_array(type, *x, *count);

        return TY_EXT;
    }

    return type;
}

static Type type_name(int *x)
{
    Type elem;
    int count, elem_x;
    Type type = type_name_elem(x, &count, &elem, &elem_x);

    if (count < 0)
        acc_error_at(tok_line, "an array type here needs its size");

    return type;
}

/* A compound literal: `(struct s){ 1, 2 }`, `(int[]){ 1, 2, 3 }`,
 * `(int){ 5 }`, with the type read and the `{` next.
 *
 * C99 makes it an unnamed object of that type, taking the initialiser a
 * declaration of it would take. In a function it is a local without a name:
 * it lives in the frame, it can be assigned to, and it goes when the block
 * does. At file scope its bytes go into the image where a global's do.
 *
 * `count` is what the type name said about its length: 0 when it is not an
 * array, -1 for `[]`, whose length the values give. `address` asks for the
 * object's address rather than its value, which is what `&` in front of one
 * wants.
 */
__attribute__((noinline))
static void compound_literal(Type type, int x, int count, Type elem,
                             int elem_x, int line, int address, int *countp)
{
    int array;

    /* At file scope there is no frame to put it in, so it is static, as C99
     * says: its bytes are built the way a global's are and left in the
     * image, and what the literal comes to is where they went. That half is
     * further down, where the machinery for a global's bytes is. */
    if (!in_body) {
        literal_bytes(type, x, count, elem, elem_x, line, address, countp);

        return;
    }

    if (count) {
        array = local_array_object(elem, elem_x, &count, line, 1);
        vaddr_array(array, elem);       /* an array is its first element's
                                         * address, address or not */
        vset_ext(elem_x);
        if (countp)
            *countp = count;

        return;
    }

    if (type_is_struct(type)) {
        array = local_struct_object(x, line, 1);
        vaddr_array(array, TY_STRUCT);
        vset_ext(x);
        if (!address)
            vderef();

        return;
    }

    /* A scalar, which C99 allows and which is a local of its own. */
    {
        int off = gen_local(type_scalar_bytes(type));
        Type outer = narrow_dest;

        expect(TK_LBRACE, "'{'");
        if (tok == TK_RBRACE)
            acc_error_at(line, "a compound literal needs a value");
        narrow_dest = type_narrow(type);
        expr();
        narrow_dest = outer;
        vstore_local(off, type);
        vdrop();
        gen_stmt_end();
        accept(TK_COMMA);
        expect(TK_RBRACE, "'}'");
        if (address)
            vaddr_local(off, type);
        else
            vpush_local(off, type);
    }
}

/* `&(type){ values }`: the object the literal makes, and its address. The
 * `&` is read, and the `(` is next. */
__attribute__((noinline))
static void address_of_literal(int line)
{
    int x, count, elem_x;
    Type elem, type;

    next();
    if (!starts_decl())
        acc_error_at(line, "'&' takes the address of a variable, and this is "
                           "an expression");
    type = type_name_elem(&x, &count, &elem, &elem_x);
    expect(TK_RPAREN, "')'");
    if (tok != TK_LBRACE)
        acc_error_at(line, "'&' takes the address of a variable, and a cast "
                           "has none");
    compound_literal(type, x, count, elem, elem_x, line, 1, NULL);
}

/* `(type) operand` and `(type){ values }`, from just past the parenthesis:
 * a cast, or a compound literal, which the brace after the `)` tells
 * apart. A cast binds as the unary operators do, so its operand is one of
 * those or a postfix expression, which is what primary() reads. */
__attribute__((noinline))
static void cast_rest(void)
{
    int x, line = tok_line, count, elem_x;
    Type elem;
    Type to = type_name_elem(&x, &count, &elem, &elem_x), outer = narrow_dest;
    int quals = base_const ? VQ_CONST : 0;

    expect(TK_RPAREN, "')'");
    if (tok == TK_LBRACE) {
        narrow_dest = 0;
        compound_literal(to, x, count, elem, elem_x, line, 0, NULL);
        narrow_dest = outer;
        if (tok_postfix())
            subscript_value();

        return;
    }
    if (count < 0)
        acc_error_at(line, "an array type here needs its size");
    if (type_is_array(to))
        acc_error_at(line, "a cast cannot make an array");
    cast_operand(to, x, quals);
}

/* The operand of a cast whose type has been read, and the conversion. */
static void cast_operand(Type to, int x, int quals)
{
    Type outer = narrow_dest;

    narrow_dest = 0;
    primary();
    narrow_dest = outer;
    vcast(to, x, quals);
}

/* sizeof's operand, which is parsed as any expression is -- code and all --
 * to learn its type, and then taken back. What it leaves is either an
 * object, as its address, or a value.
 *
 * The difference is arrays. An array is the address of its first element
 * wherever it is used, and parsed as a value it would have the size of a
 * pointer; `sizeof a` wants all of it. So a name, a subscript and a `*` are
 * left as the object they designate -- the address and its type, which for
 * an array is a pointer to the whole array -- and the size is of what that
 * points at. */
enum { SIZEOF_OBJECT, SIZEOF_VALUE };

/* Whether sizeof was asked about an array whose length the program worked
 * out, and where its size is: not a number the compiler has, but a value in
 * the frame beside it. A frame offset is negative, so the flag is its own. */
static int sizeof_vla;
static int sizeof_vla_slot;

static int sizeof_unary(void);

/* The subscripts after an operand, each of which designates an element, and
 * any `++` or `--`, which do not change the type. */
static int sizeof_postfix(int what)
{
    if (tok_postfix() || tok == TK_LPAREN)
        what = postfix_chain(what == SIZEOF_OBJECT ? POST_OBJECT : POST_VALUE)
               == POST_OBJECT ? SIZEOF_OBJECT : SIZEOF_VALUE;
    while (tok == TK_INC || tok == TK_DEC)
        next();

    return what;
}

/* The rest of a parenthesis sizeof's operand opens, from just past it. */
static int sizeof_paren(void)
{
    Type outer = narrow_dest;
    int what;

    narrow_dest = 0;
    what = sizeof_unary();
    if (tok != TK_RPAREN) {
        if (what == SIZEOF_OBJECT)
            vderef();
        binary_rest(PREC_LOWEST);
        if (tok == TK_QUESTION)
            conditional_rest();
        what = SIZEOF_VALUE;
    }
    narrow_dest = outer;
    expect(TK_RPAREN, "')'");

    return sizeof_postfix(what);
}

static int sizeof_unary(void)
{
    if (accept(TK_STAR)) {
        if (sizeof_unary() == SIZEOF_OBJECT)
            vderef();
        if (!type_pointer(vtype()))
            acc_error_at(tok_line, "'*' takes a pointer, and this is %s",
                         type_float(vtype()) ? "a floating-point value"
                                             : "an integer");

        return sizeof_postfix(SIZEOF_OBJECT);
    }

    if (tok == TK_IDENT) {
        NameRef name = tok_name;
        int sym;

        next();
        if (accept(TK_LPAREN)) {
            call_rest(name);

            return sizeof_postfix(SIZEOF_VALUE);
        }
        sym = sym_find_value(name);
        if (sym != SYM_NONE && sym_at(sym)->kind == SYM_FUNC)
            acc_error_at(tok_line, "'%s' is a function, which has no size",
                         name_text(name));

        /* `sizeof a` where a's length was worked out as the program ran:
         * the answer is in the frame beside it. A subscript or a member
         * after it is an ordinary size again. */
        if (sym != SYM_NONE && sym_at(sym)->kind == SYM_LOCAL_VLA
            && !tok_postfix()) {
            sizeof_vla = 1;
            sizeof_vla_slot = sym_count(sym);
        }

        switch (name_operand(sym, name)) {
        case NAME_CONST:
            return SIZEOF_VALUE;
        case NAME_READONLY:
            vdrop();
            readonly_address(sym);

            return sizeof_postfix(SIZEOF_OBJECT);
        case NAME_LOCAL: {
            const Sym *local = sym_at(sym);

            vaddr_local(local->val, local->type);
            vset_ext(local->ext);
            break;
        }
        case NAME_VALUE: {
            const Sym *array = sym_at(sym);

            if (sizeof_vla)
                return SIZEOF_VALUE;    /* the size is read, not counted */
            if (sym_count(sym) < 0)
                acc_error_at(tok_line, "'%s' has no size yet",
                             name_text(name));
            vset_type(type_ptr_to(TY_EXT),
                      ext_array(array->type, array->ext, sym_count(sym)));
            break;
        }
        }

        return sizeof_postfix(SIZEOF_OBJECT);
    }

    if (tok == TK_STRING) {
        int len = string_gather();

        vpush_const(0, type_ptr_to(TY_EXT));
        vset_ext(ext_array(TY_CHAR, 0, len + 1));

        return sizeof_postfix(SIZEOF_OBJECT);
    }

    if (accept(TK_LPAREN)) {
        if (starts_decl()) {
            int x, count, elem_x, line = tok_line;
            Type elem, type = type_name_elem(&x, &count, &elem, &elem_x);

            expect(TK_RPAREN, "')'");

            /* A compound literal, whose size is the object's and not the
             * pointer an array one comes to as a value. */
            if (tok == TK_LBRACE) {
                compound_literal(type, x, count, elem, elem_x, line, 0,
                                 &count);
                if (count) {
                    vset_type(type_ptr_to(TY_EXT),
                              ext_array(elem, elem_x, count));

                    return sizeof_postfix(SIZEOF_OBJECT);
                }

                return sizeof_postfix(SIZEOF_VALUE);
            }
            if (count < 0)
                acc_error_at(line, "an array type here needs its size");
            if (type_is_array(type))
                acc_error_at(line, "a cast cannot make an array");
            cast_operand(type, x, base_const ? VQ_CONST : 0);

            return SIZEOF_VALUE;
        }

        return sizeof_paren();
    }

    primary();

    return SIZEOF_VALUE;
}

/* How far one argument of a type is from the next: whole three-byte slots,
 * two for a long or a float, and as many as a struct fills -- as the call
 * pushed them. A char or a short came as an int. */
static int va_slot(Type type, int ext)
{
    if (type_is_struct(type))
        return (ext_bytes(ext) + ACC_INT_SIZE - 1) / ACC_INT_SIZE * ACC_INT_SIZE;

    if (type_eight(type))
        return 3 * ACC_INT_SIZE;

    return type_wide(type) ? 2 * ACC_INT_SIZE : ACC_INT_SIZE;
}

/* The address of the va_list variable a form names: a local, a parameter,
 * or a global, by its name. */
static void va_list_address(void)
{
    int sym;
    const Sym *v;

    if (tok != TK_IDENT)
        acc_error_at(tok_line, "expected the va_list's name, found %s",
                     tok_spelling(tok));
    sym = sym_find(tok_name);
    if (sym == SYM_NONE)
        acc_error_at(tok_line, "'%s' is not declared", name_text(tok_name));
    v = sym_at(sym);
    if (v->type != type_ptr_to(TY_CHAR)
        || (v->kind != SYM_LOCAL && v->kind != SYM_GLOBAL))
        acc_error_at(tok_line, "'%s' is not a va_list", name_text(tok_name));
    if (v->kind == SYM_LOCAL)
        vaddr_local(v->val, v->type);
    else
        global_address(sym);
    next();
}

/* What <stdarg.h> would give, as the language's own words. A va_list is a
 * char * walked along the argument slots above the frame: va_start points
 * it just past the last named parameter's, and va_arg reads a value of the
 * type it is given there and steps it over the slots that value took.
 * va_end has nothing to undo, and va_copy is an assignment. */
__attribute__((noinline))
static void va_form(void)
{
    int form = tok, line = tok_line;

    next();
    expect(TK_LPAREN, "'('");
    va_list_address();

    switch (form) {
    case TK_KW_VA_START: {
        const Sym *last;
        int sym;

        expect(TK_COMMA, "','");
        if (current_fn == SYM_NONE || !(sym_flags(current_fn) & SYMF_VARIADIC))
            acc_error_at(line, "va_start in a function with no '...'");
        sym = tok == TK_IDENT ? sym_find(tok_name) : SYM_NONE;
        last = sym == SYM_NONE ? NULL : sym_at(sym);
        if (!last || (last->kind != SYM_LOCAL && last->kind != SYM_LOCAL_CONST)
            || last->val < 2 * ACC_PTR_SIZE)
            acc_error_at(tok_line, "va_start needs the function's last named "
                                   "parameter");
        vaddr_local(last->val + va_slot(last->type, last->ext), TY_CHAR);
        next();
        vstore_indirect();
        break;
    }
    case TK_KW_VA_ARG: {
        int x, slot;
        Type type;

        expect(TK_COMMA, "','");
        if (!starts_decl())
            acc_error_at(tok_line, "va_arg needs a type, found %s",
                         tok_spelling(tok));
        type = type_name(&x);
        slot = va_slot(type, x);

        /* ap = ap + slot, and the value at ap - slot. */
        vdup();
        vderef();
        vpush_const(slot, TY_INT);
        vapply(TK_PLUS, 0);
        vstore_indirect();
        vpush_const(slot, TY_INT);
        vapply(TK_MINUS, 0);
        vset_type(type_ptr_to(type), x);
        vderef();
        expect(TK_RPAREN, "')'");

        return;
    }
    case TK_KW_VA_COPY:
        expect(TK_COMMA, "','");
        expr();
        vstore_indirect();
        break;
    default:                            /* va_end */
        vdrop();
        vpush_const(0, TY_INT);
        break;
    }
    expect(TK_RPAREN, "')'");
    vcast(TY_VOID, 0, 0);               /* they come to nothing */
}

/* `sizeof operand` and `sizeof (type)`, as a constant of type size_t, which
 * is unsigned int here as it is in agondev. */
__attribute__((noinline))
static void sizeof_value(void)
{
    int line = tok_line, paren, x;
    Type type;

    next();
    sizeof_vla = 0;
    paren = tok == TK_LPAREN;      /* as in global_array, and for its reason */
    if (paren)
        next();
    if (paren && starts_decl()) {
        type = type_name(&x);
        expect(TK_RPAREN, "')'");
    } else {
        GenMark mark;
        int what;

        gen_mark(&mark);
        what = paren ? sizeof_paren() : sizeof_unary();
        type = vtype();
        x = vext();
        if (what == SIZEOF_OBJECT && vbits())
            acc_error_at(line, "a bit-field has no size of its own");
        if (what == SIZEOF_OBJECT)
            type = type_deref(type);
        gen_rollback(&mark);

        /* An array whose length the program worked out: its size is the
         * value the declaration put beside it. */
        if (sizeof_vla) {
            vpush_local(sizeof_vla_slot, TY_UINT);

            return;
        }
    }

    if (type == TY_VOID)
        acc_error_at(line, "'void' has no size");
    vpush_const(type_bytes(type, x), TY_UINT);
}

/* What follows a declarator's stars: a name and its dimensions, or `(*name)`
 * and the dimensions of the array it points at. Returns the name; *type is
 * its type -- an array's element type, when it is one -- and *count its
 * number of elements, -1 for `[]` and 0 when it is not an array.
 *
 * The parentheses are there for one thing only, which is a pointer to a
 * whole array: `int (*p)[4]` is one pointer, stepping four ints at a time,
 * where `int *p[4]` is four pointers. Functions are not declared this way,
 * since acc has no pointers to them. */
/* A declarator with parentheses in it, the general case: `int (*fp)(int)`,
 * `char (*table[4])[8]`, `int (*get(void))(int)`. What it says is a list of
 * derivations -- pointer to, array of, function returning -- to apply to
 * the type at its head, and C's grammar gives them inside out: the stars
 * in front of a name first, then what follows it, then whatever encloses
 * it. So each level is read into a list, in that order, and the list is
 * applied once the whole declarator is read. */
enum { DECL_PTR, DECL_ARRAY, DECL_FUNC };

typedef struct {
    unsigned char op;
    int a, b, c;        /* an array's count; a function's parameter run,
                         * its length, and whether it was given */
} DeclOp;

static DeclOp decl_ops[32];
static int    ndecl_ops;

static void decl_push(int op, int a, int b, int c)
{
    if (ndecl_ops == 32)
        acc_error_at(tok_line, "a declarator this deep is more than acc takes");
    decl_ops[ndecl_ops].op = (unsigned char) op;
    decl_ops[ndecl_ops].a = a;
    decl_ops[ndecl_ops].b = b;
    decl_ops[ndecl_ops].c = c;
    ndecl_ops++;
}

static void decl_apply(const DeclOp *op, Type *t, int *x)
{
    switch (op->op) {
    case DECL_PTR:
        if (type_ptr_depth(*t) == TY_PTR_MAX)
            acc_error_at(tok_line, "a pointer can be %d deep and this is "
                                   "deeper", TY_PTR_MAX);
        *t = type_ptr_to(*t);
        break;
    case DECL_ARRAY:
        if (op->a < 0)
            acc_error_at(tok_line, "only an array's first dimension may be "
                                   "left out");
        if (type_is_func(*t))
            acc_error_at(tok_line, "an array of functions is not a thing C "
                                   "has; an array of pointers to them is");
        if (type_is_struct(*t))
            record_complete(*x, tok_line);
        *x = ext_array(*t, *x, op->a);
        *t = TY_EXT;
        break;
    case DECL_FUNC:
        if (type_is_array(*t) || type_is_func(*t))
            acc_error_at(tok_line, "a function cannot return an %s",
                         type_is_array(*t) ? "array" : "function");
        *x = ext_func(*t, *x, op->a, op->b, op->c);
        *t = TY_FUNC;
        break;
    }
}

static int  param_types(int *first, int *count);

/* The names of the parameters param_types read, by their place in the
 * parameter table: a function declared as `int (*get(int a))(int)` and
 * then defined has only these to name its parameters by. */
static NameRef *param_names;
static int      param_names_cap;

static void param_name_set(int at, NameRef name)
{
    if (at >= param_names_cap) {
        int cap = param_names_cap ? param_names_cap : 64;

        while (cap <= at)
            cap *= 2;
        param_names = realloc(param_names, (size_t) cap * sizeof *param_names);
        if (!param_names)
            acc_error("out of memory for parameters");
        param_names_cap = cap;
    }
    param_names[at] = name;
}
static NameRef decl_full(void);

/* A declarator's direct part, from a `(`, a name or nothing: the
 * parenthesised declarator inside, or the name, and the dimensions and
 * parameter lists after it -- its derivations appended in the order they
 * apply. */
static NameRef decl_direct(void)
{
    NameRef name = NAME_NONE;
    int inner = -1, inner_end = 0, suffix, i, j;

    if (tok == TK_LPAREN) {
        /* A parenthesised declarator, or -- in a type name -- the parameter
         * list of a function type with no name at all. */
        next();
        if (tok == TK_STAR || tok == TK_LPAREN || tok == TK_LBRACKET
            || (tok == TK_IDENT && !is_typedef_name(tok_name))) {
            inner = ndecl_ops;
            name = decl_full();
            inner_end = ndecl_ops;
            expect(TK_RPAREN, "')'");
        } else {
            int first, count, given = param_types(&first, &count);

            decl_push(DECL_FUNC, first, count, given);
        }
    } else if (tok == TK_IDENT) {
        name = tok_name;
        next();
    } else if (!abstract_ok) {
        acc_error_at(tok_line, "expected a name, found %s", tok_spelling(tok));
    }

    suffix = ndecl_ops;
    for (;;) {
        if (accept(TK_LBRACKET)) {
            int n = -1, line = tok_line;

            array_brackets(line);
            if (tok != TK_RBRACKET) {
                n = constant_int("an array's size", line);
                if (n <= 0)
                    acc_error_at(line, "an array needs at least one element");
            }
            expect(TK_RBRACKET, "']'");
            decl_push(DECL_ARRAY, n, 0, 0);
        } else if (accept(TK_LPAREN)) {
            int first, count, given = param_types(&first, &count);

            decl_push(DECL_FUNC, first, count, given);
        } else {
            break;
        }
    }

    /* In parse order the list is [inner..., suffixes...]; applied it has to
     * be [suffixes, last first..., inner...]. */
    if (inner >= 0 || ndecl_ops - suffix > 1) {
        DeclOp tmp[32];
        int n = 0;

        for (j = ndecl_ops - 1; j >= suffix; j--)
            tmp[n++] = decl_ops[j];
        for (i = inner < 0 ? suffix : inner; i < (inner < 0 ? suffix : inner_end); i++)
            tmp[n++] = decl_ops[i];
        memcpy(&decl_ops[inner < 0 ? suffix : inner], tmp, n * sizeof *tmp);
    }

    return name;
}

/* A whole declarator: its stars, which apply first, then its direct part. */
static NameRef decl_full(void)
{
    int stars = 0;

    while (accept(TK_STAR)) {
        stars++;
        if (tok_qualifier())
            star_qualifiers();
    }
    while (stars--)
        decl_push(DECL_PTR, 0, 0, 0);

    return decl_direct();
}

/* The parameter types of a function type in a declarator, from just past
 * its `(`: added to the parameter table as a run, with their names, if they
 * have them, read and put aside. Returns whether the parameters were given:
 * `()` does not give them. */
static int param_types(int *first, int *count)
{
    int saved_abstract = abstract_ok, saved_params = in_params, variadic = 0;

    *first = sym_params_begin();
    *count = 0;
    if (accept(TK_RPAREN))
        return 0;
    if (tok == TK_KW_VOID && lex_rparen_follows()) {
        next();
        expect(TK_RPAREN, "')'");

        return 1;
    }
    abstract_ok = 1;
    in_params = 1;
    for (;;) {
        Type base, type;
        int bx, ext, n;
        NameRef pname;

        accept(TK_KW_REGISTER);
        base = base_type();
        bx = base_ext;
        pname = direct_declarator_out(declarator_stars_out(base), bx, &type,
                                      &ext, &n);
        if (!n)
            func_suffix(&type, &ext);
        if (n || type_is_func(type)) {  /* adjusted to a pointer, as C says */
            if (type_ptr_depth(type) == TY_PTR_MAX)
                acc_error_at(tok_line, "a pointer can be %d deep and this is "
                                       "deeper", TY_PTR_MAX);
            type = type_ptr_to(type);
        }
        not_void(type, "a parameter", tok_line);
        param_name_set(*first + *count, pname);
        sym_param_add(type, ext);
        (*count)++;
        if (!accept(TK_COMMA))
            break;
        if (accept(TK_ELLIPSIS)) {
            variadic = 2;
            break;
        }
    }
    abstract_ok = saved_abstract;
    in_params = saved_params;
    expect(TK_RPAREN, "')'");

    return 1 | variadic;
}

/* A parameter list after a plain name, where it makes a function type:
 * `typedef int binary(int, int)`, a parameter `int g(int)`. Where a
 * function is being declared the caller reads the list itself. */
static void func_suffix(Type *type, int *ext)
{
    int first, count, given;

    if (!accept(TK_LPAREN))
        return;
    if (type_is_array(*type) || type_is_func(*type))
        acc_error_at(tok_line, "a function cannot return an %s",
                     type_is_array(*type) ? "array" : "function");
    given = param_types(&first, &count);
    *ext = ext_func(*type, *ext, first, count, given);
    *type = TY_FUNC;
}

__attribute__((noinline))
static NameRef paren_declarator(Type t, int tx, Type *type, int *ext,
                                int *count)
{
    NameRef name;

    if (tok == TK_LPAREN) {
        int first = ndecl_ops, i;

        name = decl_direct();
        for (i = first; i < ndecl_ops; i++) {
            DeclOp *op = &decl_ops[i];

            /* The last applied, when it is an array, makes the object an
             * array, which callers take as its element type and count. */
            if (op->op == DECL_ARRAY && op->a < 0 && i < ndecl_ops - 1
                && decl_ops[i + 1].op == DECL_PTR)
                acc_error_at(tok_line, "a pointer to an array needs the "
                                       "array's size");
            if (op->op == DECL_ARRAY && i == ndecl_ops - 1) {
                if (type_is_func(t))
                    acc_error_at(tok_line, "an array of functions is not a "
                                           "thing C has; an array of pointers "
                                           "to them is");
                ndecl_ops = first;
                *type = t;
                *ext = tx;
                *count = op->a;

                return name;
            }
            decl_apply(op, &t, &tx);
        }
        ndecl_ops = first;
        *type = t;
        *ext = tx;
        *count = 0;

        return name;
    }

    name = declared_name();
    if (!array_dims(t, tx, type, ext, count)) {
        *type = t;
        *ext = tx;
        *count = 0;
    }

    return name;
}

/* An object whose type is an array by way of a typedef -- `row r;` with
 * `typedef int row[3];` -- declared as the array it is: its element type
 * and count, as if the dimension had been written after its name. */
__attribute__((noinline))
static void typedef_array(Type *type, int *ext, int *count)
{
    int x = *ext;

    *type = ext_elem(x);
    *ext = ext_elem_x(x);
    *count = ext_count(x);
}

/* The same, with the common case -- a plain name, and nothing after it --
 * inlined into every caller: as calls, the declarator and the check for
 * dimensions were three more on every parameter and local in the program,
 * which cost 1.8% of a compile. */
static inline __attribute__((always_inline))
NameRef direct_declarator(Type t, int tx, Type *type, int *ext, int *count)
{
    NameRef name;

    vla_length = 0;             /* of this declarator, and not the last */
    if (tok != TK_IDENT) {
        name = paren_declarator(t, tx, type, ext, count);
    } else {
        name = tok_name;
        next();
        *type = t;
        *ext = tx;
        *count = 0;
        if (tok == TK_LBRACKET)
            array_dims(t, tx, type, ext, count);
    }
    if (type_is_array(*type) && !*count)
        typedef_array(type, ext, count);

    return name;
}

/* The two above as calls, for a struct's members, where they are not on the
 * path of every declaration. */
static Type declarator_stars_out(Type base)
{
    return declarator_stars(base);
}

static NameRef direct_declarator_out(Type t, int tx, Type *type, int *ext,
                                     int *count)
{
    return direct_declarator(t, tx, type, ext, count);
}

/* An array's initialiser, walked into the scalars it gives: each is handed to
 * `put` with its offset in the array, and whatever `put` does with it --
 * store it, for a local; write its bytes, for a global -- is the caller's.
 *
 * Braces nest as the array does, `{{1, 2}, {3, 4}}`, and may be left out, as
 * C allows: `{1, 2, 3, 4}` fills a 2x2 array the same way, each row taking
 * as many values as it has room for. A scalar may have braces of its own. */
/* `value` is the scalar's value when the initialiser gave it as part of a
 * string -- a byte -- and -1 when it is an expression still to be parsed. */
typedef void (*InitPut)(Type scalar, int offset, int value);

static void init_element(Type type, int x, int offset, InitPut put);
static void local_put(Type scalar, int offset, int value);
static int  local_struct_value(int x, int offset);

/* A value an initialiser has already parsed and left on the stack, for the
 * next scalar it gives to take instead of parsing one: see
 * local_struct_value. */
static int init_pending;

/* The bit-field the scalar being initialised is, 0 if it is not one. */
static int init_bits;
static void init_record(int x, int offset, InitPut put, int braced);
static int  init_string(int count, int offset, InitPut put);
static int  init_index(Type elem, int elem_x, int count, int offset,
                       InitPut put);
static int  init_member(int x, int offset, InitPut put);

/* Whether a row or a record that was taking values from the list around it
 * stopped because the next thing is a designator, which belongs to that
 * list and not to it. It has eaten the comma before the designator by then,
 * so the list is told not to look for one. */
static int init_designator_next;

/* Whether a designator starts here. A `.` is one only in front of a name:
 * `.5` is a number. */
#define tok_designator()  (tok == TK_LBRACKET \
                           || (tok == TK_DOT && lex_ident_follows()))

/* Whether `type`, an element type, is one of the three chars a string can
 * initialise an array of. */
#define type_is_char(ty)  (type_size(ty) == 1 && !type_pointer(ty) \
                           && !type_is_array(ty))

/* The elements of a braced list, the brace already read. Returns how many.
 * `count` is how many there may be, or -1 for no limit. `elem_x` is the
 * element type's extension, when it is a row. */
static int init_list(Type elem, int elem_x, int count, int offset, InitPut put,
                     int from)
{
    int n = from, most = from, step = type_bytes(elem, elem_x);

    while (tok != TK_RBRACE) {
        if (tok_designator()) {
            /* `[3] = v` puts v in element 3 and the list goes on from 4,
             * wherever it had got to. */
            n = init_index(elem, elem_x, count, offset, put) + 1;
        } else {
            if (n == count)
                acc_error_at(tok_line, "more initial values than the array has "
                                       "elements");
            init_element(elem, elem_x, offset + n * step, put);
            n++;
        }
        if (n > most)
            most = n;               /* an `[]` array is as long as its
                                     * furthest element, not its last */
        if (init_designator_next)
            init_designator_next = 0;
        else if (!accept(TK_COMMA))
            break;
    }
    expect(TK_RBRACE, "'}'");

    return most;
}

/* A row with its braces left out: it takes values from the list it is in,
 * until it is full or the list ends. The comma after its last value is left
 * for the list, unless the list ends straight after it. `x` is the row's
 * extension. */
static void init_elided(int x, int offset, InitPut put)
{
    Type elem = ext_elem(x);
    int elem_x = ext_elem_x(x);
    int count = ext_count(x), step = type_bytes(elem, elem_x), i;

    for (i = 0; i < count; i++) {
        if (i) {
            if (tok != TK_COMMA)
                return;
            next();
            if (tok == TK_RBRACE)
                return;
            if (tok_designator()) {
                init_designator_next = 1;

                return;
            }
        }
        init_element(elem, elem_x, offset + i * step, put);
    }
}

/* What a designator names, once it has been read: another designator
 * inside it -- `[2].x = 5` -- or the `=` and the value.
 *
 * C99 lets them chain as deep as the object goes, and each step is the same
 * question the list itself asks: is this an element or a member. */
static void init_designated(Type type, int x, int offset, InitPut put)
{
    if (tok_designator()) {
        if (tok == TK_LBRACKET) {
            if (!type_is_array(type))
                acc_error_at(tok_line, "'[' designates an element, and this is "
                                       "not an array");
            init_index(ext_elem(x), ext_elem_x(x), ext_count(x), offset, put);

            return;
        }
        if (!type_is_struct(type))
            acc_error_at(tok_line, "'.' designates a member, and this is not a "
                                   "struct or a union");
        init_member(x, offset, put);

        return;
    }
    expect(TK_ASSIGN, "'=' after a designator");
    init_element(type, x, offset, put);
}

/* `[3] = v` in an array's list: v goes to element 3, and 3 is returned so
 * that the list goes on from the one after it. */
static int init_index(Type elem, int elem_x, int count, int offset, InitPut put)
{
    int line = tok_line, i;

    next();                             /* the '[' */
    i = constant_int("the element a designator names", line);
    expect(TK_RBRACKET, "']'");
    if (i < 0)
        acc_error_at(line, "a designator names element %d, and there is no "
                           "such element", i);
    if (count >= 0 && i >= count)
        acc_error_at(line, "a designator names element %d of an array of %d",
                     i, count);
    init_designated(elem, elem_x, offset + i * type_bytes(elem, elem_x), put);

    return i;
}

/* `.name = v` in a record's list: the member is returned, so that the list
 * goes on with the one after it. */
static int init_member(int x, int offset, InitPut put)
{
    int line = tok_line, m, saved_bits;
    NameRef name;

    next();                             /* the '.' */
    if (tok != TK_IDENT)
        acc_error_at(tok_line, "expected a member's name after '.', found %s",
                     tok_spelling(tok));
    name = tok_name;
    next();
    record_complete(x, line);
    m = member_find(x, name);
    if (m < 0)
        acc_error_at(line, "'%s' has no member '%s'", record_name(x),
                     name_text(name));

    saved_bits = init_bits;
    init_bits = member_bits(m);
    init_designated(member_type(m), member_ext(m), offset + member_offset(m),
                    put);
    init_bits = saved_bits;

    return m;
}

static void init_element(Type type, int x, int offset, InitPut put)
{
    if (type_is_struct(type)) {
        if (accept(TK_LBRACE)) {
            init_record(x, offset, put, 1);

            return;
        }
        if (put == local_put && !init_pending && local_struct_value(x, offset))
            return;
        init_record(x, offset, put, 0);

        return;
    }
    if (type_is_array(type)) {
        if (tok == TK_STRING && !init_pending && type_is_char(ext_elem(x))) {
            init_string(ext_count(x), offset, put);

            return;
        }
        if (accept(TK_LBRACE))
            init_list(ext_elem(x), ext_elem_x(x), ext_count(x), offset, put, 0);
        else
            init_elided(x, offset, put);

        return;
    }
    if (accept(TK_LBRACE)) {
        put(type, offset, -1);
        accept(TK_COMMA);
        expect(TK_RBRACE, "'}'");

        return;
    }
    put(type, offset, -1);
}

/* A char array initialised from a string: its characters, and the
 * terminator if there is room for it -- C lets a string exactly as long as
 * the array leave it out. Returns how many elements it filled; `count` is
 * how many there are, -1 when that is for the string to say. */
static int init_string(int count, int offset, InitPut put)
{
    int len = string_gather(), i;

    if (count >= 0 && len > count)
        acc_error_at(tok_line, "this string is longer than the array it "
                               "initialises");
    for (i = 0; i < len; i++)
        put(TY_CHAR, offset + i, (unsigned char) str_joined[i]);
    if (count < 0 || len < count) {
        put(TY_CHAR, offset + len, 0);
        len++;
    }

    return len;
}

/* A struct's or a union's initialiser: its members in order, or a union's
 * first. Braced, the list is its own and ends at its brace; with the braces
 * left out it takes values from the list it is in until every member has
 * one or the list ends, as an elided row does. */
static void init_record(int x, int offset, InitPut put, int braced)
{
    int m = member_first(x), i;

    record_complete(x, tok_line);
    for (i = 0; m >= 0 || (braced && tok_designator()); i++) {
        if (braced && tok == TK_RBRACE)
            break;
        if (!braced && i) {
            if (tok != TK_COMMA)
                return;
            next();
            if (tok == TK_RBRACE)
                return;
            if (tok_designator()) {
                init_designator_next = 1;

                return;
            }
        }

        /* `.name = v` names the member itself, and the list goes on with
         * the one after it -- in a union as well, where a designator is how
         * a member other than the first is given a value. */
        if (tok_designator()) {
            m = init_member(x, offset, put);
            m = ext_is_union(x) ? -1 : member_next(m);
        } else {
            init_bits = member_bits(m);
            init_element(member_type(m), member_ext(m),
                         offset + member_offset(m), put);
            init_bits = 0;
            m = ext_is_union(x) ? -1 : member_next(m);
        }
        if (init_designator_next)
            init_designator_next = 0;
        else if (braced && !accept(TK_COMMA))
            break;
    }
    if (!braced)
        return;
    if (tok != TK_RBRACE)
        acc_error_at(tok_line, "more initial values than '%s' has members",
                     record_name(x));
    next();
}

/* The local array being initialised, and which of its scalars the
 * initialiser gave, so that the others can be zeroed after. */
static int            init_array;
static unsigned char *init_given;
static int            init_given_cap;

/* That the bytes from offset for size were given by the initialiser, so
 * the zeroing after it leaves them. */
static void init_mark(int offset, int size)
{
    int end = offset + size;

    if (size <= 0)
        return;                 /* nothing given, and nothing to grow for */

    if (end > init_given_cap) {
        int cap = init_given_cap ? init_given_cap : 64;

        while (cap < end)
            cap *= 2;
        init_given = realloc(init_given, (size_t) cap);
        if (!init_given)
            acc_error("out of memory for an initialiser");
        memset(init_given + init_given_cap, 0, (size_t) (cap - init_given_cap));
        init_given_cap = cap;
    }
    memset(init_given + offset, 1, (size_t) size);
}

static void local_put(Type scalar, int offset, int value)
{
    Type outer = narrow_dest;

    /* By the byte: a struct's members are not at multiples of their own
     * width. */
    vaddr_array(init_array, TY_CHAR);
    vmember(offset, scalar, 0, 0);     /* an initialiser writes const too */
    if (init_bits)
        vset_bits(init_bits);
    if (init_pending) {
        vswap();                        /* the address under the value */
        init_pending = 0;
    } else {
        narrow_dest = type_narrow(scalar);
        if (value >= 0)
            vpush_const(value, TY_INT);
        else
            expr();
        narrow_dest = outer;
    }
    vstore_indirect();
    vdrop();
    gen_stmt_end();
    init_mark(offset, init_bits ? bitfield_at(init_bits)->bytes
                                : type_scalar_bytes(scalar));
}

/* Zeroes the bytes of the first `total` of a local array or struct that its
 * initialiser did not give, a run at a time. */
static void zero_gaps(int array, int total)
{
    int i, run;

    for (i = 0; i < total; i = run) {
        if (i < init_given_cap && init_given[i]) {
            run = i + 1;
            continue;
        }
        for (run = i; run < total && !(run < init_given_cap && init_given[run]); run++)
            ;
        gen_zero_array(array, i, run - i);
    }
}

/* A local with bit-fields in it, zeroed before its initialiser runs: a
 * bit-field is written by reading its bytes and putting its bits in among
 * the others', and those have to be zero, not whatever the stack held. */
static void bits_zeroed(int array, int x, int bytes)
{
    if (!ext_has_bits(x))
        return;
    gen_zero_array(array, 0, bytes);
    init_mark(0, bytes);
}

/* A local struct or union, from just past its declarator: in the array area,
 * where it is reached by its address as an array is. Its initialiser is a
 * braced list, the members not given zeroed, or a struct of the same type,
 * copied. */
/* The object itself, without the name: a declaration pushes a symbol for it
 * and a compound literal leaves it unnamed. `braced` says the initialiser
 * follows straight away, as a compound literal's does, rather than after an
 * `=`. Returns which array area it is in. */
__attribute__((noinline))
static int local_struct_object(int x, int line, int braced)
{
    int array = gen_local_array();

    record_complete(x, line);
    gen_local_array_size(array, ext_bytes(x));
    if (braced || accept(TK_ASSIGN)) {
        if (braced || accept(TK_LBRACE)) {
            if (braced)
                expect(TK_LBRACE, "'{'");
            init_array = array;
            if (init_given_cap)
                memset(init_given, 0, (size_t) init_given_cap);
            bits_zeroed(array, x, ext_bytes(x));
            init_record(x, 0, local_put, 1);
            zero_gaps(array, ext_bytes(x));
        } else {
            vaddr_array(array, TY_STRUCT);
            vset_ext(x);
            expr();
            vstore_indirect();
            vdrop();
            gen_stmt_end();
        }
    }

    return array;
}

static void local_struct(int x, NameRef name, int line)
{
    int array = local_struct_object(x, line, 0);
    int sym = sym_push(name, SYM_LOCAL_STRUCT, array);

    sym_at(sym)->type = TY_STRUCT;
    sym_at(sym)->ext = (unsigned char) x;
}

/* A struct member or element of a local's initialiser, given without
 * braces. C says that an expression of the struct's own type initialises it
 * whole, and anything else is the first of its members' values with the
 * braces left out -- which only the expression's type can tell, so it is
 * parsed first. A struct is copied in, and returns 1; anything else is left
 * for the first member to take, and returns 0. */
__attribute__((noinline))
static int local_struct_value(int x, int offset)
{
    Type outer = narrow_dest;

    narrow_dest = 0;
    expr();
    narrow_dest = outer;
    if (!type_is_struct(vtype())) {
        init_pending = 1;

        return 0;
    }
    if (vext() != x)
        acc_error_at(tok_line, "a struct can only be assigned a struct of the "
                               "same type");
    vaddr_array(init_array, TY_CHAR);
    vmember(offset, TY_STRUCT, x, 0);
    vswap();
    vstore_indirect();
    vdrop();
    gen_stmt_end();
    init_mark(offset, ext_bytes(x));

    return 1;
}

/* A local array, from just past its declarator.
 *
 * The scalars an initialiser gives are stored one at a time, each at the
 * array's address plus its offset, and the ones it does not give are zeroed
 * after them, a run at a time -- which is what C says they start as, and
 * which leaves an array with no initialiser as whatever the stack held, as C
 * also says. With nested braces a row may stop short, so the gaps can be
 * anywhere, not only at the end. A `[]` array is as long as its initialiser,
 * which is only known once the brace closes; its size is given then, the
 * elements having been stored against an address that is only filled in
 * when the function ends. */
__attribute__((noinline))
static int local_array_object(Type elem, int elem_x, int *countp, int line,
                              int braced)
{
    int array = gen_local_array();
    int step = type_bytes(elem, elem_x);
    int count = *countp;
    int init = braced || accept(TK_ASSIGN);

    if (count > 0)
        gen_local_array_size(array, count * step);

    /* A string for a char array: its bytes written into the image, where a
     * string constant goes, and copied into the array with ldir, the rest
     * zeroed after. */
    if (init && tok == TK_STRING && type_is_char(elem)) {
        int len = string_gather(), from, n;

        if (count >= 0 && len > count)
            acc_error_at(line, "this string is longer than the array it "
                               "initialises");
        from = gen_data(str_joined, len);
        n = (count < 0 || len < count) ? len + 1 : len;   /* the terminator */
        if (count < 0) {
            count = n;
            gen_local_array_size(array, count);
        }
        gen_copy_to_array(array, 0, from, n);
        if (n < count)
            gen_zero_array(array, n, count - n);
    } else if (init && !type_is_array(elem) && !type_is_struct(elem)) {
        /* One dimension: the values in order, each stored as it is read, and
         * the rest zeroed after them in one run, as before arrays of
         * arrays. */
        int n = 0, designated = 0;

        expect(TK_LBRACE, "'{'");
        init_array = array;
        while (tok != TK_RBRACE) {
            /* A designator, so the rest of the list is not in order: what
             * has been given so far is marked as given -- it is the first n
             * elements, in order -- and the walk takes it from here, which
             * knows how to zero the gaps it leaves. */
            if (tok_designator()) {
                if (init_given_cap)
                    memset(init_given, 0, (size_t) init_given_cap);
                init_mark(0, n * step);
                n = init_list(elem, elem_x, count, 0, local_put, n);
                designated = 1;
                break;
            }
            if (n == count)
                acc_error_at(tok_line, "more initial values than the array has "
                                       "elements");
            init_element(elem, elem_x, n * step, local_put);
            n++;
            if (!accept(TK_COMMA))
                break;
        }
        if (!designated)
            expect(TK_RBRACE, "'}'");        /* the walk read its own */
        if (count < 0) {
            if (n == 0)
                acc_error_at(line, "an array needs at least one element");
            count = n;
            gen_local_array_size(array, count * step);
        }
        if (designated)
            zero_gaps(array, count * step);
        else if (n < count)
            gen_zero_array(array, n * step, (count - n) * step);
    } else if (init) {
        expect(TK_LBRACE, "'{'");
        init_array = array;
        if (init_given_cap)
            memset(init_given, 0, (size_t) init_given_cap);
        if (type_is_struct(elem) && count > 0)
            bits_zeroed(array, elem_x, count * step);
        {
            int n = init_list(elem, elem_x, count, 0, local_put, 0);

            if (count < 0) {
                if (n == 0)
                    acc_error_at(line, "an array needs at least one element");
                count = n;
                gen_local_array_size(array, count * step);
            }
        }

        zero_gaps(array, count * step);
    } else if (count < 0) {
        acc_error_at(line, "an array declared with [] needs initial values to "
                           "say how long it is");
    }

    *countp = count;

    return array;
}

/* `int a[n];` -- an array whose length the program works out. The room
 * comes off the stack where the declaration stands, and the name is bound
 * to a pointer to it: a local holding the address, which is what makes
 * `a[i]` the subscript it already is, and another holding the size in
 * bytes, which is what sizeof reads.
 *
 * The length is on the value stack, left there by array_size. C99 says it
 * has to be positive; a length of zero or less takes no room and leaves a
 * pointer that nothing may be read through, which is what the program asked
 * for.
 *
 * The room goes back at the end of the block, and not before: see
 * block_vla_mark. An initialiser is not allowed -- C99 says so, and there
 * would be no telling how many values to expect. */
__attribute__((noinline))
static void local_vla(Type elem, int elem_x, NameRef name, int line)
{
    int step = type_bytes(elem, elem_x);
    int ptr = gen_local(ACC_PTR_SIZE);
    int size = gen_local(ACC_INT_SIZE);
    int sym;

    not_void(elem, "an array's element", line);
    block_vla_mark();

    /* bytes = length * the element's size, and the size local holds it. */
    if (step != 1) {
        vpush_const(step, TY_INT);
        vapply(TK_STAR, 0);
    }
    vstore_local(size, TY_UINT);
    gen_stack_take(ptr);
    gen_stmt_end();

    if (tok == TK_ASSIGN)
        acc_error_at(line, "an array whose length is worked out as it runs "
                           "cannot have an initialiser");

    sym = sym_push(name, SYM_LOCAL_VLA, ptr);
    sym_at(sym)->type = type_ptr_to(elem);
    sym_at(sym)->ext = (unsigned char) elem_x;
    sym_set_count(sym, size);
}

static void local_array(Type elem, int elem_x, NameRef name, int count,
                        int line)
{
    int array = local_array_object(elem, elem_x, &count, line, 0);
    int sym = sym_push(name, SYM_LOCAL_ARRAY, array);

    sym_at(sym)->type = elem;
    sym_at(sym)->ext = (unsigned char) elem_x;
    sym_set_count(sym, count);
}

static int  function_declarator(Type ret_type, int ret_ext, NameRef name,
                                int line);
static int  function_from_type(int x, NameRef name, int line);
static void global_variable(Type type, int ext, NameRef name, int count,
                            int line);
static int  global_again(int sym, Type type, int ext, int count, int line);

/* How the variable being declared at file scope is bound: see push_global. */
static int static_local;
static int redefining = SYM_NONE;
static unsigned char decl_const;    /* the variable being declared is const */
static int decl_extern;             /* and its declaration said extern */
static unsigned char decl_bottom_const; /* and SQ_CONST, when its type is */

/* `typedef`, and names for types rather than objects: each declarator names
 * the type it would have given a variable. An array type keeps its shape in
 * an extension, so that an object declared with it is that array. */
__attribute__((noinline))
static void typedef_declaration(void)
{
    Type base;
    int bx;
    unsigned char bc;

    next();
    base = base_type();
    bx = base_ext;
    bc = base_const;
    for (;;) {
        int line = tok_line, count, ext, sym;
        Type type;
        NameRef name = direct_declarator(declarator_stars(base), bx, &type,
                                         &ext, &count);

        if (!count)
            func_suffix(&type, &ext);
        if (count < 0)
            acc_error_at(line, "a typedef of an array needs the array's size");
        if (count) {
            ext = ext_array(type, ext, count);
            type = TY_EXT;
        }
        not_redeclared(name, line);
        sym = push_here(name, SYM_TYPEDEF, 0);
        sym_at(sym)->type = type;
        sym_at(sym)->ext = (unsigned char) ext;
        sym_at(sym)->quals = bc ? SQ_CONST : 0;
        decl_start[TK_IDENT] = 1;
        if (!accept(TK_COMMA))
            break;
    }
    expect(TK_SEMI, "';'");
}

/* `extern name;` in a block: the file-scope variable of that name, declared
 * there now if it is not yet, and the block's name for it. */
static void block_extern(Type type, int ext, NameRef name, int count,
                         int line)
{
    int g = name_global(name), sym;
    const Sym *global;

    if (tok == TK_ASSIGN)
        acc_error_at(line, "an extern in a block cannot give a value");
    if (g == SYM_NONE) {
        /* Nothing of that name at file scope yet, so this declaration
         * introduces it -- and reserves nothing for it, which is what extern
         * means wherever it is said. It used to put the variable's bytes
         * here, in the middle of a function, with a jump over them. */
        decl_extern = 1;
        global_variable(type, ext, name, count, line);
        decl_extern = 0;
        g = name_global(name);
    } else {
        global_again(g, type, ext, count, line);
    }
    global = sym_at(g);
    sym = sym_push_local(name, global->kind, global->val);
    global = sym_at(g);
    sym_at(sym)->type = global->type;
    sym_at(sym)->ext = global->ext;
    sym_at(sym)->quals = global->quals;
    if (count)
        sym_set_count(sym, sym_count(g));
}

/* The qualifiers the declaration being read gives each of its variables:
 * SQ_REGISTER, for `register int x`. */
static unsigned char decl_quals;

static inline __attribute__((always_inline)) void declaration(void);

/* A declaration in a block that begins with typedef, static or extern.
 *
 * A block's static variable is a file-scope one in all but its name: its
 * bytes are in the image, written here and jumped over, initialised once
 * from constants, and kept from one call to the next. */
__attribute__((noinline))
static void storage_declaration(void)
{
    int storage = tok;
    Type base;
    int bx;
    unsigned char bc;

    if (storage == TK_KW_TYPEDEF) {
        typedef_declaration();

        return;
    }

    /* auto is what a block's variable is anyway, and register only forbids
     * taking its address: both are then an ordinary declaration. */
    if (storage == TK_KW_AUTO || storage == TK_KW_REGISTER) {
        next();
        decl_quals = storage == TK_KW_REGISTER ? SQ_REGISTER : 0;
        declaration();
        decl_quals = 0;

        return;
    }
    next();
    base = base_type();
    bx = base_ext;
    bc = base_const;
    if (accept(TK_SEMI))
        return;
    for (;;) {
        int line = tok_line, count, ext;
        Type type, stars = declarator_stars(base);
        NameRef name = direct_declarator(stars, bx, &type, &ext, &count);

        decl_const = stars != base ? stars_const : bc;
        decl_bottom_const = bc ? SQ_CONST : 0;
        if (tok == TK_LPAREN) {
            function_declarator(type, ext, name, line);
        } else if (storage == TK_KW_EXTERN) {
            not_redeclared(name, line);
            block_extern(type, ext, name, count, line);
        } else {
            int hole = gen_jump();

            not_redeclared(name, line);
            static_local = 1;
            global_variable(type, ext, name, count, line);
            static_local = 0;
            gen_label(hole);
        }
        if (!accept(TK_COMMA))
            break;
    }
    expect(TK_SEMI, "';'");
}

/* Inlined into both callers, the function body and a for's first clause:
 * it was inlined into the first when it had only that one, and as a call it
 * is one more on every declaration in the program. */
static inline __attribute__((always_inline))
void declaration(void)
{
    Type base;
    int bx;
    unsigned char bc;

    /* typedef, static, extern, auto and register, one range. */
    if ((unsigned char) (tok_low - TK_KW_TYPEDEF) < 5u) {
        storage_declaration();

        return;
    }
    base = base_type();
    bx = base_ext;
    bc = base_const;

    /* Nothing but the type: `enum e { A, B };`, declaring what is in it. */
    if (accept(TK_SEMI))
        return;

    for (;;) {
        int line = tok_line, count, ext, off, sym;
        Type type, stars = declarator_stars(base);
        NameRef name = direct_declarator(stars, bx, &type, &ext, &count);

        if (type_is_func(type)) {
            function_from_type(ext, name, line);
            if (!accept(TK_COMMA))
                break;
            continue;
        }
        if (tok == TK_LPAREN) {         /* a function declared in a block */
            decl_bottom_const = bc ? SQ_CONST : 0;
            function_declarator(type, ext, name, line);
            if (!accept(TK_COMMA))
                break;
            continue;
        }
        if (vla_length) {
            local_vla(type, ext, name, line);
            if (bc)
                sym_at(sym_find(name))->quals |= SQ_CONST;
            if (!accept(TK_COMMA))
                break;
            continue;
        }
        if (count) {
            local_array(type, ext, name, count, line);
            if (bc)
                sym_at(sym_find(name))->quals |= SQ_CONST;
            if (!accept(TK_COMMA))
                break;
            continue;
        }
        if (type_is_struct(type)) {
            local_struct(ext, name, line);
            if (bc)
                sym_at(sym_find(name))->quals |= SQ_CONST;
            if (!accept(TK_COMMA))
                break;
            continue;
        }
        not_void(type, "a variable", line);

        off = gen_local(type_scalar_bytes(type));
        if (accept(TK_ASSIGN)) {
            Type outer = narrow_dest;

            narrow_dest = type_narrow(type);
            expr();
            narrow_dest = outer;
            vstore_local(off, type);
            vdrop();            /* a declaration is not an expression */
        }
        /* Pushed after the initialiser, so `int x = x;` does not see itself. */
        sym = sym_push(name, SYM_LOCAL, off);
        sym_at(sym)->type = type;
        sym_at(sym)->ext = (unsigned char) ext;
        sym_at(sym)->quals = decl_quals | (bc ? SQ_CONST : 0);
        if (stars != base ? stars_const : bc)
            sym_at(sym)->kind = SYM_LOCAL_CONST;

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

static long     *case_value;
static uint32_t *case_high;     /* the top four bytes, for a long long */
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
    if (vla_mark >= 0)
        gen_stack_back(vla_mark);
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
    if (vla_mark >= 0)
        gen_stack_back(vla_mark);
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
static long case_constant(uint32_t *high)
{
    int line = tok_line;
    long value;

    *high = 0;
    if (tok == TK_INT && type_wide(tok_type)) {
        value = tok_val;
        *high = type_eight(tok_type) ? tok_val_hi
                                     : (uint32_t) (value < 0 ? -1 : 0);
        next();
    } else if (tok == TK_MINUS) {
        /* A sign binds to what follows it and no further, so `-3 + 2` is
         * -1: a wide literal after it is negated here, and anything else
         * goes back into the expression the way a unary minus does. */
        int before = out_here();

        next();
        if (tok == TK_INT && type_wide(tok_type)) {
            uint64_t whole = (uint64_t) (uint32_t) tok_val;

            if (type_eight(tok_type))
                whole |= (uint64_t) tok_val_hi << 32;
            whole = 0 - whole;
            value = (long) (uint32_t) whole;
            *high = (uint32_t) (whole >> 32);
            next();
        } else {
            Type outer = narrow_dest;

            narrow_dest = 0;
            primary();
            vneg();
            binary_rest(PREC_LOWEST);
            narrow_dest = outer;
            value = constant_folded("a case label", line, before);
            *high = (uint32_t) (value < 0 ? -1 : 0);
        }
    } else {
        value = constant_int("a case label", line);
        *high = (uint32_t) (value < 0 ? -1 : 0);
    }

    if (type_eight(in_switch.type))
        return (long) (uint32_t) value;
    if (type_wide(in_switch.type)) {
        *high = 0;

        return (long) (uint32_t) value;
    }
    *high = 0;

    return value & 0xffffff;
}

/* `case constant:` -- where the code for it starts, noted for the tests at
 * the end of the switch. The statement after it is parsed by the caller. */
__attribute__((noinline))
static void case_label(void)
{
    int line = tok_line, i;
    long value;
    uint32_t high;

    next();
    if (in_switch.case_mark < 0)
        acc_error_at(line, "'case' is not inside a switch");
    value = case_constant(&high);
    expect(TK_COLON, "':' after a case");

    for (i = in_switch.case_mark; i < ncases; i++)
        if (case_value[i] == value && case_high[i] == high)
            acc_error_at(line, "this switch already has a case for %ld",
                         type_unsigned(in_switch.type) || type_wide(in_switch.type)
                         ? value : (long) ((value ^ 0x800000) - 0x800000));

    if (ncases == cases_cap) {
        cases_cap = cases_cap ? cases_cap * 2 : 16;
        case_value = realloc(case_value, (size_t) cases_cap * sizeof *case_value);
        case_high = realloc(case_high, (size_t) cases_cap * sizeof *case_high);
        case_at = realloc(case_at, (size_t) cases_cap * sizeof *case_at);
        if (!case_value || !case_high || !case_at)
            acc_error("out of memory for case labels");
    }
    case_value[ncases] = value;
    case_high[ncases] = high;
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
    comma_expr();
    expect(TK_RPAREN, "')'");
    type = vtype();
    if (type_pointer(type) || type_float(type))
        acc_error_at(line, "a switch needs an integer, and this is %s",
                     type_pointer(type) ? "a pointer" : "a floating-point value");
    type = type_promote(type);
    vconvert(type);
    slot = gen_local(type_scalar_bytes(type));
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
        gen_switch_case(case_value[i], case_high[i], type, case_at[i], slot);
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

    if (starts_decl()) {
        declaration();                  /* and its semicolon */
    } else {
        if (tok != TK_SEMI) {
            comma_expr();
            vdrop();
        }
        expect(TK_SEMI, "';'");
    }

    gen_stmt_end();
    top = gen_here();
    if (tok != TK_SEMI) {
        comma_expr();
        to_end = gen_jump_if_false();
    }
    expect(TK_SEMI, "';'");

    again = top;
    if (tok != TK_RPAREN) {
        to_body = gen_jump();
        again = gen_here();
        gen_stmt_end();
        comma_expr();
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

/* `{ ... }`: declarations and statements in any order, as C99 has them, and
 * what is declared in it ends with it -- an inner name shadows an outer one
 * of the same spelling until the brace closes. The frame bytes are not
 * reused; the scope is only which names mean what. The body of a function is
 * one of these too. */
static void block(void)
{
    int mark = sym_scope_begin(), outer = scope_mark;
    int outer_vla = vla_mark;

    scope_mark = mark;
    vla_mark = -1;
    while (tok != TK_RBRACE && tok != TK_EOF) {
        if (starts_decl())
            declaration();
        else
            statement();
    }
    expect(TK_RBRACE, "'}'");
    if (vla_mark >= 0)
        gen_stack_back(vla_mark);       /* the room those arrays took */
    vla_mark = outer_vla;
    sym_scope_end(mark);
    scope_mark = outer;
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
    comma_expr();
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
            comma_expr();
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
        comma_expr();
        vdrop();                /* the value of a statement is discarded */
        expect(TK_SEMI, "';'");

        return;
    }
}

/* The struct parameters of the function being defined, which are copied
 * into its frame once the prologue has been emitted: see function_rest. */
typedef struct {
    int sym, argoff;
} StructParam;

static StructParam *struct_params;
static int          nstruct_params, struct_params_cap;

/* A struct parameter is copied from its slots into the array area, where
 * every local struct lives, so that the body finds it as it finds any
 * other: the slots are above the frame, and past the reach of (ix+d) once
 * there are a few of them. Out of line, for the few functions that have
 * one. */
__attribute__((noinline))
static void struct_params_copy(void)
{
    int i;

    for (i = 0; i < nstruct_params; i++) {
        Sym *param = sym_at(struct_params[i].sym);
        int array = gen_local_array(), x = param->ext;

        gen_local_array_size(array, ext_bytes(x));
        vaddr_array(array, TY_STRUCT);
        vset_ext(x);
        vaddr_local(struct_params[i].argoff, TY_STRUCT);
        vset_ext(x);
        vderef();
        vstore_indirect();
        vdrop();
        param = sym_at(struct_params[i].sym);
        param->kind = SYM_LOCAL_STRUCT;
        param->val = array;
    }
}

/* That a function declared again is declared the same way: its result, and
 * its parameters if both declarations give them. */
static void same_signature(int fn, Type ret_type, int ret_ext, int first,
                           int count, int params, NameRef name, int line)
{
    int i, prior = sym_params_first(fn);

    if (sym_at(fn)->type != ret_type || sym_at(fn)->ext != ret_ext)
        acc_error_at(line, "'%s' is declared again with another result type",
                     name_text(name));
    if (!params || !(sym_flags(fn) & SYMF_PARAMS))
        return;
    if (count != sym_nparams(fn))
        acc_error_at(line, "'%s' is declared again with %d parameters, not %d",
                     name_text(name), count, sym_nparams(fn));
    for (i = 0; i < count; i++)
        if (sym_param_type(first, i) != sym_param_type(prior, i)
            || sym_param_ext(first, i) != sym_param_ext(prior, i))
            acc_error_at(line, "'%s' is declared again with parameter %d of "
                               "another type", name_text(name), i + 1);
}

/* A function's declarator, from just past its name: its parameters, and
 * then either its body -- a definition -- or nothing more, which makes it a
 * prototype. Returns whether it was a definition.
 *
 * The parameters are read the same way for both, as locals at the frame
 * offsets their arguments arrive at; a prototype drops them again at the
 * `)`. A prototype may leave their names out. `()` in a prototype says
 * nothing about the parameters, as C has it, so calls convert nothing; in
 * a definition it means there are none. */
static int function_declarator(Type ret_type, int ret_ext, NameRef name,
                               int line)
{
    int fn, declared, params = 1, unnamed = 0, mark, variadic = 0;
    int nparams = 0, argoff, params_first;

    expect(TK_LPAREN, "'('");

    fn = sym_find(name);
    if (fn != SYM_NONE && sym_at(fn)->kind != SYM_FUNC)
        acc_error_at(line, "'%s' is already %s", name_text(name),
                     sym_at(fn)->kind == SYM_GLOBAL ? "a variable"
                                                    : "declared");
    declared = fn != SYM_NONE && (sym_flags(fn) & SYMF_DECLARED);

    /* A call before any declaration took the result to be an int, which
     * for a struct is not something that can be put right afterwards. */
    if (fn != SYM_NONE && !declared && type_is_struct(ret_type))
        acc_error_at(line, "'%s' returns a struct, so it has to be declared "
                           "before it is called", name_text(name));

    /* Pushed before the parameters: a file-scope symbol goes in below the
     * locals, which would move the parameters along if it came after. */
    if (fn == SYM_NONE)
        fn = sym_push(name, SYM_FUNC, 0);

    /* The first argument sits above the saved ix and the return address --
     * and above the hidden one, the address a struct result goes to, when
     * there is one. */
    mark = sym_scope_begin();
    params_first = sym_params_begin();
    argoff = 2 * ACC_PTR_SIZE;
    if (type_is_struct(ret_type))
        argoff += ACC_PTR_SIZE;
    nstruct_params = 0;
    abstract_ok = 1;
    in_params = 1;
    if (tok == TK_KW_VOID && lex_rparen_follows()) {
        next();
    } else if (tok == TK_RPAREN) {
        params = 0;
    } else {
        for (;;) {
            Type pbase, ptype, pstars;
            unsigned char pconst, pquals = 0;
            int pbx, pline = tok_line, pcount, pext;
            NameRef pname;
            int psym = SYM_NONE;

            if (accept(TK_KW_REGISTER))
                pquals = SQ_REGISTER;
            pbase = base_type();
            pbx = base_ext;
            pconst = base_const;
            if (pconst)
                pquals |= SQ_CONST;
            pstars = declarator_stars(pbase);
            pname = direct_declarator(pstars, pbx, &ptype, &pext, &pcount);
            if (pstars != pbase)
                pconst = stars_const;
            if (!pcount)
                func_suffix(&ptype, &pext);
            if (type_is_func(ptype))        /* a function is its address */
                ptype = type_ptr_to(ptype);

            /* `int a[]`, `int a[10]` and `int m[][4]` declare a parameter
             * that is a pointer, as C says: an array is passed as the address
             * of its first element, and the first size, if there is one, is
             * not kept. The others are: they are the row a step takes. */
            if (pcount) {
                if (type_ptr_depth(ptype) == TY_PTR_MAX)
                    acc_error_at(tok_line, "a pointer can be %d deep and this "
                                           "is deeper", TY_PTR_MAX);
                ptype = type_ptr_to(ptype);
            }
            not_void(ptype, "a parameter", pline);

            if (pname) {
                psym = sym_push(pname, SYM_LOCAL, argoff);
                sym_at(psym)->type = ptype;
                sym_at(psym)->ext = (unsigned char) pext;
                sym_at(psym)->quals = pquals;
                if (pconst && !pcount && !type_is_struct(ptype))
                    sym_at(psym)->kind = SYM_LOCAL_CONST;
            } else {
                unnamed = 1;
            }
            sym_param_add(ptype, pext);

            /* Every argument occupies whole slots: one however narrow it is,
             * two for a long, and as many as a struct fills. That is what
             * agondev does, and the narrow ones are read from the low bytes
             * of their slot. */
            if (type_is_struct(ptype)) {
                record_complete(pext, pline);
                if (nstruct_params == struct_params_cap) {
                    struct_params_cap = struct_params_cap
                                        ? struct_params_cap * 2 : 8;
                    struct_params = realloc(struct_params,
                                            (size_t) struct_params_cap
                                            * sizeof *struct_params);
                    if (!struct_params)
                        acc_error("out of memory for parameters");
                }
                struct_params[nstruct_params].sym = psym;
                struct_params[nstruct_params].argoff = argoff;
                nstruct_params++;
                argoff += (ext_bytes(pext) + ACC_INT_SIZE - 1) / ACC_INT_SIZE
                          * ACC_INT_SIZE;
            } else {
                argoff += va_slot(ptype, 0);
            }
            nparams++;
            if (!accept(TK_COMMA))
                break;
            if (accept(TK_ELLIPSIS)) {
                variadic = SYMF_VARIADIC;
                break;
            }
        }
    }
    abstract_ok = 0;
    in_params = 0;
    expect(TK_RPAREN, "')'");

    if (declared) {
        same_signature(fn, ret_type, ret_ext, params_first, nparams, params,
                       name, line);
        if (params && (sym_flags(fn) & SYMF_PARAMS)
            && (sym_flags(fn) & SYMF_VARIADIC) != variadic)
            acc_error_at(line, "'%s' is declared again with%s '...'",
                         name_text(name), variadic ? "" : "out");
    }
    sym_at(fn)->type = ret_type;
    sym_at(fn)->ext = (unsigned char) ret_ext;
    sym_at(fn)->quals = decl_bottom_const;

    /* A prototype: what it said is kept -- unless an earlier one already
     * gave the parameters and this one does not -- and the parameters are
     * dropped. A function inside another's body can only be declared. */
    if (tok != TK_LBRACE || in_body) {
        sym_scope_end(mark);
        if (!declared || !(sym_flags(fn) & SYMF_PARAMS)) {
            sym_set_params(fn, params_first, nparams);
            sym_set_flags(fn, params ? SYMF_DECLARED | SYMF_PARAMS | variadic
                                     : SYMF_DECLARED);
        }

        return 0;
    }

    /* Asked of the flag and not of the address, because compiling to an
     * object puts the first function at offset zero, and zero is what a
     * function that has not been defined has. */
    if (sym_flags(fn) & SYMF_DEFINED)
        acc_error_at(line, "'%s' is defined twice", name_text(name));
    if (unnamed)
        acc_error_at(line, "a parameter of a function's definition needs a "
                           "name");
    next();                     /* the body's `{` */

    sym_set_params(fn, params_first, nparams);
    sym_set_flags(fn, SYMF_DECLARED | SYMF_PARAMS | SYMF_DEFINED | variadic);
    current_fn = fn;
    gen_func_begin(fn, nparams, ret_type);

    if (nstruct_params)
        struct_params_copy();
    in_body = 1;
    block();
    in_body = 0;
    labels_end();
    gen_func_end();

    sym_drop_locals();

    return 1;
}

/* How the variable being declared at file scope is bound to its name.
 *
 * A block's `static` is one too -- its bytes in the image, where they last
 * as long as the program -- but its name is the block's: static_local says
 * so. And a variable declared once already has its storage, allocated when
 * it was first declared: a later definition writes its initial value over
 * those bytes, with the output rewound to them, and binds nothing new --
 * redefining says which symbol it is. */
static int push_global(NameRef name, int kind, int at)
{
    int sym;

    if (redefining != SYM_NONE) {
        /* A name only declared so far has no address, and this is where it
         * gets one. One that already had room keeps what it had: the output
         * was wound back to it, so `at` is that address again. */
        if (sym_at(redefining)->val < 0)
            sym_at(redefining)->val = at;

        return redefining;
    }
    sym = static_local ? sym_push_local(name, kind, at)
                       : sym_push(name, kind, at);
    sym_at(sym)->quals = decl_bottom_const;

    return sym;
}

/* That the value just read is an address inside the image -- another
 * global's, a string's -- and so moves with it, which is a relocation
 * wherever it lands. The companion to gen_pending_sym, which says the same of
 * a function whose address is not known yet; a value is one or the other and
 * never both. */
static int init_address;

/* And that it is an offset into the bss, which wants the start of the bss
 * added to it wherever the bytes end up. walk_fn_add's `fn` is WALK_BSS for
 * one of these: there is no symbol, since every one of them wants the same
 * one number. */
static int init_bss;
#define WALK_BSS (-2)

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
    int size = type_scalar_bytes(type);
    int before;
    uint64_t value;
    Type from;
    int i;

    /* Read as an expression and folded, whatever it is: a long's arithmetic
     * is worked out by the compiler now, so `2L + 3L` and `1L << 20` are as
     * much a constant here as `5L` is. What a global may not have is
     * anything that leaves code behind, which is what the out_here check
     * catches -- a call, a variable, an address that is not a symbol's. */
    gen_pending_sym = SYM_NONE;
    init_address = init_bss = 0;
    gen_data_context = 1;
    expr();                     /* not comma_expr: in a braced list the
                                 * comma between values is a separator */
    gen_data_context = 0;

    /* From here on nothing may be emitted: what the expression itself left
     * in the image is a string's bytes, which are a constant's and fine.
     * What would not be fine is the conversion below turning into code,
     * which is what an initial value that is not a constant does. */
    before = out_here();
    vconvert(type);
    if (!vconst_wide(&value, &from) || out_here() != before)
        acc_error_at(line, "a global's initial value has to be a constant");
    init_address = vconst_addr();
    init_bss = vconst_bss();
    vdrop();
    gen_stmt_end();             /* the constants it used are done with */

    for (i = 0; i < size; i++) {
        bytes[i] = (unsigned char) value;
        value >>= 8;
    }
}

/* A file-scope array's bytes, built here from its initialiser before they
 * are written into the image: with nested braces the values do not arrive in
 * order of address with nothing between them, and a `[]` array's length is
 * only known at the end. */
static unsigned char *init_bytes;
static int            init_bytes_cap;
static int            init_bytes_len;   /* bytes of this array zeroed so far */

/* The buffer out to `end`, the part not reached before zeroed. Only what the
 * array uses is cleared, and once: clearing the whole buffer for each array,
 * as this first did, cost 0.8% of a compile of a program with a few dozen
 * small global arrays. */
__attribute__((noinline))
static void init_grow(int end)
{
    if (end > init_bytes_cap) {
        int cap = init_bytes_cap ? init_bytes_cap : 256;

        while (cap < end)
            cap *= 2;
        init_bytes = realloc(init_bytes, (size_t) cap);
        if (!init_bytes)
            acc_error("out of memory for an initialiser");
        init_bytes_cap = cap;
    }
    memset(init_bytes + init_bytes_len, 0, (size_t) (end - init_bytes_len));
    init_bytes_len = end;
}

static inline __attribute__((always_inline)) void init_room(int end)
{
    if (end > init_bytes_len)
        init_grow(end);
}

/* The addresses in a global's bytes, to be put in once the bytes have an
 * address of their own: for a value written straight into the image, at `at`,
 * and for one built in the initialiser's buffer, at its offset from where the
 * buffer goes.
 *
 * `fn` is a function not yet defined, whose address only gen_finish will
 * know, or SYM_NONE for an address that is already known and has only to be
 * recorded as one so that it moves with the image.
 *
 * It grew from a fixed sixteen when that second kind arrived. Sixteen was
 * generous for functions not yet defined; it is not for addresses, which is
 * every entry of `char *names[] = { "a", "b", ... }`. */
static struct { int fn, offset; } *walk_fns;
static int nwalk_fns, walk_fns_cap;

static void walk_fn_add(int fn, int offset)
{
    if (nwalk_fns == walk_fns_cap) {
        walk_fns_cap = walk_fns_cap ? walk_fns_cap * 2 : 16;
        walk_fns = realloc(walk_fns, (size_t) walk_fns_cap * sizeof *walk_fns);
        if (!walk_fns)
            acc_error("out of memory for an initialiser's addresses");
    }
    walk_fns[nwalk_fns].fn = fn;
    walk_fns[nwalk_fns].offset = offset;
    nwalk_fns++;
}

static void data_fn_at(int at)
{
    if (init_address) {
        out_reloc(at);
        init_address = 0;
    }
    if (init_bss) {
        gen_bss_fixup(at);
        init_bss = 0;
    }

    if (gen_pending_sym == SYM_NONE)
        return;
    gen_data_fixup(gen_pending_sym, at);
    gen_pending_sym = SYM_NONE;
}

static void walk_fns_at(int at)
{
    int i;

    for (i = 0; i < nwalk_fns; i++) {
        if (walk_fns[i].fn == WALK_BSS)
            gen_bss_fixup(at + walk_fns[i].offset);
        else if (walk_fns[i].fn == SYM_NONE)
            out_reloc(at + walk_fns[i].offset);
        else
            gen_data_fixup(walk_fns[i].fn, at + walk_fns[i].offset);
    }
    nwalk_fns = 0;
}

/* A bit-field's value in a global's bytes: the constant, cut to the width,
 * put in at its bits. */
__attribute__((noinline))
static void global_bits(Type scalar, int offset)
{
    const BitField *bf = bitfield_at(init_bits);
    unsigned char bytes[ACC_LONG_SIZE] = { 0 };
    uint32_t value = 0;
    int i;

    global_initializer(scalar == TY_BOOL ? TY_BOOL : TY_ULONG, bytes, tok_line);
    for (i = ACC_LONG_SIZE - 1; i >= 0; i--)
        value = value << 8 | bytes[i];
    if (bf->width < 32)
        value &= (1UL << bf->width) - 1;
    init_room(offset + bf->bytes);
    for (i = 0; i < bf->width; i++)
        if (value >> i & 1)
            init_bytes[offset + (bf->pos + i) / 8] |=
                (unsigned char) (1 << (bf->pos + i) % 8);
}

static void global_put(Type scalar, int offset, int value)
{
    if (init_bits) {
        global_bits(scalar, offset);

        return;
    }
    init_room(offset + type_scalar_bytes(scalar));
    if (value >= 0)
        init_bytes[offset] = (unsigned char) value;     /* a char of a string */
    else
        global_initializer(scalar, init_bytes + offset, tok_line);
    if (gen_pending_sym != SYM_NONE) {
        walk_fn_add(gen_pending_sym, offset);
        gen_pending_sym = SYM_NONE;
    } else if (init_address) {
        walk_fn_add(SYM_NONE, offset);
        init_address = 0;
    } else if (init_bss) {
        walk_fn_add(WALK_BSS, offset);
        init_bss = 0;
    }
}

/* The static half of a compound literal: at file scope its bytes are built
 * the way a global's are, in the initialiser's buffer, and left in the
 * image. See compound_literal, which is where the rest of it is. */
__attribute__((noinline))
static void literal_bytes(Type type, int x, int count, Type elem, int elem_x,
                          int line, int address, int *countp)
{

        int at, total, i;

        init_bytes_len = 0;
        expect(TK_LBRACE, "'{'");
        if (count) {
            int n = init_list(elem, elem_x, count, 0, global_put, 0);

            if (count < 0) {
                if (n == 0)
                    acc_error_at(line, "a compound literal of an array with no "
                                       "size needs a value to say how long "
                                       "it is");
                count = n;
            }
            total = count * type_bytes(elem, elem_x);
        } else if (type_is_struct(type)) {
            init_record(x, 0, global_put, 1);
            total = ext_bytes(x);
        } else {
            global_put(type, 0, -1);
            accept(TK_COMMA);
            expect(TK_RBRACE, "'}'");
            total = type_scalar_bytes(type);
        }
        init_room(total);
        at = out_here();
        walk_fns_at(at);
        for (i = 0; i < total; i++)
            out_byte(init_bytes[i]);

        if (count) {
            vpush_const(at, type_ptr_to(elem));
            vset_addr();
            vset_ext(elem_x);
            if (countp)
                *countp = count;
        } else if (type_is_struct(type)) {
            vpush_const(at, type_ptr_to(TY_STRUCT));
            vset_addr();
            vset_ext(x);
            if (!address)
                vderef();
        } else {
            vpush_const(at, type_ptr_to(type));
            vset_addr();
            if (!address)
                vderef();
        }
}

/* A file-scope array, from just past its declarator: its bytes, each value a
 * constant as a global's value has to be and zeros for the rest, written into
 * the image. A `[]` array is as long as its initialiser. */
static void global_array(Type elem, int elem_x, NameRef name, int count,
                         int line)
{
    int step = type_bytes(elem, elem_x), total, at, sym, i;
    /* Read as a test and then consumed, rather than as accept's value.
     * agondev's clang lowers `x = accept(t)` into a compare, the zero for
     * the other arm, and a conditional call -- and puts the zero, which is
     * `or a, a` and `sbc hl, hl`, between the compare and the call. Both
     * instructions write the zero flag the call reads, so the call is made
     * whatever the token was: every array declared with no initial value
     * went looking for a '{'. */
    int init = tok == TK_ASSIGN;

    if (init)
        next();

    /* One dimension, which is most arrays, needs none of the walk: its values
     * arrive in order, each written as it is read, and zeros follow. As the
     * walk, with a buffer, it cost 1% of a compile of a program with a few
     * dozen small ones. A string for a char array goes the walk's way, which
     * already knows strings. */
    /* A pointer goes the other way, because an element's value may put bytes
     * in the image before it is known -- a string's, a compound literal's --
     * and the walk writes each element where the output happens to be. Those
     * bytes would land between two elements of the array being built, which
     * is how `char *names[] = { "a", "b" }` came out as a string, a pointer,
     * a string and a pointer rather than as four pointers. Built in the
     * buffer, the array's address is taken once everything it refers to has
     * been written, and it is contiguous. */
    if (!type_is_array(elem) && !type_is_struct(elem) && !type_pointer(elem)
        && !(init && tok == TK_STRING)) {
        int n = 0, designated = 0;

        at = out_here();
        if (init) {
            expect(TK_LBRACE, "'{'");
            while (tok != TK_RBRACE) {
                unsigned char bytes[8] = { 0 };

                /* A designator: the rest of the list is not in order, and
                 * writing it as it is read no longer works. What has been
                 * written -- the first n elements, in order -- goes into the
                 * buffer the walk builds, the image is wound back to where
                 * they were, and the walk carries on from there. */
                if (tok_designator()) {
                    init_bytes_len = 0;
                    init_room(n * step);
                    out_copy(at, init_bytes, n * step);
                    out_rewind(at);
                    n = init_list(elem, elem_x, count, 0, global_put, n);
                    designated = 1;
                    break;
                }
                if (n == count)
                    acc_error_at(tok_line, "more initial values than the array "
                                           "has elements");
                if (accept(TK_LBRACE)) {
                    global_initializer(elem, bytes, tok_line);
                    accept(TK_COMMA);
                    expect(TK_RBRACE, "'}'");
                } else {
                    global_initializer(elem, bytes, tok_line);
                }
                /* Nothing may have been written since the last element, or
                 * this one is not where the array says it is. Only a value
                 * that leaves bytes behind can do that, and the element
                 * types that have such values were sent the other way. */
                if (out_here() != at + n * step)
                    acc_error_at(tok_line, "an initial value here would put "
                                           "bytes inside the array");
                data_fn_at(out_here());
                for (i = 0; i < step; i++)
                    out_byte(bytes[i]);
                n++;
                if (!accept(TK_COMMA))
                    break;
            }
            if (!designated)
                expect(TK_RBRACE, "'}'");    /* the walk read its own */
            if (count < 0) {
                if (n == 0)
                    acc_error_at(line, "an array needs at least one element");
                count = n;
            }
        } else if (count < 0) {
            acc_error_at(line, "an array declared with [] needs initial values "
                               "to say how long it is");
        }
        if (designated) {
            init_room(count * step);
            walk_fns_at(at);
            for (i = 0; i < count * step; i++)
                out_byte(init_bytes[i]);
        } else {
            for (i = n * step; i < count * step; i++)
                out_byte(0);
        }

        sym = push_global(name, SYM_GLOBAL_ARRAY, at);
        sym_at(sym)->type = elem;
        sym_at(sym)->ext = (unsigned char) elem_x;
        sym_set_count(sym, count);

        return;
    }

    init_bytes_len = 0;

    if (init && tok == TK_STRING && type_is_char(elem)) {
        int n = init_string(count, 0, global_put);

        if (count < 0)
            count = n;
    } else if (init) {
        int n;

        expect(TK_LBRACE, "'{'");
        n = init_list(elem, elem_x, count, 0, global_put, 0);
        if (count < 0) {
            if (n == 0)
                acc_error_at(line, "an array needs at least one element");
            count = n;
        }
    } else if (count < 0) {
        acc_error_at(line, "an array declared with [] needs initial values to "
                           "say how long it is");
    }

    total = count * step;
    init_room(total);
    at = out_here();
    walk_fns_at(at);
    for (i = 0; i < total; i++)
        out_byte(init_bytes[i]);

    sym = push_global(name, SYM_GLOBAL_ARRAY, at);
    sym_at(sym)->type = elem;
    sym_at(sym)->ext = (unsigned char) elem_x;
    sym_set_count(sym, count);
}

/* A file-scope struct or union: its bytes built from a braced initialiser,
 * as an array's are, zeros for whatever it does not give. */
__attribute__((noinline))
static void global_struct(int x, NameRef name, int line)
{
    int total, at, sym, i;

    record_complete(x, line);
    total = ext_bytes(x);
    init_bytes_len = 0;
    if (accept(TK_ASSIGN)) {
        expect(TK_LBRACE, "'{'");
        init_record(x, 0, global_put, 1);
    }
    init_room(total);
    at = out_here();
    walk_fns_at(at);
    for (i = 0; i < total; i++)
        out_byte(init_bytes[i]);

    sym = push_global(name, SYM_GLOBAL, at);
    sym_at(sym)->type = TY_STRUCT;
    sym_at(sym)->ext = (unsigned char) x;
}

/* That a variable declared again at file scope is declared the same way,
 * and whether this declaration gives it its value -- which it may only do
 * once. */
static int global_again(int sym, Type type, int ext, int count, int line)
{
    const Sym *g = sym_at(sym);
    NameRef name = g->name;

    if (g->kind == SYM_FUNC)
        acc_error_at(line, "'%s' is already a function", name_text(name));
    if (count < 0 && g->kind == SYM_GLOBAL_ARRAY)
        count = sym_count(sym);         /* `int a[] = ...` after `int a[4]` */
    /* An array declared with no size takes the size a later declaration
     * gives it: `extern int a[];` and then `int a[4];` is one array, not two
     * declared differently. */
    if (g->kind == SYM_GLOBAL_ARRAY && sym_count(sym) < 0 && count > 0)
        sym_set_count(sym, count);
    if (g->kind != (count ? SYM_GLOBAL_ARRAY
                          : decl_const && !type_is_struct(type) ? SYM_GLOBAL_CONST
                          : SYM_GLOBAL)
        || g->type != type || g->ext != ext
        || (count && count != sym_count(sym)))
        acc_error_at(line, "'%s' is declared again with another type",
                     name_text(name));
    if (tok != TK_ASSIGN)
        return 0;
    if (sym_flags(sym) & SYMF_DEFINED)
        acc_error_at(line, "'%s' is defined twice", name_text(name));

    return 1;
}

/* `extern int a[];`: an array with no size yet, so nowhere to put it. A
 * cell of three bytes is put here instead, to hold its address once a
 * definition gives it one, and a use until then reads the address from the
 * cell as the program runs. The definition writes it there, and from then
 * on the array is an ordinary one, used directly. */
static void global_emit(Type type, int ext, NameRef name, int count, int line);

/* A file-scope variable's bytes, from its initialiser if it has one, and
 * its name bound to them -- or, when redefining, written over the bytes it
 * already has.
 *
 * Between two functions is as good a place as any: nothing runs into it,
 * since every function ends in a return, and the address is known the moment
 * it is written, so nothing that refers to it ever needs patching. A global
 * with no initial value is zero, as C says, and takes its bytes in the image
 * like any other -- there is no separate zeroed area yet. */
static void global_emit(Type type, int ext, NameRef name, int count, int line)
{
    unsigned char bytes[8] = { 0 };
    int size = type_scalar_bytes(type), sym, i, at;

    if (count) {
        global_array(type, ext, name, count, line);

        return;
    }
    if (type_is_struct(type)) {
        global_struct(ext, name, line);

        return;
    }

    not_void(type, "a variable", line);
    if (accept(TK_ASSIGN))
        global_initializer(type, bytes, line);

    at = out_here();
    data_fn_at(at);
    for (i = 0; i < size; i++)
        out_byte(bytes[i]);

    sym = push_global(name, decl_const ? SYM_GLOBAL_CONST : SYM_GLOBAL, at);
    sym_at(sym)->type = type;
    sym_at(sym)->ext = (unsigned char) ext;
}

/* How many bytes a variable takes, worked out from what its declaration
 * said. The same three cases global_emit writes out, asked the other way
 * round: it is the declaration that is remembered, not the size. */
static int global_bytes(int sym)
{
    const Sym *s = sym_at(sym);

    if (s->kind == SYM_GLOBAL_ARRAY)
        return sym_count(sym) * type_bytes(s->type, s->ext);
    if (type_is_struct(s->type))
        return ext_bytes(s->ext);

    return type_scalar_bytes(s->type);
}

/* A name with everything the type says and no address at all: a variable
 * some other file defines, or an array with no size yet, which is the same
 * thing said another way -- there is nothing to reserve room by.
 *
 * -1 is the address until something gives it one, and every use of it in
 * between is written down for gen_finish or the linker to fill in. An array
 * with no size used to get a cell here instead: three bytes to hold its
 * address once a definition gave it one, which every use of the array read
 * as the program ran. A relocation is what that was standing in for, and it
 * does the same job without the load.
 *
 * The kind is the one a definition would push, so that a later declaration
 * of the same thing agrees with this one. */
static void global_undefined(Type type, int ext, NameRef name, int count,
                             int line, int is_extern)
{
    int kind = count ? SYM_GLOBAL_ARRAY
             : decl_const && !type_is_struct(type) ? SYM_GLOBAL_CONST
             : SYM_GLOBAL;
    int sym;

    not_void(type, "a variable", line);
    sym = push_global(name, kind, -1);
    sym_at(sym)->type = type;
    sym_at(sym)->ext = (unsigned char) ext;
    sym_at(sym)->quals = decl_bottom_const;
    if (kind == SYM_GLOBAL_ARRAY)
        sym_set_count(sym, count);
    if (is_extern) {
        sym_set_flags(sym, SYMF_EXTERN);

        return;
    }

    /* Room in the bss now, rather than at the end of the file, so that every
     * use of it between here and there knows where in the bss it is and can
     * fold that into whatever is done to it. An array declared with no size
     * has to wait -- there is nothing to reserve room by -- and bss_end
     * gives it room once a later declaration has said how long it is.
     *
     * If a later declaration gives this one a value after all, its bytes go
     * in the file and the room reserved here is left empty. That costs a few
     * bytes of an area that is all zeros anyway, and it is the price of
     * every other use of it folding. */
    if (count >= 0) {
        int at = gen_bss_reserve(global_bytes(sym));

        sym_at(sym)->val = sym_bss_val(at);

        /* Only what is at file scope is written down as a symbol. A block's
         * static is a local one, dropped at the end of its function and its
         * place in the table handed to somebody else's local -- so nothing
         * may hold on to it. Nothing needs to: it is reached by its offset,
         * and no other file can name it. */
        if (!static_local)
            gen_bss_symbol(sym, at);
    }
}

/* One file-scope variable, or a block's `static` one.
 *
 * C lets `int x;` be said twice at file scope -- and `extern int x;` as
 * often as it likes -- as long as at most one of them gives a value. The
 * first allocates the bytes, zero, which is what a variable no declaration
 * gives a value has; the one that gives it writes it there, with the output
 * rewound to them. So a use between the two finds the address the variable
 * will always have. */
static void global_variable(Type type, int ext, NameRef name, int count,
                            int line)
{
    int sym, init = (tok == TK_ASSIGN);

    /* `extern T x;` and nothing more. Some file defines x and reserves room
     * for it; this one does not, or the two would be different variables at
     * different addresses. Until something here gives it an address its
     * address is -1, and every use of it is written down for gen_finish or
     * the linker to fill in -- which is what a call to a function not yet
     * seen has always been.
     *
     * An array with no size goes the same way and for the same reason:
     * there is no size to reserve room by. */
    if (decl_extern && !init && !static_local) {
        int known = name_global(name);

        if (known == SYM_NONE) {
            global_undefined(type, ext, name, count, line, 1);

            return;
        }
        (void) global_again(known, type, ext, count, line);
        sym_set_flags(known, SYMF_EXTERN);

        return;
    }

    if (!static_local && (sym = name_global(name)) != SYM_NONE) {
        int saved, again = global_again(sym, type, ext, count, line);

        if (count < 0)
            count = sym_count(sym);

        /* Declared before and given no room then. A declaration that gives
         * it a value is what reserves that room, here, in the file. One
         * that does not leaves it where it was -- still waiting, and by the
         * end of the file either in the bss or another file's to define.
         *
         * Either way this file is no longer only declaring it: `extern int
         * x;` and then `int x;` is a definition, as C says, and what makes
         * it one is that the second says nothing about extern. */
        if (!decl_extern)
            sym_clear_flags(sym, SYMF_EXTERN);
        if (sym_at(sym)->val < 0) {
            if (!again)
                return;

            /* It was given room in the bss when it was first declared, and
             * now it has a value, so its bytes go in the file instead. The
             * room is abandoned -- unless something compiled in between was
             * told to look in it, and then there is nowhere for that to be
             * put right. */
            if (sym_in_bss(sym_at(sym)->val)) {
                int at = sym_bss_at(sym_at(sym)->val);

                if (gen_bss_used(at, global_bytes(sym)))
                    acc_error_at(line, "'%s' is used above and given a value "
                                       "here, and what is above it was "
                                       "compiled to find it where a variable "
                                       "with no value goes; give it its value "
                                       "where it is first declared",
                                 name_text(name));
                gen_bss_forget(sym);
            }
            redefining = sym;
            global_emit(type, ext, name, count, line);
            redefining = SYM_NONE;
            sym_set_flags(sym, SYMF_DEFINED);

            return;
        }
        if (!again)
            return;
        saved = out_here();
        out_seek(sym_at(sym)->val);
        redefining = sym;
        global_emit(type, ext, name, count, line);
        redefining = SYM_NONE;
        out_seek(saved);
        sym_set_flags(sym, SYMF_DEFINED);

        return;
    }

    /* No initial value, so C says it starts at zero -- and zeros do not have
     * to be in the file. It is given room past the image's last byte -- at
     * its declaration when its size is known, and by bss_end otherwise.
     *
     * A block's static goes the same way. It has no name outside the
     * function it is in and its symbol is dropped at the end of that
     * function, so there is nothing to hang a symbol on at the end -- but it
     * needs none: a use of it is a slot holding its offset into the bss, and
     * what puts those right is the start of the bss, which is the same one
     * number for all of them. */
    if (!init) {
        global_undefined(type, ext, name, count, line, 0);

        return;
    }
    global_emit(type, ext, name, count, line);
    if (init && !static_local)
        sym_set_flags(name_global(name), SYMF_DEFINED);
}

/* At the end of the file: every variable it declared, never gave a value to,
 * and did not say another file defines. Those are the ones that start at
 * zero, and this is where they are given room.
 *
 * At the end and not where they are declared, because until the file has
 * been read a later declaration may still give one of them a value -- which
 * C calls a tentative definition, and acc has always let it do. */
static void bss_end(void)
{
    int step = (int) sizeof(Sym);
    int s;

    for (s = 0; s < sym_nglobals(); s += step) {
        Sym *sym = sym_at(s);
        int at;

        /* Variables, and only variables. A function has an address or does
         * not; a typedef and a tag have no room of their own; and an enum
         * constant's val is the constant itself, which `enum { BELOW = -1 }`
         * makes negative -- and negative is what "no address yet" looks
         * like. */
        if (sym->kind != SYM_GLOBAL && sym->kind != SYM_GLOBAL_ARRAY
            && sym->kind != SYM_GLOBAL_CONST)
            continue;
        if (sym->val != -1)             /* placed already, here or in the bss */
            continue;
        if (sym_flags(s) & SYMF_EXTERN)
            continue;                   /* another file's to define */
        if (sym->kind == SYM_GLOBAL_ARRAY && sym_count(s) < 0)
            acc_error("'%s' is declared with no size and never given one",
                      name_text(sym->name));
        at = gen_bss_reserve(global_bytes(s));
        sym_at(s)->val = sym_bss_val(at);
        gen_bss_symbol(s, at);
    }
}

/* A function declared by a declarator whose type came out a function --
 * `int (*get(void))(int)`, or `op f;` with op a typedef for a function
 * type -- rather than by a name and a parameter list. It is declared as
 * any function is, and defined, when a body follows, with the parameters
 * its type's parameter list named. Returns whether it was defined. */
__attribute__((noinline))
static int function_from_type(int x, NameRef name, int line)
{
    Type ret = ext_elem(x);
    int ret_x = ext_elem_x(x), first = ext_func_first(x);
    int count = ext_func_count(x), params = ext_func_declared(x);
    int fn = sym_find(name), i, argoff;

    if (fn != SYM_NONE && sym_at(fn)->kind != SYM_FUNC)
        acc_error_at(line, "'%s' is already declared", name_text(name));
    if (fn != SYM_NONE && (sym_flags(fn) & SYMF_DECLARED))
        same_signature(fn, ret, ret_x, first, count, params, name, line);
    if (fn == SYM_NONE)
        fn = sym_push(name, SYM_FUNC, 0);
    sym_at(fn)->type = ret;
    sym_at(fn)->ext = (unsigned char) ret_x;
    if (tok != TK_LBRACE || in_body) {
        sym_set_params(fn, first, count);
        sym_set_flags(fn, params ? SYMF_DECLARED | SYMF_PARAMS
                                   | (ext_func_variadic(x) ? SYMF_VARIADIC : 0)
                                 : SYMF_DECLARED);

        return 0;
    }

    if (sym_flags(fn) & SYMF_DEFINED)
        acc_error_at(line, "'%s' is defined twice", name_text(name));
    argoff = 2 * ACC_PTR_SIZE + (type_is_struct(ret) ? ACC_PTR_SIZE : 0);
    nstruct_params = 0;
    for (i = 0; i < count; i++) {
        Type t = sym_param_type(first, i);
        int e = sym_param_ext(first, i), psym;

        if (i >= param_names_cap || !param_names[first + i])
            acc_error_at(line, "a parameter of a function's definition needs "
                               "a name");
        psym = sym_push(param_names[first + i], SYM_LOCAL, argoff);
        sym_at(psym)->type = t;
        sym_at(psym)->ext = (unsigned char) e;
        if (type_is_struct(t)) {
            if (nstruct_params == struct_params_cap) {
                struct_params_cap = struct_params_cap ? struct_params_cap * 2 : 8;
                struct_params = realloc(struct_params, (size_t) struct_params_cap
                                                       * sizeof *struct_params);
                if (!struct_params)
                    acc_error("out of memory for parameters");
            }
            struct_params[nstruct_params].sym = psym;
            struct_params[nstruct_params].argoff = argoff;
            nstruct_params++;
            argoff += (ext_bytes(e) + ACC_INT_SIZE - 1) / ACC_INT_SIZE
                      * ACC_INT_SIZE;
        } else {
            argoff += va_slot(t, 0);
        }
    }
    next();                     /* the body's `{` */
    sym_set_params(fn, first, count);
    sym_set_flags(fn, SYMF_DECLARED | SYMF_PARAMS | SYMF_DEFINED
                      | (ext_func_variadic(x) ? SYMF_VARIADIC : 0));
    current_fn = fn;
    gen_func_begin(fn, count, ret);
    if (nstruct_params)
        struct_params_copy();
    in_body = 1;
    block();
    in_body = 0;
    labels_end();
    gen_func_end();
    sym_drop_locals();

    return 1;
}

/* What is at file scope: a function's definition, or a list of variables.
 * Which one shows only once the name has been read, by whether a '(' comes
 * next -- the type and the stars in front of the name are the same for
 * both. */
static void external_declaration(void)
{
    Type base, type;
    int line, count = 0, ext, bx;
    unsigned char bc;
    NameRef name;

    if (tok == TK_KW_TYPEDEF) {
        typedef_declaration();

        return;
    }

    /* At file scope static changes nothing for a program that is one file,
     * and extern declares what any declaration here does: storage that the
     * one that gives the value, if any does, fills in. inline asks that
     * calls be fast, and a call is what they are: C lets that be the
     * answer. In any order, as C allows. */
    decl_extern = 0;
    while (tok == TK_KW_STATIC || tok == TK_KW_EXTERN || tok == TK_KW_INLINE) {
        decl_extern |= tok == TK_KW_EXTERN;
        next();
    }
    if (tok == TK_KW_AUTO || tok == TK_KW_REGISTER)
        acc_error_at(tok_line, "%s is for a variable in a block, not at file "
                               "scope", tok_spelling(tok));
    base = base_type();
    bx = ext = base_ext;
    bc = base_const;
    line = tok_line;
    if (accept(TK_SEMI))
        return;
    /* A name and then '(' is a function -- a prototype, or a definition if
     * a body follows; anything else is a variable. */
    for (;;) {
        Type stars;

        line = tok_line;
        stars = declarator_stars(base);
        name = direct_declarator(stars, bx, &type, &ext, &count);
        decl_const = stars != base ? stars_const : bc;
        decl_bottom_const = bc ? SQ_CONST : 0;
        if (tok == TK_LPAREN) {
            if (count)
                acc_error_at(line, "a function cannot return an array");
            if (function_declarator(type, ext, name, line))
                return;
        } else if (type_is_func(type)) {
            if (function_from_type(ext, name, line))
                return;
        } else {
            global_variable(type, ext, name, count, line);
        }
        if (!accept(TK_COMMA))
            break;
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
        "usage: acc [-c] <source.c> -o <out> [-I <dir>]... [-b <addr>]\n"
        "                                    [-r <file>] [-x]\n"
        "       acc <file.o|lib.a>... -o <out.bin> [-x]\n"
        "       acc -a <lib.a> <file.o>...\n"
        "\n"
        "  -a  put the objects that follow into that library rather than\n"
        "      into a program. A link takes from a library only the\n"
        "      members it turns out to need.\n"
        "  -c  compile to an object rather than to a program, to be\n"
        "      linked with others later. An object also records what the\n"
        "      compile read, so that a build can tell whether it has to\n"
        "      be made again.\n"
        "  -I  a directory to look in for an #include, after the one the\n"
        "      including file is in.\n"
        "  -b  the address the image is loaded at, in hexadecimal. The\n"
        "      default is 40000, where MOS loads a program.\n"
        "  -r  write the addresses inside the image that -b moved, one\n"
        "      hexadecimal offset a line.\n"
        "  The program prints what main returned, as six hex digits.\n"
        "  -x  report it to IO port 0 instead, which stops an emulator\n"
        "      with the low byte as its exit status.\n");
    exit(2);
}

#if defined(AGONDEV) && defined(ACC_CYCLES)
#include <ez80f92.h>

/* How many cycles the compile takes, counted by the eZ80 itself: timer 1,
 * which MOS leaves alone, counting down from 65535 once every 256 cycles.
 * The seconds MOS reports come from a clock another thread of the emulator
 * keeps, and wander by a few percent from one sitting to the next; the
 * timer is stepped by the instructions the emulator runs, so the same
 * compile counts the same, give or take the interrupts that arrive while it
 * runs. One pass of it is 16.7 million cycles, about 0.9 s, which is longer
 * than any benchmark compile; one that took longer says so. */
static void cycles_start(void)
{
    IO(TMR1_CTL) = 0;
    IO(TMR1_RR_L) = 0xff;
    IO(TMR1_RR_H) = 0xff;
    IO(TMR1_CTL) = 0x0f;        /* on, reloaded now, clock / 256, one pass */
}

static void cycles_report(void)
{
    unsigned char ctl = IO(TMR1_CTL);
    unsigned lo = IO(TMR1_DR_L), hi = IO(TMR1_DR_H);

    if (!(ctl & 0x01) || (ctl & 0x80)) {
        printf("Cycles: over 16777216\r\n");

        return;
    }
    printf("Cycles: %lu\r\n", (0xffffUL - (hi << 8 | lo)) * 256);
}
#else
#define cycles_start()
#define cycles_report()
#endif

#if defined(AGONDEV) && defined(ACC_STACK)
/* How deep the stack goes, for sizing the room the linker script leaves it
 * above the heap.
 *
 * The reserve -- everything from the heap's top to the stack's bottom -- is
 * painted before the compile and read back after it. The lowest byte still
 * holding the pattern is as far down as the stack reached, give or take a
 * frame that wrote nothing. Nothing else is in that region: the heap stops
 * at ___heaptop, which is where the painting starts.
 *
 * Built by `make -f Makefile.agon STACK=1`. Not in the ordinary build: the
 * painting is a pass over 8 KB, and the report would be noise in front of
 * every compile. */
/* The linker's own symbols, whose names gain an underscore on the way from
 * C to the assembler: ___heaptop and __stack. */
extern char __heaptop[], _stack[];

#define STACK_PAINT 0x5a

static void stack_paint(void)
{
    char here;
    char *p;

    /* Up to a little below this frame, which is live. */
    for (p = __heaptop; p < &here - 64; p++)
        *p = STACK_PAINT;
}

static void stack_report(void)
{
    char *p;

    for (p = __heaptop; p < _stack; p++)
        if (*p != STACK_PAINT)
            break;

    printf("Stack: %u bytes of %u reserved\r\n",
           (unsigned) (_stack - p), (unsigned) (_stack - __heaptop));
}
#else
#define stack_paint()
#define stack_report()
#endif

/* ------------------------------------------------------------------ */
/* linking                                                             */

/* An input that is already compiled. By its name, as every other toolchain
 * tells them apart, and not by looking inside: a file called x.c that turns
 * out to hold an object is a mistake worth a clear complaint rather than a
 * clever recovery. */
static int ends_in(const char *path, char what)
{
    size_t n = strlen(path);

    return n > 2 && path[n - 2] == '.' && path[n - 1] == what;
}

#define is_object(path)  ends_in((path), 'o')
#define is_archive(path) ends_in((path), 'a')

/* A name from an object, as a symbol in the compiler's own table.
 *
 * The linker reuses what the compiler already has: a call whose target is not
 * known yet is a fixup, and gen_finish fills the fixups in once everything
 * has been read. That is the same problem a call to a function further down
 * the file is, so it is the same machinery -- an object is just a file whose
 * functions arrive all at once. */
static int link_symbol(const char *text, int flags)
{
    NameRef name = name_intern(text, (int) strlen(text));
    int sym = name_global(name);

    if (sym == SYM_NONE) {
        /* As the kind the object said, so that what is said about one that
         * nothing defines is about the right sort of thing -- and so that a
         * variable's "no address yet" is the -1 the compiler uses for it. */
        int func = (flags & OBJ_FUNC) != 0;

        sym = sym_push(name, func ? SYM_FUNC : SYM_GLOBAL, func ? 0 : -1);
        sym_set_flags(sym, SYMF_DECLARED | SYMF_PARAMS);
    }

    return sym;
}

/* One object, placed where the image has got to. */
static void place_object(Object *op, const char *path)
{
    Object o = *op;
    int base, bss, i;

    base = out_here();
    for (i = 0; i < o.text_len; i++)
        out_byte(o.text[i]);

    /* And room past the image for what it wants at zero. Which address that
     * comes to is not known until every object has been placed, so this is
     * only its place in the queue. */
    bss = gen_bss_reserve(o.bss_len);

    /* What it has, at the address it now has it. */
    for (i = 0; i < o.nsyms; i++) {
        int flags = obj_sym_flags(&o, i);
        int sym;

        if (!(flags & OBJ_DEFINED))
            continue;
        sym = link_symbol(obj_sym_name(&o, i), flags);
        if ((sym_flags(sym) & SYMF_DEFINED) || gen_bss_offset(sym) >= 0)
            acc_error("'%s' is defined in more than one object, and '%s' is "
                      "one of them", obj_sym_name(&o, i), path);
        if (flags & OBJ_BSS) {
            gen_bss_symbol(sym, bss + obj_sym_value(&o, i));

            continue;           /* its address comes with the rest of them */
        }
        sym_at(sym)->val = base + obj_sym_value(&o, i);
        sym_set_flags(sym, SYMF_DECLARED | SYMF_DEFINED | SYMF_PARAMS);
    }

    /* And the slots in it that hold an address. One inside the object moves
     * with it; one that wants a name from somewhere else becomes a fixup,
     * which gen_finish settles when every object has been read. */
    for (i = 0; i < o.nrelocs; i++) {
        int at = obj_reloc_at(&o, i), which = obj_reloc_sym(&o, i);

        if (at < 0 || at + 3 > o.text_len)
            acc_error("'%s' has a relocation at %06x, outside its %d bytes",
                      path, at, o.text_len);
        if (!which) {
            out_patch24(base + at, get24(o.text + at) + base);

            continue;
        }

        /* An offset into this object's bss, which is that far along the
         * queue of them -- and then the start of the whole bss, once every
         * object has been placed and there is one to add. */
        if (which == 1) {
            out_patch24(base + at, get24(o.text + at) + bss);
            gen_bss_fixup(base + at);

            continue;
        }

        /* Whatever the slot holds is the amount to add to the symbol's
         * address, and it is already in the image: gen_finish reads it back
         * when it fills the slot in. */
        gen_data_fixup(link_symbol(obj_sym_name(&o, which - 2),
                                   obj_sym_flags(&o, which - 2)), base + at);
    }
    obj_free(&o);
}

static void link_object(const char *path)
{
    Object o;

    obj_read(path, &o);
    place_object(&o, path);
}

/* A library, which is asked only for what the link is short of.
 *
 * Round and round until it has nothing more to offer: a member pulled in may
 * call something that nothing has called yet, and that something may be in
 * this same library. Which is why a link takes more than one look at a
 * library and only one at an object.
 *
 * A member is placed exactly as an object is, because it is one. */
static void link_archive(const char *path)
{
    Archive a;
    char *pulled;
    int again = 1;

    ar_open(path, &a);
    pulled = calloc((size_t) a.nmembers + 1, 1);
    if (!pulled)
        acc_error("out of memory for '%s'", path);

    while (again) {
        int i, n = gen_nfixups();

        again = 0;
        for (i = 0; i < n; i++) {
            int sym = gen_fixup_sym(i);
            Object o;
            int m;

            if (!gen_no_address(sym))
                continue;
            m = ar_find(&a, name_text(sym_at(sym)->name));
            if (m < 0 || pulled[m])
                continue;
            pulled[m] = 1;
            ar_member(&a, m, &o);
            place_object(&o, ar_member_name(&a, m));
            again = 1;
        }
    }
    free(pulled);
    ar_close(&a);
}

int main(int argc, char **argv)
{
    const char *in = NULL, *out = NULL, *relocs = NULL;
    const char **objs;
    int nobjs = 0, to_object = 0, to_archive = 0;
    int by_exit = 0;
    int i;
    clock_t begin;
    unsigned cs;

    objs = malloc((size_t) argc * sizeof *objs);
    if (!objs)
        acc_error("out of memory for the inputs");

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'o') {
            if (argv[i][2])
                out = argv[i] + 2;
            else if (++i < argc)
                out = argv[i];
            else
                usage();
        } else if (argv[i][0] == '-' && argv[i][1] == 'I') {
            if (argv[i][2])
                lex_add_include(argv[i] + 2);
            else if (++i < argc)
                lex_add_include(argv[i]);
            else
                usage();
        } else if (argv[i][0] == '-' && argv[i][1] == 'b') {
            const char *arg = argv[i][2] ? argv[i] + 2
                            : ++i < argc  ? argv[i] : NULL;
            char *end;
            long value;

            if (!arg)
                usage();
            value = strtol(arg, &end, 16);
            if (*end || value < 0 || value > 0xfe0000)
                acc_error("-b wants an address in hexadecimal, and '%s' is "
                          "not one", arg);
            out_base = (int) value;
        } else if (argv[i][0] == '-' && argv[i][1] == 'r') {
            if (argv[i][2])
                relocs = argv[i] + 2;
            else if (++i < argc)
                relocs = argv[i];
            else
                usage();
        } else if (argv[i][0] == '-' && argv[i][1] == 'c' && !argv[i][2]) {
            to_object = 1;
        } else if (argv[i][0] == '-' && argv[i][1] == 'a') {
            /* The library to make, named here rather than with -o, so that
             * what is being made is one word and reads left to right. */
            if (argv[i][2])
                out = argv[i] + 2;
            else if (++i < argc)
                out = argv[i];
            else
                usage();
            to_archive = 1;
        } else if (argv[i][0] == '-' && argv[i][1] == 'x' && !argv[i][2]) {
            by_exit = 1;
        } else if (argv[i][0] == '-') {
            usage();
        } else if (is_object(argv[i]) || is_archive(argv[i])) {
            objs[nobjs++] = argv[i];
        } else if (!in) {
            in = argv[i];
        } else {
            usage();
        }
    }
    if (!out || (!in && !nobjs) || (in && nobjs))
        usage();
    if (to_object && !in)
        usage();
    if (to_archive && (!nobjs || to_object))
        usage();

    begin = clock();
    stack_paint();
    cycles_start();

    name_init();
    lex_init();
    sym_init();
    gen_init();

    if (to_archive) {
        /* Nothing is compiled and nothing is linked: the objects are put
         * together with a list of what each of them defines in front. */
        ar_write(out, objs, nobjs);
    } else if (nobjs) {
        /* Linking. The entry stub goes in first, as it does for a program
         * compiled in one piece, and its call to main is a fixup like any
         * other -- which is what makes the objects' own symbols do the work
         * of finding it. */
        out_open(out, 1);
        gen_startup(by_exit);
        for (i = 0; i < nobjs; i++) {
            if (is_archive(objs[i]))
                link_archive(objs[i]);
            else
                link_object(objs[i]);
        }
        gen_finish();
        out_close();
    } else if (to_object && obj_current(out, in)) {
        /* Nothing to do: the object is there and every file it was made
         * from is unchanged. This is the whole point of an object knowing
         * what it was made from. */
        printf("%s is up to date\r\n", out);
    } else if (to_object) {
        /* Compiling to an object. No header and no entry stub: those belong
         * to a program, and this is a piece of one. Based at zero, so that
         * every address in it is an offset from its own first byte and
         * placing it is one addition. */
        gen_objects = 1;
        lex_want_deps();
        out_base = 0;
        out_open(out, 0);
        lex_open(in);
        translation_unit();
        lex_end();
        bss_end();
        gen_finish();
        lex_close();
        obj_write(out);
        if (relocs)
            out_relocs_write(relocs);
        out_free();
    } else {
        out_open(out, 1);
        gen_startup(by_exit);
        lex_open(in);
        translation_unit();
        lex_end();
        bss_end();
        gen_finish();
        lex_close();
        if (relocs)
            out_relocs_write(relocs);
        out_close();
    }

    /* Reported the way zap reports it, down to the wording, so that the two
     * halves of a build can be read as one number. Measured from after the
     * arguments are checked to after the file is written: everything a
     * "how long did that take" is asking about, and nothing else. */
    cs = elapsed_cs(begin, clock());
    cycles_report();
    stack_report();
    printf("Done in %u.%02u seconds\r\n", cs / 100, cs % 100);

    return 0;
}
