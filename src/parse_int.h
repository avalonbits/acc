/*
 * What the parser's files share among themselves: the declaration being
 * read, the function being compiled, the scopes, and their calls to each
 * other. What the rest of acc sees of them is in acc.h.
 *
 * There is no syntax tree. Each production emits as it is recognised, which
 * is what makes the compiler one pass and what keeps it small enough to run
 * on the machine it compiles for.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ACC_PARSE_INT_H
#define ACC_PARSE_INT_H

#include <stdio.h>

#include "acc.h"

#define ERRORS_EXIT 100

/* How acc fails without -errors. On the host, 1 for an error and 2 for a
 * command line it does not take, as a compiler does. On the Agon an error
 * is 100 as well: acc has said what went wrong, 1 would add "Invalid
 * command" under it, and a code past MOS's table still stops an obey file
 * at the line that failed. A command line is 19, whose message, "Invalid
 * parameter", is the right one. */
#ifdef AGONDEV
#define ERROR_EXIT  ERRORS_EXIT
#define USAGE_EXIT  19
#else
#define ERROR_EXIT  1
#define USAGE_EXIT  2
#endif

#define INLINE_PARAMS_MAX 8

struct Inline {
    int      fn;
    char    *text;              /* the expression and a `)` to end it */
    int      len;
    int      nparams;
    NameRef  params[INLINE_PARAMS_MAX];
    int      nids;
    NameRef *ids;               /* every other name in the text */
    int     *id_sym;            /* what each was where it was written */
    int     *id_macro;
#ifdef OPT_ACC
    int      body;              /* the whole body: see inline_body_expand */
#endif
};

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

/* Whether the token is const, volatile or restrict, which are next to each
 * other among the tokens. */
#define tok_qualifier()  ((unsigned char) (tok_low - TK_KW_CONST) < 3u)

/* Where the stack was when the block being compiled started taking room for
 * arrays whose lengths it works out -- the frame slot it was saved in -- or
 * NO_VLA_MARK when it has taken none.
 *
 * "None" was -1, and asked about as `>= 0`. But a frame slot is below the
 * frame pointer, so every mark ever taken was negative and read as none:
 * the room was never given back, at the end of a block or at a break or a
 * continue, and a loop whose body declared one took it again on every
 * turn until the stack ran into the heap. None is 1 now, which no local's
 * slot can be -- the bytes above the frame pointer are the saved ix, the
 * return address and the arguments.
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
#define NO_VLA_MARK 1

/* The marks of the blocks the one being compiled is inside, outermost first,
 * each with a serial number that says which block it is: what a goto back
 * to a label reads to find out how much room it is leaving. vla_mark is
 * the last of them. A block is known by its serial because two blocks one
 * after the other can start with the same symbols in scope.
 *
 * And the last variably modified declaration each has made, numbered from
 * the serials, so that it is later than the block's own: a goto may not
 * jump into the scope of one (C99 6.8.6.1p1). One no later is an earlier
 * block's, left in the entry, which opening a block does not clear. */
typedef struct {
    int serial, mark;
    int vm_last;            /* its last variably modified declaration's
                             * number, if that is more than its serial */
} VlaBlock;

/* Whether a block has declared one since `then`, a serial. */
#define vm_since(b, then) \
    ((b)->vm_last > (then) && (b)->vm_last > (b)->serial)

#if defined(AGONDEV) && !defined(ACC_LIBC)
#define ACC_LIBC "/lib/acc/libc.a"
#endif

#define is_archive(path) ends_in((path), 'a')

/* diag.c */
extern const char *errors_path;
extern int errors_asked;
__attribute__((noreturn)) void error_before(int line, const char *at, const char *fmt, ...);

/* expr.c */
extern int current_fn;
extern Type narrow_dest;
void reserved_word(void);
int member_lookup(int x, NameRef name, int *offset, int *top);
extern char *str_joined;
extern int str_joined_wide;
int string_gather(void);
int joined_count(int len);
int joined_data(int len);
void primary(void);
void binary_rest(int min_prec);
void expr(void);
void comma_expr(void);
void conditional(void);
int va_slot(Type type, int ext);

/* inline.c */
extern int inline_capture;
int inline_fits(int fn, Type ret);
__attribute__((noinline)) void return_kept(int line, const char *spot);
const struct Inline *inline_usable(int fn);
__attribute__((noinline)) void inline_expand(const struct Inline *in, int fn);
#ifdef OPT_ACC
extern int inline_count_mode;     /* a child's run: see inline_child */
extern char inline_count_out[];   /* where a child writes, and removes */
int  inline_counts_fork(void);    /* 1 in a child */
void inline_counts_report(void);  /* a child's findings, sent; it ends */
void inline_count_call(int fn);
void inline_func_done(int fn, int size);  /* its code made, so many bytes */
void inline_count_address(int fn);
void inline_body_begin(int fn);   /* the body's `{` read: keep it? */
void inline_body_end(void);       /* its `}` the current token */
int  inline_body_returning(void); /* whether a return is a body's */
int  inline_body_call(const struct Inline *in, int fn, int line, const char *spot);
int  stmt_in_loop(void);
void call_args_stored(int fn, const int *slots, int n, int line, const char *spot);
void inline_statement(void);     /* stmt.c */
int  gen_stack_depth(void);       /* vstack.c */
void inline_body_return(void);
#endif

