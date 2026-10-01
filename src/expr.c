/*
 * Expressions: primary through comma, calls, members, subscripts, taking
 * an address, strings, compound literals, casts, sizeof, offsetof and
 * va_arg -- each emitted as it is read.
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

static int  paren_name(void);

/* The function being compiled, for __func__ and for the variadic forms;
 * SYM_NONE between functions. */
int current_fn = SYM_NONE;
static void deref_rest(void);
static void conditional_rest(void);
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

/* The width the value being parsed is going straight into, when that is
 * narrower than an int -- the destination of an assignment or an initialiser.
 * Zero the rest of the time.
 *
 * It is what makes byte arithmetic legal: computing in eight bits is
 * indistinguishable from C's promote-then-truncate exactly when the result is
 * truncated to that width and nothing wider ever sees it. */
Type narrow_dest;


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
static int call_args_open(void);

static int call_args(void)
{
    int nargs = call_args_open();

    expect(TK_RPAREN, "')'");

    return nargs;
}

/* The arguments, up to the `)`, which is left the current token. */
static int call_args_open(void)
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
    if (tok != TK_RPAREN)
        expect(TK_RPAREN, "')'");

    return nargs;
}

#ifdef OPT_ACC
/* The arguments of a call whose body is read in place, up to the `)`: each
 * stored into its parameter's slot as it is made, rather than left on the
 * stack for the next to spill. A call with other than `n` of them is an
 * error, as it is of any function with a prototype. */
void call_args_stored(int fn, const int *slots, int n, int line, const char *spot)
{
    int first = sym_params_first(fn), nargs = 0;
    Type outer = narrow_dest;

    narrow_dest = 0;
    if (tok != TK_RPAREN)
        for (;;) {
            expr();
            if (nargs < n) {
                vstore_local(slots[nargs], sym_param_type(first, nargs));
                gen_discard();
            }
            nargs++;
            if (!accept(TK_COMMA))
                break;
        }
    narrow_dest = outer;
    if (tok != TK_RPAREN)
        expect(TK_RPAREN, "')'");
    if (nargs != n)
        error_before(line, spot, "'%s' takes %d argument%s, and this call "
                                 "gives it %d", name_text(sym_at(fn)->name),
                     n, n == 1 ? "" : "s", nargs);
}
#endif

/* A call through the pointer to a function on the stack, from just past
 * its `(`, which was at `spot` on `line`: an error about the call points at
 * what is called, the name before it. */
__attribute__((noinline))
static void call_value(int line, const char *spot)
{
    int x = vext(), nargs;

    if (vtype() != type_ptr_to(TY_FUNC))
        error_before(line, spot, "only a function or a pointer to one can be "
                                 "called");
    nargs = call_args();
    if (ext_func_declared(x) && nargs != ext_func_count(x)
        && !(nargs > ext_func_count(x) && ext_func_variadic(x)))
        error_before(line, spot, "this function takes %d argument%s, and this "
                                 "call gives it %d", ext_func_count(x),
                     ext_func_count(x) == 1 ? "" : "s", nargs);
    gen_call_indirect(nargs);
}

/* A variable's value, for a call through it: `fp(3)`. */
__attribute__((noinline))
static void call_variable(int sym, NameRef name, int line, const char *spot)
{
    const Sym *v = sym_at(sym);

    switch (v->kind) {
    case SYM_LOCAL:
    case SYM_LOCAL_CONST:
        vpush_local(v->val, v->type);
        break;
    case SYM_LOCAL_FAR:
        vaddr_array(v->val, v->type);
        vderef();
        break;
    case SYM_GLOBAL:
    case SYM_GLOBAL_CONST:
        global_address(sym);
        vderef();
        break;
    default:
        error_before(line, spot, "'%s' is not a function", name_text(name));
    }
    vset_ext(sym_at(sym)->ext);
    call_value(line, spot);
}