/* type.c */
extern unsigned char base_const;
extern unsigned char storage_ok;
extern int decl_storage;
extern unsigned char decl_inline;
extern int in_body;
extern int scope_mark;
extern int body_mark;
int push_here(NameRef name, int kind, int val);
void not_redeclared(NameRef name, int line, const char *spot);
__attribute__((noinline)) void local_redeclared(NameRef name, int line, const char *spot);
#ifdef OPT_ACC
extern int attr_hot;            /* lex.c: `hot` said, not yet taken */
void hot_add(NameRef name);     /* func.c: a function said hot */
void inline_said(NameRef name, int said);   /* inline.c: attr_inline taken */
#endif
extern int base_ext;
const char *record_name(int x);
void record_complete(int x, int line, const char *spot);
Type base_type(void);
extern unsigned char decl_start[TK_COUNT];
__attribute__((noinline)) int is_typedef_name(NameRef name);
extern unsigned char stars_const;
__attribute__((noinline)) unsigned char star_qualifiers(void);
int constant_folded(const char *what, int line, const char *spot, int before);
int constant_int(const char *what, int line);
extern int vla_length;
extern int vla_mark;
extern VlaBlock *vla_blocks, *vla_top;
extern int nvla_blocks, vla_blocks_cap, vla_serial;
void vla_block_open(void);
void vm_declared(void);
__attribute__((noinline)) void vm_maybe(int ext);
void block_vla_mark(void);
extern int in_params;
extern int vla_texts_used;
extern unsigned char nvla_texts;
extern unsigned char nvla_params;
int vla_type(Type elem, int elem_x, int length, int count);
__attribute__((noinline)) void vla_params_enter(void);
int array_dims(Type base, int base_x, Type *elem, int *elem_x, int *count);
extern int abstract_ok;
NameRef declared_name(void);
Type type_name_elem(int *x, int *count, Type *elem, int *elem_x);
Type type_name(int *x);
extern NameRef *param_names;
extern int param_names_cap;
extern unsigned nested_lists;
__attribute__((noinline)) int params_keep_run(int first, int count);
void func_suffix(Type *type, int *ext);
__attribute__((noinline)) NameRef paren_declarator(Type t, int tx, Type *type, int *ext, int *count);
__attribute__((noinline)) void typedef_array(Type *type, int *ext, int *count);

/* init.c */
__attribute__((noinline)) int local_struct_object(int x, int line, const char *spot, int braced);
void local_struct(int x, NameRef name, int line, const char *spot);
__attribute__((noinline)) int local_array_object(Type elem, int elem_x, int *countp, int line, const char *spot, int braced);
__attribute__((noinline)) void local_vla(Type elem, int elem_x, NameRef name, int line, const char *spot);
void local_array(Type elem, int elem_x, NameRef name, int count, int line, const char *spot);
extern int static_local;
extern int redefining;
extern unsigned char decl_const;
extern int decl_extern;
extern int decl_static;
extern int decl_inline_fn;
extern unsigned char decl_bottom_const;
int push_global(NameRef name, int kind, int at);
__attribute__((noinline)) void literal_bytes(Type type, int x, int count, Type elem, int elem_x, int line, const char *spot, int address, int *countp);
void global_emit(Type type, int ext, NameRef name, int count, int line, const char *spot);

/* decl.c */
extern unsigned char decl_quals;
__attribute__((noinline)) int storage_declaration(Type base, int bx, unsigned char bc);
int function_declarator(Type ret_type, int ret_ext, NameRef name, int line, const char *spot);
void bss_end(void);
__attribute__((noinline)) int function_from_type(int x, NameRef name, int line, const char *spot);
void static_assert_declaration(void);
void translation_unit(void);

/* stmt.c */
void labels_end(void);
void block(void);

/* link.c */
void link_inputs(const char **objs, int nobjs);

/* main.c */
int ends_in(const char *path, char what);