/* A call to a name, with the `(` after it the current token. */
static void call_rest(NameRef name)
{
    int fn = sym_find(name);
    int nargs, nparams;
    const struct Inline *inl;
    int line = tok_line;                /* the `(`, for an error */
    const char *spot = tok_at;

    next();

    /* Not declared: C99 took away the implicit declaration C89 gave such a
     * call, of a function returning int (6.5.1p2, 6.5.2.2), and a program
     * that relies on one is refused, as gcc 14 refuses it. */
    if (fn == SYM_NONE) {
        error_before(line, spot, "'%s' is called and not declared; C99 needs "
                                 "a declaration of a function before a call "
                                 "to it", name_text(name));
    } else if (sym_at(fn)->kind != SYM_FUNC) {
        call_variable(fn, name, line, spot);

        return;
    }

#ifdef OPT_ACC
    inline_count_call(fn);
#endif
    inl = (sym_flags(fn) & SYMF_INLINE) ? inline_usable(fn) : NULL;
#ifdef OPT_ACC
    if (inl && inl->body) {
        if (inline_body_call(inl, fn, line, spot))
            return;
        inl = NULL;
    }
#endif
    nargs = call_args_open();
    nparams = sym_nparams(fn);
    if (inl && nargs == nparams) {
        inline_expand(inl, fn);

        return;
    }
    expect(TK_RPAREN, "')'");
    if (nargs != nparams && (sym_flags(fn) & SYMF_PARAMS)
        && !(nargs > nparams && (sym_flags(fn) & SYMF_VARIADIC)))
        error_before(line, spot, "'%s' takes %s%d argument%s, and this call "
                                 "gives it %d", name_text(name),
                     sym_flags(fn) & SYMF_VARIADIC ? "at least " : "", nparams,
                     nparams == 1 ? "" : "s", nargs);
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
void reserved_word(void)
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
        acc_error_prev("a pointer can be %d deep and this is deeper",
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
/* Neither side of a subscript a pointer, said of the one on the left, at
 * the `[`. */
__attribute__((noinline))
static void subscript_refused(int line, const char *spot)
{
    acc_error_spot(line, spot, "'[' needs an array or a pointer, and this "
                               "is %s",
                   type_float(vtype_at(1)) ? "a floating-point value"
                                           : "an integer");
}

static void subscript(void)
{
    Type outer = narrow_dest;
    int left = type_pointer(vtype()) != 0, line = tok_line;
    const char *spot = tok_at;

    next();
    narrow_dest = 0;            /* the index is not the destination */
    comma_expr();
    narrow_dest = outer;
    expect(TK_RBRACKET, "']'");

    /* `a[i]` is `*(a + i)`, and so is `i[a]` (C99 6.5.2.1): one of the two
     * is the pointer, and the addition takes it on either side. */
    if (!left && !type_pointer(vtype()))
        subscript_refused(line, spot);
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

/* The member of record x called `name`, looked for through its anonymous
 * members too, with *offset its place from the start of x. -1 if there is
 * none. `*top` is the member of x itself that holds it: the anonymous one,
 * when it is inside one, which is where a designated initialiser goes on
 * from. */
int member_lookup(int x, NameRef name, int *offset, int *top)
{
    int m;

    for (m = member_first(x); m >= 0; m = member_next(m)) {
        if (member_name(m) == name) {
            *offset = member_offset(m);
            *top = m;

            return m;
        }
        if (member_name(m) == NAME_NONE && type_is_struct(member_type(m))) {
            int inner_top, found = member_lookup(member_ext(m), name, offset,
                                                 &inner_top);

            if (found >= 0) {
                *offset += member_offset(m);
                *top = m;

                return found;
            }
        }
    }

    return -1;
}

/* `.name` or `->name`, with the struct's address on the stack and the
 * operator not yet read: the member's address in its place. */
__attribute__((noinline))
static void member(void)
{
    int line = tok_line, arrow = (tok == TK_ARROW), x = vext(), m, offset, top;
    const char *spot = tok_at;
    Type type = vtype();
    NameRef name;

    next();
    if (type != type_ptr_to(TY_STRUCT))
        acc_error_spot(line, spot,
                       arrow ? "'->' needs a pointer to a struct or union"
                             : "'.' needs a struct or union");
    record_complete(x, line, spot);
    if (tok != TK_IDENT)
        acc_error_at(tok_line, "expected a member's name, found %s",
                     tok_spelling(tok));
    name = tok_name;
    m = member_lookup(x, name, &offset, &top);
    if (m < 0)
        acc_error_at(tok_line, "'%s' has no member '%s'", record_name(x),
                     name_text(name));
    next();
    vmember(offset, member_type(m), member_ext(m), member_quals(m));
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
            int line = tok_line;
            const char *spot = tok_at;

            if (object == POST_OBJECT)
                vderef();
            next();
            call_value(line, spot);
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
        acc_error_prev("'%s' is not declared", name_text(name));
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
    case SYM_LOCAL_FAR:
        /* A local past what (ix+d) reaches: it lives where the arrays do,
         * so what a use of it starts from is its address. */
        vaddr_array(s->val, s->type);
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
#ifdef OPT_ACC
        inline_count_address(sym);
#endif
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
        acc_error_prev("'%s' is not a variable", name_text(name));
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
        acc_error_prev("'%s' is not declared", name_text(name));

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

        /* What `p++` leaves is a value, and a value can be gone on with:
         * `f++->at` is the member of where f pointed, and the chain reads
         * left to right. Stopping here made that `expected ';', found '->'`,
         * which acc's own code generator says twice.
         *
         * Through subscript_value, which is what every other value with a
         * chain after it uses: the chain can end holding a place rather than
         * what is in it, and then the place has to be read. Calling the
         * chain alone assigned the address of the member instead. */
        subscript_value();

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
static void prefix_operand(int op, const char *spelling);

__attribute__((noinline))
static void prefix_step(void)
{
    int op = (tok == TK_INC) ? TK_PLUS : TK_MINUS;
    const char *spelling = tok_spelling(tok);

    next();
    prefix_operand(op, spelling);
}

/* What `++` or `--` changes: a name, what a pointer points at, or either
 * in parentheses -- `--(*p)`, which Csmith writes. A postfix after the `)`
 * would bind to what is inside first, and is refused rather than read the
 * wrong way round. */
static void prefix_operand(int op, const char *spelling)
{
    if (tok == TK_LPAREN) {
        int line = tok_line;
        const char *spot = tok_at;

        next();
        prefix_operand(op, spelling);
        expect(TK_RPAREN, "')'");
        if (tok_postfix())
            acc_error_spot(line, spot, "%s of a parenthesis with a subscript "
                                       "or a member after it is more than acc "
                                       "reads", spelling);

        return;
    }

    if (tok == TK_IDENT) {
        NameRef name = tok_name;
        int sym = sym_find(name);
        int line = tok_line;
        const char *spot = tok_at;

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
        acc_error_spot(line, spot, "'%s' cannot be changed by %s",
                       name_text(name), spelling);
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
        } else if (tok == TK_ASSIGN || compound_op[tok]) {
            /* `(*p) = x` and `(*p) += x`, which are `*p = x` and `*p += x`
             * with a parenthesis round the left -- and C lets one be put
             * there. The address is on the stack either way, so what
             * follows is what follows a star. */
            deref_rest();
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
/* What address_of_operand left on the stack: an object's address, a whole
 * array's, or the value a cast made -- which is not an address at all, and
 * is only of use to a member or a subscript written after it. */
#define ADDR_OBJECT 0
#define ADDR_ARRAY  1
#define ADDR_VALUE  2

__attribute__((noinline))
static int  address_of_literal(int line, const char *spot);
static int  address_of_operand(void);
static int  string_address(void);
static Type joined_elem(void);

static void address_of(void)
{
    int line = tok_line;
    const char *spot = tok_at;

    next();
    if (address_of_operand() == ADDR_VALUE)
        acc_error_spot(line, spot, "'&' takes the address of a variable, and "
                                   "a cast has none");
}

/* What `&` is being applied to, with the `&` already read. Answers whether
 * what it left is a whole array's address rather than an object's, which is
 * a difference to whatever follows a parenthesis round it. */
static int address_of_operand(void)
{
    NameRef name;
    int sym, line;
    const char *spot;

    /* `&(struct s){ ... }`: a compound literal is an object, and this is
     * the address of the one it makes. A parenthesis round anything else is
     * handled there too, since the two are told apart by what follows the
     * `(`. Out of line, next to the rest of the literals: starts_decl has to
     * stay inlined into its callers, and a call to it from here is one more
     * of those on a path that is rare. */
    if (tok == TK_LPAREN)
        return address_of_literal(tok_line, tok_at);


    /* `&*p`: the address of what a pointer leads to is the pointer, so the
     * two cancel and what is left is the pointer. The stars are counted the
     * way a dereference counts them -- all but the last is a read -- and
     * what binds tighter than the `*` has already been read by primary(),
     * so `&*p[i]` is the address of p[i] and not of p. */
    if (tok == TK_STAR) {
        int stars = 0;

        while (accept(TK_STAR))
            stars++;
        primary();
        while (--stars != 0)
            vderef();

        return ADDR_OBJECT;
    }

    /* `&"x"[1]`, and `&"x"` itself. A string literal is an array with static
     * storage, so it has an address like any other object. The bytes are
     * written as the address of the first, so a subscript after it is walked
     * for the element's address and stops short of reading it -- which is
     * the one thing string_value() does that is not wanted here.
     *
     * With nothing after it the answer is the whole array: the same three
     * bytes, typed as a pointer to all of them, exactly as a named array is
     * below. */
    if (tok == TK_STRING) {
        int len = string_address();

        if (tok_postfix()) {
            postfix_chain(POST_VALUE);

            return ADDR_OBJECT;
        }
        vset_type(type_ptr_to(TY_EXT), ext_array(joined_elem(), 0, len + 1));

        return ADDR_ARRAY;
    }

    if (tok != TK_IDENT)
        acc_error_at(tok_line, "'&' takes the address of a variable, and "
                               "this is %s", tok_spelling(tok));
    name = tok_name;
    line = tok_line;
    spot = tok_at;
    spot = tok_at;
    sym = sym_find(name);
    next();
    switch (name_operand(sym, name)) {
    case NAME_LOCAL: {
        const Sym *local = sym_at(sym);

        if (local->quals & SQ_REGISTER)
            acc_error_spot(line, spot, "'%s' is declared register, so it has "
                                       "no address to take", name_text(name));
        vaddr_local(local->val, local->type);
        vset_ext(local->ext);
        vset_quals(local->quals);

        return ADDR_OBJECT;
    }
    case NAME_OBJECT:
        if (vbits())
            acc_error_spot(line, spot, "a bit-field has no address to take");

        return ADDR_OBJECT;     /* the address is what is wanted */
    case NAME_CONST:
        acc_error_spot(line, spot, "'%s' is a constant, which has no address",
                       name_text(name));
    case NAME_READONLY:
        vdrop();
        readonly_address(sym);

        return ADDR_OBJECT;
    case NAME_FUNC:
        return ADDR_OBJECT;     /* `&f` is f's address, as `f` is */
    case NAME_RESULT:
        acc_error_spot(line, spot, "what a call comes to has no address to "
                                   "take");
    }

    /* An array whose length was worked out as the program ran is already
     * its own address: `&a` and `a` are the same three bytes, and the type
     * C gives the first -- a pointer to an array of n -- is not one acc has
     * a way of writing down. */
    if (sym_at(sym)->kind == SYM_LOCAL_VLA)
        return ADDR_OBJECT;

    /* A whole array: the same address as its first element, as a pointer to
     * the array, which steps over all of it at once. One with no size yet,
     * `extern int a[];`, is a pointer to an array of unknown size, which
     * has no step to take but is an address like any other. */
    {
        const Sym *array = sym_at(sym);

        vset_type(type_ptr_to(TY_EXT),
                  ext_array(array->type, array->ext, sym_count(sym)));
    }

    return ADDR_ARRAY;
}

/* Adjacent string literals, joined as C joins them: "ab" "cd" is "abcd".
 * The lexer's buffer is its own and reused for each token, so they are
 * gathered here. */
char *str_joined;
static int   str_joined_cap;
int   str_joined_wide;   /* whether what was gathered is wchar_t */

static void joined_room(int len)
{
    while (len >= str_joined_cap)
        str_joined_cap = str_joined_cap ? str_joined_cap * 2 : 128;
    str_joined = realloc(str_joined, (size_t) str_joined_cap);
    if (!str_joined)
        acc_error("out of memory for a string");
}

/* Narrow bytes made wide in place, as C99 6.4.5 has a narrow literal joined
 * to a wide one read as though it were wide: its UTF-8 taken apart into
 * characters, one wchar_t each -- two, the halves of a surrogate pair, past
 * 0xffff. `len` bytes at `at`; answers how many bytes they became.
 *
 * Unless an escape in them gave a byte past 0x7f: "\343\201\202" L"" is
 * three wide characters to gcc and clang, as L"\343\201\202" would be, and
 * not the one their UTF-8 spells. The lexer says only that there was such
 * an escape, not which bytes it gave, so then every byte is one. */
static int joined_widen(int at, int len, int bytes)
{
    unsigned char *narrow = malloc((size_t) len + 1);
    int i = 0, n = at;

    if (!narrow)
        acc_error("out of memory for a string");
    memcpy(narrow, str_joined + at, (size_t) len);
    joined_room(at + 4 * len + 4);
    while (i < len) {
        unsigned long v = narrow[i++];
        int more = bytes ? 0 : v >= 0xf0 ? 3 : v >= 0xe0 ? 2 : v >= 0xc0 ? 1 : 0;

        if (more) {
            v &= 0x3fUL >> more;
            while (more-- && i < len)
                v = v << 6 | (narrow[i++] & 0x3fUL);
        }
        if (v > 0xffff) {
            unsigned long hi = 0xd800 + ((v - 0x10000) >> 10);
            unsigned long lo = 0xdc00 + ((v - 0x10000) & 0x3ff);

            str_joined[n++] = (char) (hi & 0xff);
            str_joined[n++] = (char) (hi >> 8);
            v = lo;
        }
        str_joined[n++] = (char) (v & 0xff);
        str_joined[n++] = (char) (v >> 8);
    }
    free(narrow);

    return n - at;
}

int string_gather(void)
{
    int len = 0, escaped;

    str_joined_wide = 0;
    escaped = 0;
    while (tok == TK_STRING) {
        int at = len;

        if (len + tok_str_len >= str_joined_cap)
            joined_room(len + tok_str_len);
        /* Asked, because `""` is a string of no bytes and the lexer has
         * nothing to point at for it -- and memcpy from a null pointer is
         * undefined even when it is told to copy nothing. */
        if (tok_str_len)
            memcpy(str_joined + len, tok_str, (size_t) tok_str_len);
        len += tok_str_len;

        /* "a" L"b" is as wide as L"a" L"b", whichever side is wide. */
        if (tok_str_wide && !str_joined_wide) {
            len = joined_widen(0, at, escaped) + tok_str_len;
            memcpy(str_joined + len - tok_str_len, tok_str,
                   (size_t) tok_str_len);
            str_joined_wide = 1;
        } else if (!tok_str_wide && str_joined_wide) {
            len = at + joined_widen(at, tok_str_len, tok_str_escaped);
        } else if (!tok_str_wide) {
            escaped |= tok_str_escaped;
        }
        next();
    }

    return len;
}

/* The type of one element of what was gathered, and how many elements its
 * `len` bytes are. */
static Type joined_elem(void)
{
    return str_joined_wide ? TY_SHORT : TY_CHAR;
}

int joined_count(int len)
{
    return str_joined_wide ? len / 2 : len;
}

/* What was gathered, written into the image with its terminator: gen_data
 * ends what it writes with one zero byte, and a wide string's terminator is
 * a wchar_t, so it is given the other. Answers where. */
int joined_data(int len)
{
    if (!str_joined_wide)
        return gen_data(str_joined, len);
    if (len + 1 >= str_joined_cap)
        joined_room(len + 1);
    str_joined[len] = 0;

    return gen_data(str_joined, len + 1);
}

static int string_address(void)
{
    int len = string_gather();

    vpush_const(joined_data(len), type_ptr_to(joined_elem()));
    vset_addr();

    return joined_count(len);
}

/* A string literal as an operand: an array of char, which is the address of
 * its first character, and then the subscript that may follow it -- which
 * ends in reading the byte, this being a value that is wanted. */
__attribute__((noinline))
static void string_value(void)
{
    string_address();
    if (tok_postfix())
        subscript_value();
}

static void cast_rest(void);
static int  cast_rest_as(int statement);
static void cast_operand(Type to, int x, int quals);
static void sizeof_value(void);
static void offsetof_value(void);
static void compound_literal(Type type, int x, int count, Type elem,
                             int elem_x, int line, const char *spot,
                             int address, int *countp);
static void va_form(void);

/* A name used as a value: a local read, or a call. */
void primary(void)
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
        if (tok_low == TK_IDENT || tok_low == TK_LPAREN) {
            /* An assignment to it is not this operand's to make: `a + (x) =
             * 1` has no meaning, and the `=` left over says so. A step is. */
            if (paren_name())
                object_value();
            narrow_dest = outer;
            if (tok_postfix() || tok == TK_LPAREN)
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

        if (tok == TK_LPAREN) {
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

    if (tok == TK_KW_OFFSETOF) {
        offsetof_value();

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

/* Out of line: inlined, it gave binary_rest a frame of 54 bytes, built
 * for every operator in the program. */
__attribute__((noinline))
static void logical_rest(int op, int op_prec)
{
    int settles = (op == TK_OROR);      /* the truth that decides it early */
    Type outer = narrow_dest;
    int early, left, right;
    Type left_type, right_type;

    /* Both sides known while compiling, which C says is a constant
     * expression and acc did not: `1 && 1` was refused as an initial value
     * and as an array's size, and acc's own header asserts with one -- which
     * is what stopped acc compiling itself.
     *
     * Folded here and not in the code generator because a short circuit is a
     * branch, and a branch is not a constant. The left is taken off first so
     * that the mark, and the rolling back of it, describe the stack without
     * it. */
    if (vconst_top(&left, &left_type) && !type_float(left_type)) {
        GenMark mark;

        vdrop();
        gen_mark(&mark);
        narrow_dest = 0;
        primary();
        binary_rest(op_prec + 1);
        narrow_dest = outer;

        if (vconst_top(&right, &right_type) && !type_float(right_type)) {
            gen_rollback(&mark);        /* neither side wanted any code */
            vpush_const(op == TK_OROR ? (left || right) : (left && right),
                        TY_INT);

            return;
        }

        /* The left settles it on its own, so the right is not evaluated and
         * whatever working it out took goes away with it. */
        if (settles ? left != 0 : left == 0) {
            gen_rollback(&mark);
            vpush_const(settles ? 1 : 0, TY_INT);

            return;
        }

        /* It does not settle it, so the answer is the right as a one or a
         * nought -- and there is no branch, because there is nothing left to
         * skip over. */
        vtruth(TK_NE);

        return;
    }

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
void binary_rest(int min_prec)
{
    for (;;) {
        unsigned char op = tok_low;     /* a byte: tok_pair on an int held */
        int op_prec = prec[op];         /* in a register was a 24-bit AND */

        /* PREC_NONE included: not an operator. Unsigned, since neither is
         * ever negative and a signed compare is a call on this chip. */
        if ((unsigned) op_prec < (unsigned) min_prec)
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
        acc_error_prev("'%s' is not declared", name_text(name));
    if (sym_at(sym)->kind != SYM_LOCAL)
        acc_error_prev("'%s' cannot be assigned to", name_text(name));

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
static void object_rest(int what, NameRef name);

static void object_statement(int sym, NameRef name)
{
    object_rest(name_operand(sym, name), name);
}

/* What follows a name that name_operand has read, by what it turned out to
 * be: for an object, a store, a compound store, a step, or a read and the
 * rest; for anything else, the rest. */
static void object_rest(int what, NameRef name)
{
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

/* A statement that starts with a cast or a compound literal. The literal
 * is an object, and what follows may assign to it or step it. Out of line,
 * where it costs the statements that start with a parenthesis nothing. */
__attribute__((noinline))
static void cast_statement(void)
{
    if (!cast_rest_as(1)) {
        binary_rest(PREC_LOWEST);
    } else if (tok == TK_INC || tok == TK_DEC) {
        object_value();                 /* `(int){3}++` */
        binary_rest(PREC_LOWEST);
    } else {
        deref_rest();                   /* `(int){3} = 5` */
    }
}

static void paren_statement(void)
{
    Type outer = narrow_dest;

    next();
    if (starts_decl()) {
        cast_statement();

        return;
    }
    narrow_dest = 0;
    if (tok_low == TK_IDENT || tok_low == TK_LPAREN) {
        int object = paren_name();

        narrow_dest = outer;
        if (!object) {
            postfix_statement();
        } else if (tok == TK_INC || tok == TK_DEC) {
            object_value();             /* `(p->n)++` */
            binary_rest(PREC_LOWEST);
        } else {
            deref_rest();               /* `(x) = v`, `(p->n) += 2` */
        }

        return;
    }
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

/* What follows a name at the start of an expression, the name read: an
 * assignment to it, a call, or a read and the rest of the expression.
 * Inlined into both callers -- assignment, and a parenthesis that opens with
 * a name -- since as a call it would be one more on every statement that
 * begins with a name. */
static inline __attribute__((always_inline))
void name_rest(NameRef name)
{
    int sym;

    if (tok == TK_LPAREN) {
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
        int dest = sym, line = tok_line;
        const char *spot = tok_at;      /* the `=`, for an error */
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
                error_before(line, spot, "'%s' cannot be assigned to",
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

/* The rest of what a parenthesis holds, from a point where the first
 * assignment in it is done: a `?:`, the commas, and the `)`. What expr and
 * comma_expr do after assignment(). */
static void paren_rest(void)
{
    if (tok == TK_QUESTION)
        conditional_rest();
    if (tok == TK_ASSIGN || compound_op[tok] || tok == TK_INC || tok == TK_DEC)
        acc_error_at(tok_line, "the left of %s is not something that can be "
                               "assigned to", tok_spelling(tok));
    while (tok == TK_COMMA) {
        next();
        gen_discard();
        expr();
    }
    expect(TK_RPAREN, "')'");
}

/* A parenthesis that opens with a name, from the name: `(x)`, `(p->n)`,
 * `(x + 1)`. A parenthesis round an object leaves it an object, as C says,
 * and macros put one round nearly everything they name -- `#define N (v)`
 * and then `N++`, `N = 0`. Read to a value, as everything in a parenthesis
 * was, there was nothing left for the `++` or the `=` to change.
 *
 * Answers 1 with the object's address on the stack, the `)` read, when an
 * assignment follows the `)` -- the one thing a caller has to do itself. A
 * `++`, a `--` or a subscript after it is done here, and then, as for
 * anything else the parenthesis held, 0 with the value on the stack. */
/* After the `)` and any chain after it: 1, the object left for the caller,
 * when what follows needs one -- an assignment, a step, or the `)` of a
 * parenthesis round this one, whose own caller decides; otherwise its value,
 * and 0. `object` says whether there is an object at all rather than a
 * value, which there is not after `(f)(x)`. */
static int paren_object_rest(int object)
{
    if (!object)
        return 0;
    if (tok == TK_ASSIGN || compound_op[tok] || tok == TK_INC
        || tok == TK_DEC || tok == TK_RPAREN)
        return 1;
    object_value();

    return 0;
}

__attribute__((noinline))
static int paren_name(void)
{
    NameRef name = tok_name;
    int sym, what;

    /* `((p)->n)`, `((x))`: a parenthesis round one, whose object survives
     * its `)` for this one's to decide about. 20060910-1 steps
     * `*((deeper)->buffer_position)++`. */
    if (tok == TK_LPAREN) {
        int object;

        next();
        if (starts_decl()) {            /* `((int) x + 1)`, a value */
            cast_rest();
            binary_rest(PREC_LOWEST);
            paren_rest();

            return 0;
        }
        if (tok_low == TK_IDENT || tok_low == TK_LPAREN) {
            object = paren_name();
        } else if (tok == TK_STAR) {
            /* `((*p) = 1)`, `((*p)++)`: what is done to it is done there,
             * and a value is left. Csmith writes these by the thousand. */
            paren_deref_step();
            object = 0;
        } else {
            comma_expr();
            expect(TK_RPAREN, "')'");
            object = 0;
        }
        if (tok_postfix())
            object = postfix_chain(object ? POST_OBJECT : POST_VALUE)
                     == POST_OBJECT;
        if (tok == TK_RPAREN) {         /* it was all of this one */
            next();
            if (tok_postfix())
                object = postfix_chain(object ? POST_OBJECT : POST_VALUE)
                         == POST_OBJECT;

            return paren_object_rest(object);
        }
        if (object && tok != TK_INC && tok != TK_DEC) {
            deref_rest();               /* `((x) = 5)` */
        } else {
            if (object)
                object_value();
            binary_rest(PREC_LOWEST);
        }
        paren_rest();

        return 0;
    }

    next();
    if (tok == TK_RPAREN) {             /* `(x)`: the name is all of it */
        next();
        sym = sym_find_value(name);

        /* `(p)[i] = v`, `(s).m += 2`: the chain after the `)` is the
         * name's, and what it ends at may be assigned to. */
        if (tok_postfix())
            return paren_object_rest(name_operand(sym, name) == NAME_OBJECT);
        if (tok != TK_ASSIGN && !compound_op[tok] && tok != TK_RPAREN) {
            symbol_value(sym, name);    /* a `++` after it too */

            return 0;
        }
        if (sym != SYM_NONE && sym_at(sym)->kind == SYM_LOCAL) {
            const Sym *local = sym_at(sym);

            vaddr_local(local->val, local->type);
            vset_ext(local->ext);
            vset_quals(local->quals);

            return 1;
        }
        what = name_operand(sym, name);

        /* A const one is a value, which is all the parenthesis round it
         * wants, `((void *)(v))`; only an assignment to it is wrong. */
        if (what == NAME_READONLY && tok != TK_RPAREN)
            acc_error_at(tok_line, "'%s' is const, so it cannot be changed",
                         name_text(name));

        return what == NAME_OBJECT;
    }
    if (tok_postfix()) {                /* `(p->n)`, `(a[i])` */
        what = name_operand(sym_find_value(name), name);
        if (what == NAME_OBJECT && tok == TK_RPAREN) {
            next();

            return paren_object_rest(!tok_postfix()
                                     || postfix_chain(POST_OBJECT)
                                        == POST_OBJECT);
        }
        object_rest(what, name);
    } else {
        name_rest(name);
    }
    paren_rest();

    return 0;
}

static void assignment(void)
{
    if (tok == TK_LPAREN) {
        paren_statement();

        return;
    }

    if (tok == TK_IDENT) {
        NameRef name = tok_name;

        /* Look one token ahead by remembering this one: an identifier
         * followed by '=' is an assignment, anything else is a value. */
        next();
        name_rest(name);

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
void expr(void)
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
void comma_expr(void)
{
    expr();
    while (tok == TK_COMMA) {
        next();
        gen_discard();
        expr();
    }
}

/* The third operand of `?:`, which may itself be a conditional -- `a ? b : c ?
 * d : e` groups to the right -- but not an assignment: C reads `a ? b : c = d`
 * as an assignment to the whole conditional, which cannot be assigned to. */
void conditional(void)
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
    int slot, lock, to_third, to_stub, middle_null, middle_ext;
    int cond;
    Type cond_type;

    /* The condition is known while compiling, so one side is the answer and
     * the other is not evaluated. C says that is a constant expression; acc
     * used to generate both paths and a branch between them, which is not,
     * and its own header asserts with `? 1 : -1`.
     *
     * Both sides are still parsed, because the tokens have to be read either
     * way. Only one is kept: the other's code is rolled back, which is what
     * makes it unevaluated rather than merely unused.
     *
     * The answer takes the type of the side that was kept and not the type
     * the two sides have in common, which for `1 ? 2 : 3L` is long. The
     * value is the same and what a constant expression is asked for is the
     * value; a program that can tell the difference is asking sizeof about a
     * conditional, which acc does not do. */
    if (vconst_top(&cond, &cond_type) && !type_float(cond_type)) {
        GenMark before_middle, after_middle;
        Type kept;
        int kept_ext;

        next();
        vdrop();
        gen_mark(&before_middle);
        narrow_dest = 0;
        comma_expr();
        expect(TK_COLON, "':'");
        kept = vtype();                 /* noted before either is rolled back */
        kept_ext = vext();

        if (cond) {
            gen_mark(&after_middle);
            conditional();
            gen_cond_same(kept, kept_ext);
            gen_rollback(&after_middle);
        } else {
            gen_rollback(&before_middle);
            conditional();
            gen_cond_same(kept, kept_ext);
        }
        narrow_dest = outer;

        return;
    }

    next();
    to_third = gen_cond_begin(&slot, &lock);

    narrow_dest = 0;
    comma_expr();
    expect(TK_COLON, "':'");

    /* Both sides void makes the whole of it void, and there is nothing to
     * carry across the join. */
    if (vtype() == TY_VOID) {
        to_stub = gen_cond_middle_void();
        gen_label(to_third);
        conditional();
        gen_cond_end_void(to_stub, lock);
        narrow_dest = outer;

        return;
    }

    to_stub = gen_cond_middle(&slot, &middle, &middle_ext, &middle_null);

    gen_label(to_third);
    conditional();
    gen_cond_end(to_stub, slot, lock, middle, middle_ext, middle_null);
    narrow_dest = outer;
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
                             int elem_x, int line, const char *spot,
                             int address, int *countp)
{
    int array;

    /* At file scope there is no frame to put it in, so it is static, as C99
     * says: its bytes are built the way a global's are and left in the
     * image, and what the literal comes to is where they went. That half is
     * further down, where the machinery for a global's bytes is. */
    if (!in_body) {
        literal_bytes(type, x, count, elem, elem_x, line, spot, address,
                      countp);

        return;
    }

    if (count) {
        array = local_array_object(elem, elem_x, &count, line, spot, 1);
        vaddr_array(array, elem);       /* an array is its first element's
                                         * address, address or not */
        vset_ext(elem_x);
        if (countp)
            *countp = count;

        return;
    }

    if (type_is_struct(type)) {
        array = local_struct_object(x, line, spot, 1);
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
            acc_error_spot(line, spot, "a compound literal needs a value");
        narrow_dest = type_narrow(type);
        expr();
        narrow_dest = outer;
        vstore_local(off, type);
        gen_discard();
        gen_value_end();
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
static int address_of_literal(int line, const char *spot)
{
    int x, count, elem_x, quals;
    Type elem, type;

    next();

    /* Not a type after the `(`, so this is `&(x)` -- a parenthesis round
     * what the address is being taken of, which C allows and which says
     * nothing beyond where the operand ends. */
    if (!starts_decl()) {
        int kind;

        /* `&((&a[1])->m)`, `&(p + 1)->m`: what is inside is a value, a
         * pointer, and the chain after the `)` is what makes an object of
         * it. address_of_operand reads the operands `&` can take -- a name,
         * a `*`, a string, another parenthesis -- and past one of those, or
         * in place of one, the rest is read as the value it is. */
        if (tok != TK_IDENT && tok != TK_STAR && tok != TK_STRING
            && tok != TK_LPAREN) {
            comma_expr();
            kind = ADDR_VALUE;
        } else {
            kind = address_of_operand();
        }
        if (tok != TK_RPAREN) {
            if (kind == ADDR_ARRAY)
                vset_type(type_ptr_to(ext_elem(vext())), ext_elem_x(vext()));
            else if (kind == ADDR_OBJECT)
                object_value();
            binary_rest(PREC_LOWEST);
            if (tok == TK_QUESTION)
                conditional_rest();
            while (accept(TK_COMMA)) {
                vdrop();
                expr();
            }
            kind = ADDR_VALUE;
        }
        expect(TK_RPAREN, "')'");

        /* `&(*p)[i]`, `&(*p).m`: the parenthesis ended the operand, and the
         * subscripts and members after it bind to what was inside. What is
         * on the stack is that object's address, which is where the chain
         * starts from -- unless it was a whole array or a cast, which are
         * values to walk from rather than objects to read. */
        if (tok_postfix()) {
            if (kind == ADDR_ARRAY)
                vset_type(type_ptr_to(ext_elem(vext())), ext_elem_x(vext()));
            postfix_chain(kind == ADDR_OBJECT ? POST_OBJECT : POST_VALUE);

            return ADDR_OBJECT;
        }
        if (kind == ADDR_VALUE)
            acc_error_spot(line, spot, "'&' takes the address of a variable, "
                                       "and what is in the parenthesis is a "
                                       "value");

        return kind;
    }
    type = type_name_elem(&x, &count, &elem, &elem_x);
    quals = base_const ? VQ_CONST : 0;
    expect(TK_RPAREN, "')'");

    /* `&((struct s *) p)->m`, `&((char *) p)[i]`: a cast, and not a compound
     * literal. The cast itself has no address -- it is a value, and C says
     * so -- but the member or the element it leads to is an object like any
     * other, and taking the address of one of those is what this is for.
     * What follows the parenthesis is read by the caller, which is where the
     * `)` that closes it is; this says only that a value was left. */
    if (tok != TK_LBRACE) {
        if (count < 0)
            acc_error_spot(line, spot, "an array type here needs its size");
        if (type_is_array(type))
            acc_error_spot(line, spot, "a cast cannot make an array");
        cast_operand(type, x, quals);

        return ADDR_VALUE;
    }
    compound_literal(type, x, count, elem, elem_x, line, spot, 1, NULL);

    /* `&(int []){ 0, 1, 2 }[i]`, `&(struct s){ ... }.m`: what follows binds
     * to the literal, before the `&` does. An array's literal is already
     * its first element's address, a value to walk from; a struct's is the
     * object itself. */
    if (tok_postfix())
        postfix_chain(count ? POST_VALUE : POST_OBJECT);

    return ADDR_OBJECT;
}

/* `(type) operand` and `(type){ values }`, from just past the parenthesis:
 * a cast, or a compound literal, which the brace after the `)` tells
 * apart. A cast binds as the unary operators do, so its operand is one of
 * those or a postfix expression, which is what primary() reads. */
__attribute__((noinline))
static int cast_rest_as(int statement)
{
    int x, line = tok_line, count, elem_x;
    const char *spot = tok_at;
    Type elem;
    Type to = type_name_elem(&x, &count, &elem, &elem_x), outer = narrow_dest;
    int quals = base_const ? VQ_CONST : 0;

    expect(TK_RPAREN, "')'");

    /* At the start of a statement a literal that is not an array is left
     * as its address: it is an object (C99 6.5.2.5p4), and `(int){3} = 5`
     * assigns to it. */
    if (tok == TK_LBRACE && statement && !count) {
        narrow_dest = 0;
        compound_literal(to, x, count, elem, elem_x, line, spot, 1, NULL);
        narrow_dest = outer;

        return !tok_postfix() || postfix_chain(POST_OBJECT) == POST_OBJECT;
    }
    if (tok == TK_LBRACE) {
        narrow_dest = 0;
        compound_literal(to, x, count, elem, elem_x, line, spot, 0, NULL);
        narrow_dest = outer;
        if (tok_postfix())
            subscript_value();

        return 0;
    }
    if (count < 0)
        acc_error_spot(line, spot, "an array type here needs its size");
    if (type_is_array(to))
        acc_error_spot(line, spot, "a cast cannot make an array");
    cast_operand(to, x, quals);

    return 0;
}

static void cast_rest(void)
{
    cast_rest_as(0);
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
        /* `sizeof (x = y)`: the type is x's, and nothing is done, so the
         * right side is read only to be passed over. */
        if (what == SIZEOF_OBJECT && (tok == TK_ASSIGN || compound_op[tok])) {
            next();
            expr();
            vdrop();
            if (tok == TK_RPAREN) {     /* the object's type, not widened */
                narrow_dest = outer;
                next();

                return sizeof_postfix(SIZEOF_OBJECT);
            }
        }
        if (what == SIZEOF_OBJECT)
            vderef();
        binary_rest(PREC_LOWEST);
        if (tok == TK_QUESTION)
            conditional_rest();

        /* `sizeof (0, x.c)`: the comma's answer is its right side, a
         * value, so an array there is its first element's address. */
        while (accept(TK_COMMA)) {
            vdrop();
            expr();
        }
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
        if (tok == TK_LPAREN) {
            call_rest(name);

            return sizeof_postfix(SIZEOF_VALUE);
        }
        sym = sym_find_value(name);
        if (sym != SYM_NONE && sym_at(sym)->kind == SYM_FUNC)
            acc_error_prev("'%s' is a function, which has no size",
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
                acc_error_prev("'%s' has no size yet", name_text(name));
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
        vset_ext(ext_array(joined_elem(), 0, joined_count(len) + 1));

        return sizeof_postfix(SIZEOF_OBJECT);
    }

    if (accept(TK_LPAREN)) {
        if (starts_decl()) {
            int x, count, elem_x, line = tok_line;
            const char *spot = tok_at;
            Type elem, type = type_name_elem(&x, &count, &elem, &elem_x);

            expect(TK_RPAREN, "')'");

            /* A compound literal, whose size is the object's and not the
             * pointer an array one comes to as a value. */
            if (tok == TK_LBRACE) {
                compound_literal(type, x, count, elem, elem_x, line, spot, 0,
                                 &count);
                if (count) {
                    vset_type(type_ptr_to(TY_EXT),
                              ext_array(elem, elem_x, count));

                    return sizeof_postfix(SIZEOF_OBJECT);
                }

                return sizeof_postfix(SIZEOF_VALUE);
            }
            if (count < 0)
                acc_error_spot(line, spot, "an array type here needs its "
                                           "size");
            if (type_is_array(type))
                acc_error_spot(line, spot, "a cast cannot make an array");
            cast_operand(type, x, base_const ? VQ_CONST : 0);

            /* A cast's result has the type it names, and that type's size is
             * the answer. What cast_operand leaves is promoted to int for
             * the arithmetic that usually comes next, which sizeof does not
             * do: `sizeof ((char) v)` was three. */
            vset_type(type, x);

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
int va_slot(Type type, int ext)
{
    if (type_is_struct(type))
        return (ext_bytes(ext) + ACC_INT_SIZE - 1) / ACC_INT_SIZE * ACC_INT_SIZE;

    if (type_eight(type))
        return 3 * ACC_INT_SIZE;

    return type_wide(type) ? 2 * ACC_INT_SIZE : ACC_INT_SIZE;
}

/* The address of the va_list a form is given. C99 asks that va_arg and the
 * rest be handed the va_list object itself, and a function that walks
 * someone else's arguments is handed a pointer to it -- so `va_arg(*ap, T)`
 * and `va_arg(aps[i], T)` are as ordinary as `va_arg(ap, T)`, and all three
 * are the same question: where does the object live? That is what `&` asks
 * of an operand, so it is asked here the same way, and what comes back is a
 * pointer to the va_list -- a char ** -- which is what says it was one. */
static void va_list_address(void)
{
    int line = tok_line;
    const char *spot = tok_at;

    if (tok != TK_IDENT && tok != TK_STAR && tok != TK_LPAREN)
        acc_error_spot(line, spot, "expected the va_list, found %s",
                       tok_spelling(tok));
    address_of_operand();
    if (vtype() != type_ptr_to(type_ptr_to(TY_CHAR)))
        acc_error_spot(line, spot, "this has to be a va_list, and it is not");
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
    const char *spot = tok_at;

    next();
    expect(TK_LPAREN, "'('");
    va_list_address();

    switch (form) {
    case TK_KW_VA_START: {
        const Sym *last;
        int sym;

        expect(TK_COMMA, "','");
        if (current_fn == SYM_NONE || !(sym_flags(current_fn) & SYMF_VARIADIC))
            acc_error_spot(line, spot, "va_start in a function with no '...'");
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
        int x, slot, tline;
        const char *tspot;
        Type type;

        expect(TK_COMMA, "','");
        if (!starts_decl())
            acc_error_at(tok_line, "va_arg needs a type, found %s",
                         tok_spelling(tok));
        tline = tok_line;
        tspot = tok_at;
        type = type_name(&x);
        if (type_is_func(type))         /* 7.15.1.1: an object's type */
            acc_error_spot(tline, tspot, "va_arg reads a value, and a "
                                         "function type has none");
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

        /* A value like any other, and a subscript or a member may follow
         * it: `va_arg (ap, char *)[0]`, pr46130-1. */
        if (tok_postfix() || tok == TK_LPAREN)
            subscript_value();

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
/* `__builtin_offsetof(type, member)`: how far into the type the member
 * starts, as a constant.
 *
 * A builtin rather than the macro <stddef.h> would have, because the macro
 * is `&((type *) 0)->member` and what that asks of a compiler -- the address
 * of a member of an object at address zero, folded rather than emitted -- is
 * more than acc has. This is the same answer without the pretence.
 *
 * The member may be reached through others and through subscripts, since
 * `__builtin_offsetof(t, a.b[2].c)` is as much a place in the type as a
 * plain name is. */
static void offsetof_value(void)
{
    int line = tok_line, x, count, elem_x, at = 0;
    const char *spot = tok_at;
    Type elem, type;

    next();
    expect(TK_LPAREN, "'('");
    type = type_name_elem(&x, &count, &elem, &elem_x);
    if (!type_is_struct(type))
        acc_error_spot(line, spot, "__builtin_offsetof wants a struct or a "
                                   "union, and this is not one");
    expect(TK_COMMA, "','");

    for (;;) {
        NameRef name = declared_name();
        int offset, top, member = member_lookup(x, name, &offset, &top);

        if (member < 0)
            acc_error_spot(line, spot, "'%s' is not a member of it",
                           name_text(name));
        at += offset;
        type = member_type(member);
        x = member_ext(member);

        /* Into an array of them, or into one of them. */
        while (accept(TK_LBRACKET)) {
            int index = constant_int("an index in __builtin_offsetof", line);

            expect(TK_RBRACKET, "']'");
            at += index * type_bytes(ext_elem(x), ext_elem_x(x));
            type = ext_elem(x);
            x = ext_elem_x(x);
        }
        if (!accept(TK_DOT))
            break;
        if (!type_is_struct(type))
            acc_error_spot(line, spot, "what is before the '.' is not a "
                                       "struct");
    }
    expect(TK_RPAREN, "')'");
    vpush_const(at, TY_UINT);
    (void) count;
    (void) elem;
    (void) elem_x;
}

static void sizeof_value(void)
{
    int line = tok_line, paren, x;
    const char *spot = tok_at;
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
            acc_error_spot(line, spot, "a bit-field has no size of its own");
        if (what == SIZEOF_OBJECT)
            type = type_deref(type);

        /* An operand whose type is a VLA is evaluated (C99 6.5.3.4p2), and
         * the code that worked out its size is part of what it did:
         * rolled back, the size's slot was never written. typename-vla-1
         * takes `sizeof (*(++a, (char (*)[a])0))`. */
        if (vla_size_slot(type, x))
            vdrop();
        else
            gen_rollback(&mark);

        /* An array whose length the program worked out: its size is the
         * value the declaration put beside it. */
        if (sizeof_vla) {
            vpush_local(sizeof_vla_slot, TY_UINT);

            return;
        }
    }

    if (type == TY_VOID)
        acc_error_spot(line, spot, "'void' has no size");
    if (vla_size_slot(type, x)) {       /* a VLA's row, or a typedef of one */
        vpush_local(vla_size_slot(type, x), TY_UINT);

        return;
    }
    if (type_is_array(type) && ext_count(x) < 0)
        acc_error_spot(line, spot, "an array of unknown size has no size to "
                                   "give");
    vpush_const(type_bytes(type, x), TY_UINT);
}