static inline __attribute__((always_inline))
void local_not_redeclared(NameRef name, int line, const char *spot)
{
    local_redeclared(name, line, spot);
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

/* The stars in front of one name. Inlined, as not_void is: they were one
 * function before the split, and as two calls each they cost 1% of a compile
 * of a program that declares a lot of names. */
static inline __attribute__((always_inline))
Type declarator_stars(Type base)
{
    int line = tok_line;
    const char *spot = tok_at;

    stars_const = 0;
    while (accept(TK_STAR)) {
        if (type_ptr_depth(base) == TY_PTR_MAX)
            acc_error_spot(line, spot, "a pointer can be %d deep and this is "
                                       "deeper", TY_PTR_MAX);
        base = type_ptr_to(base);
        stars_const = tok_qualifier() ? star_qualifiers() : 0;
    }

    return base;
}

/* That what the stars left is a type a value can have. The check has to come
 * after them: there is no value of type void, but `void *` is an ordinary
 * pointer to something unsaid. */
static inline __attribute__((always_inline))
void not_void(Type type, const char *what, int line, const char *spot)
{
    if (type == TY_VOID)
        acc_error_spot(line, spot, "'void' is not a type %s can have", what);
}

/* The frame slot a VLA's size is in, when the type is one; 0 otherwise. */
static inline __attribute__((always_inline))
int vla_size_slot(Type type, int x)
{
    return type_is_array(type) ? ext_vla_size(x) : 0;
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

/* Inlined into both callers, the function body and a for's first clause:
 * it was inlined into the first when it had only that one, and as a call it
 * is one more on every declaration in the program. */
static inline __attribute__((always_inline))
void declaration(void)
{
    Type base;
    int bx;
    unsigned char bc;

    if (tok == TK_KW_STATIC_ASSERT) {
        static_assert_declaration();

        return;
    }

    storage_ok = 1;
    decl_storage = 0;
    base = base_type();
    storage_ok = 0;
    bx = base_ext;
    bc = base_const;
    if (decl_storage && storage_declaration(base, bx, bc))
        return;

    /* Nothing but the type: `enum e { A, B };`, declaring what is in it. */
    if (accept(TK_SEMI))
        return;

    for (;;) {
        int line = tok_line, count, ext, off, sym, far = 0;
        const char *spot = tok_at;
        Type type, stars = declarator_stars(base);
        NameRef name = direct_declarator(stars, bx, &type, &ext, &count);

        if (type_is_func(type)) {
            function_from_type(ext, name, line, spot);
            if (!accept(TK_COMMA))
                break;
            continue;
        }
        if (tok == TK_LPAREN) {         /* a function declared in a block */
            decl_bottom_const = bc ? SQ_CONST : 0;
            function_declarator(type, ext, name, line, spot);
            if (!accept(TK_COMMA))
                break;
            continue;
        }
        local_not_redeclared(name, line, spot);
        if (vla_length) {
            local_vla(type, ext, name, line, spot);
            if (bc)
                sym_at(sym_find(name))->quals |= SQ_CONST;
            if (!accept(TK_COMMA))
                break;
            continue;
        }
        if (count) {
            local_array(type, ext, name, count, line, spot);
            if (bc)
                sym_at(sym_find(name))->quals |= SQ_CONST;
            if (!accept(TK_COMMA))
                break;
            continue;
        }
        if (type_is_struct(type)) {
            local_struct(ext, name, line, spot);
            if (bc)
                sym_at(sym_find(name))->quals |= SQ_CONST;
            if (!accept(TK_COMMA))
                break;
            continue;
        }
        not_void(type, "a variable", line, spot);

        /* Past what the frame pointer reaches, a local goes where the
         * arrays and the structs go: see gen_local_fits. Everything about
         * it is the same afterwards except that its address is worked out
         * rather than being a displacement. */
        far = !gen_local_fits(type_scalar_bytes(type));
        off = far ? gen_local_far(type_scalar_bytes(type))
                  : gen_local(type_scalar_bytes(type));

        /* In scope from here, before its initialiser, as C says: in
         * `int x = x;` the second x is the new one. */
        sym = sym_push(name, far ? SYM_LOCAL_FAR : SYM_LOCAL, off);
        sym_at(sym)->type = type;
        sym_at(sym)->ext = (unsigned char) ext;
        if (ext)
            vm_maybe(ext);              /* `int (*p)[n]` */
        sym_at(sym)->quals = decl_quals | (bc ? SQ_CONST : 0);
        if (!far)
            gen_iy_claim(off, type, decl_quals);
#ifdef OPT_ACC
        if (!far)
            prescan_claim(off, type, name);
#endif
        if (stars != base ? stars_const : bc)
            sym_at(sym)->kind = far ? SYM_LOCAL_FAR : SYM_LOCAL_CONST;
        if (accept(TK_ASSIGN)) {
            Type outer = narrow_dest;
            int braced = tok == TK_LBRACE;      /* as global_emit's */

            if (braced)
                next();
            narrow_dest = type_narrow(type);
            expr();
            narrow_dest = outer;
            if (braced) {
                accept(TK_COMMA);
                expect(TK_RBRACE, "'}'");
            }
            if (far) {
                vaddr_array(off, type);
                vswap();
                vstore_indirect();
            } else {
                vstore_local(off, type);
            }
            gen_discard();            /* a declaration is not an expression */
        }

        if (!accept(TK_COMMA))
            break;
    }
    expect(TK_SEMI, "';'");
    decl_quals = 0;             /* a register given after the type */
}

#endif
