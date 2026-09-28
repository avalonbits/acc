/*
 * Declarations: at block scope and at file scope, typedefs and externs,
 * function definitions, a global defined and defined again, and the
 * translation unit.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "acc.h"
#include "ctype.h"
#include "fmt.h"
#include "timing.h"
#include "version.h"
#include "parse_int.h"

/* Whether `name` is declared in this block already as the file-scope
 * variable an extern names, which -- having linkage -- it may be again
 * (6.7p3), to the same type: block_extern holds it to that. */
static int extern_again(NameRef name)
{
    int sym = sym_find(name), g = name_global(name);

    return sym != SYM_NONE && g != SYM_NONE && sym != g
           && sym_declared_in(sym, scope_mark)
           && sym_at(sym)->kind == sym_at(g)->kind
           && sym_at(sym)->val == sym_at(g)->val;
}
static void global_variable(Type type, int ext, NameRef name, int count,
                            int line, const char *spot);
static int  global_again(int sym, Type type, int ext, int count, int line,
                         const char *spot);

/* `typedef`, and names for types rather than objects: each declarator names
 * the type it would have given a variable. An array type keeps its shape in
 * an extension, so that an object declared with it is that array. */
__attribute__((noinline))
/* The names a typedef declares, once its specifiers have been read. */
static void typedef_declarators(Type base, int bx, unsigned char bc)
{
    for (;;) {
        int line = tok_line, count, ext, sym;
        const char *spot = tok_at;
        Type type;
        NameRef name = direct_declarator(declarator_stars(base), bx, &type,
                                         &ext, &count);

        if (!count)
            func_suffix(&type, &ext);

        /* `typedef int row[n];`: its length and size are worked out here,
         * where C99 says they are, and the type keeps where they went. */
        if (vla_length) {
            int length = gen_local(ACC_INT_SIZE);

            vstore_local(length, TY_INT);
            vdrop();
            ext = vla_type(type, ext, length, 0);
            type = TY_EXT;
            count = 0;
        }
        /* `typedef int A[];` names an array of unknown size, and what is
         * declared with it takes its length from its initialiser. */
        if (count) {
            ext = ext_array(type, ext, count);
            type = TY_EXT;
        }
        /* The same typedef again is not a mistake: two headers that refer
         * to each other's types each say `typedef struct _u u;` so that the
         * other's pointers have a name, and then one of them completes it.
         * C11 says as much outright, and every compiler accepted it long
         * before that, so code written for any of them relies on it. The
         * types have to agree; naming the same word for two things is the
         * mistake this is still here to catch. */
        sym = sym_find(name);
        if (sym != SYM_NONE && sym_declared_in(sym, scope_mark)
            && sym_at(sym)->kind == SYM_TYPEDEF
            && sym_at(sym)->type == type && sym_at(sym)->ext == ext) {
            decl_start[TK_IDENT] = 1;
            if (!accept(TK_COMMA))
                break;

            continue;
        }
        not_redeclared(name, line, spot);
        if (in_body && ext)
            vm_maybe(ext);              /* `typedef int row[n];` */
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
                         int line, const char *spot)
{
    int g = name_global(name), sym;
    const Sym *global;

    if (tok == TK_ASSIGN)
        acc_error_spot(line, spot, "an extern in a block cannot give a value");
    if (g == SYM_NONE) {
        /* Nothing of that name at file scope yet, so this declaration
         * introduces it -- and reserves nothing for it, which is what extern
         * means wherever it is said. It used to put the variable's bytes
         * here, in the middle of a function, with a jump over them. */
        decl_extern = 1;
        global_variable(type, ext, name, count, line, spot);
        decl_extern = 0;
        g = name_global(name);
    } else {
        global_again(g, type, ext, count, line, spot);
    }
    global = sym_at(g);
    sym_stamp_name(name);               /* see local_not_redeclared */
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
unsigned char decl_quals;

/* The names a block's static or extern declaration declares, once its
 * specifiers have been read.
 *
 * A block's static variable is a file-scope one in all but its name: its
 * bytes are in the image, written here and jumped over, initialised once
 * from constants, and kept from one call to the next. */
static void storage_declarators(int storage, Type base, int bx,
                                unsigned char bc)
{
    if (accept(TK_SEMI))
        return;
    for (;;) {
        int line = tok_line, count, ext;
        const char *spot = tok_at;
        Type type, stars = declarator_stars(base);
        NameRef name = direct_declarator(stars, bx, &type, &ext, &count);

        if (vla_length)                 /* C99 6.7.5.2p2 */
            acc_error_spot(line, spot, "a static or extern array's length has "
                                       "to be known as the program is "
                                       "compiled");
        decl_const = stars != base ? stars_const : bc;
        decl_bottom_const = bc ? SQ_CONST : 0;
        if (tok == TK_LPAREN) {
            function_declarator(type, ext, name, line, spot);
        } else if (storage == TK_KW_EXTERN) {
            if (!extern_again(name))
                not_redeclared(name, line, spot);
            block_extern(type, ext, name, count, line, spot);
        } else {
            int hole = gen_jump();

            not_redeclared(name, line, spot);
            static_local = 1;
            global_variable(type, ext, name, count, line, spot);
            static_local = 0;
            gen_label(hole);
        }
        if (!accept(TK_COMMA))
            break;
    }
    expect(TK_SEMI, "';'");
}

/* A block's declaration with a storage class, wherever among its
 * specifiers it was: `static int x;`, `int static x;`. 0 for auto and
 * register, whose declarators are the ordinary ones -- auto is what a
 * block's variable is anyway, and register only forbids taking its
 * address, which decl_quals marks. */
__attribute__((noinline))
int storage_declaration(Type base, int bx, unsigned char bc)
{
    int storage = decl_storage;

    if (storage == TK_KW_TYPEDEF) {
        typedef_declarators(base, bx, bc);

        return 1;
    }
    if (storage == TK_KW_STATIC || storage == TK_KW_EXTERN) {
        storage_declarators(storage, base, bx, bc);

        return 1;
    }
    decl_quals = storage == TK_KW_REGISTER ? SQ_REGISTER : 0;

    return 0;
}

/* The struct parameters of the function being defined, which are copied
 * into its frame once the prologue has been emitted: see function_rest. */
typedef struct {
    int sym, argoff;
} StructParam;

static StructParam *struct_params;
static int          nstruct_params, struct_params_cap;

/* The end of a function's body, reached by running off it. For main that
 * returns 0: C99 5.1.2.2.3 says reaching the } that ends main is a return
 * of 0, and a program that relies on it is correct. Every other function
 * that runs off its end leaves whatever was last in HL, which is only
 * undefined if its caller uses it, and costs nothing to leave alone.
 * Here main returned whatever HL held, so a program whose checks all
 * passed reported a failure.
 *
 * The name is looked up once per file, not once per function: interning it
 * hashes it and compares it against its bucket every time, and done for
 * every function body that was 1% of compiling names.c. */
static NameRef main_name;       /* interned once, in translation_unit */

static void body_end(int fn)
{
    const Sym *s = sym_at(fn);

    if (s->name == main_name && s->type == TY_INT) {
        vpush_const(0, TY_INT);
        gen_return(tok_line, tok_at);
    }
}

/* A struct parameter is copied from its slots into the array area, where
 * every local struct lives, so that the body finds it as it finds any
 * other: the slots are above the frame, and past the reach of (ix+d) once
 * there are a few of them. Out of line, for the few functions that have
 * one. */
__attribute__((noinline))
static void struct_params_copy(void)
{
    int i;

    for (i = 0; i != nstruct_params; i++) {
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
                           int count, int params, NameRef name, int line,
                           const char *spot)
{
    int i, prior = sym_params_first(fn);

    if (sym_at(fn)->type != ret_type || sym_at(fn)->ext != ret_ext)
        acc_error_spot(line, spot, "'%s' is declared again with another "
                                   "result type", name_text(name));
    if (!params || !(sym_flags(fn) & SYMF_PARAMS))
        return;
    if (count != sym_nparams(fn))
        acc_error_spot(line, spot, "'%s' is declared again with %d "
                                   "parameters, not %d", name_text(name),
                       count, sym_nparams(fn));
    for (i = 0; i < count; i++)
        if (sym_param_type(first, i) != sym_param_type(prior, i)
            || !ext_compatible(sym_param_ext(first, i), sym_param_ext(prior, i)))
            acc_error_spot(line, spot, "'%s' is declared again with parameter "
                                       "%d of another type", name_text(name),
                           i + 1);
}

/* `()` against a list of parameters, which C99 6.7.5.3p15 lets agree only
 * where a call through `()` would pass what the list takes: no `...`, and
 * no parameter the default promotions would change -- a char, a short. A
 * definition's `()` has no parameters, and agrees with a list of none:
 * `unlisted` says this declaration is the one with `()`. */
__attribute__((noinline))
static void unlisted_agrees(int first, int count, int variadic, int unlisted,
                            NameRef name, int line, const char *spot)
{
    int i;

    if (variadic)
        acc_error_spot(line, spot, "'%s' is declared with '()' and with "
                                   "'...', which do not agree",
                       name_text(name));
    if (unlisted && tok == TK_LBRACE && !in_body && count)
        acc_error_spot(line, spot, "'%s' is defined with no parameters, and "
                                   "was declared with %d", name_text(name),
                       count);
    for (i = 0; i < count; i++) {
        Type t = sym_param_type(first, i);

        if (!type_pointer(t) && !type_is_struct(t) && !type_float(t)
            && type_promote(t) != t)
            acc_error_spot(line, spot, "'%s' is declared with '()', which "
                                       "passes parameter %d promoted, and "
                                       "with a type for it that is not",
                           name_text(name), i + 1);
    }
}

/* That a name declared again at file scope keeps its linkage (C99
 * 6.2.2p7 leaves both at once undefined): `static` after a declaration
 * that was not, or a variable with no storage class after one that was.
 * `extern`, and a function with no storage class, take the linkage it
 * already has. */
__attribute__((noinline))
static void same_linkage(int sym, int is_static, int takes, int line,
                         const char *spot)
{
    int was = sym_flags(sym) & SYMF_STATIC;

    if (is_static && !was)
        acc_error_spot(line, spot, "'%s' is declared static after a "
                                   "declaration that was not",
                       name_text(sym_at(sym)->name));
    if (!is_static && !takes && was)
        acc_error_spot(line, spot, "'%s' was declared static, and this "
                                   "declaration is not",
                       name_text(sym_at(sym)->name));
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
int function_declarator(Type ret_type, int ret_ext, NameRef name,
                               int line, const char *spot)
{
    int fn, declared, params = 1, unnamed = 0, mark, variadic = 0;
    int nparams = 0, argoff, params_first;
    int incomplete_line = 0, incomplete_x = 0;
    const char *incomplete_spot = NULL;
    unsigned seen = nested_lists;

    expect(TK_LPAREN, "'('");

    fn = sym_find(name);
    if (fn != SYM_NONE && sym_at(fn)->kind != SYM_FUNC)
        acc_error_spot(line, spot, "'%s' is already %s", name_text(name),
                       sym_at(fn)->kind == SYM_GLOBAL ? "a variable"
                                                      : "declared");
    declared = fn != SYM_NONE && (sym_flags(fn) & SYMF_DECLARED);

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
    vla_texts_used = 0;
    nvla_texts = nvla_params = 0;
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
            const char *pspot = tok_at;
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
            not_void(ptype, "a parameter", pline, pspot);

            if (pname) {
                /* Named once, as C99 6.7p3 asks of a prototype as much as
                 * of a definition. */
                psym = sym_push_param(pname, argoff, mark);
                if (psym == SYM_NONE)
                    acc_error_spot(pline, pspot,
                                   "'%s' is already declared",
                                   name_text(pname));
                sym_at(psym)->type = ptype;
                sym_at(psym)->ext = (unsigned char) pext;
                sym_at(psym)->quals = pquals;
                if (pconst && !pcount && !type_is_struct(ptype))
                    sym_at(psym)->kind = SYM_LOCAL_CONST;
            } else {
                unnamed = 1;
            }
            if (nested_lists != seen) {
                params_first = params_keep_run(params_first, nparams);
                seen = nested_lists;
            }
            sym_param_add(ptype, pext);

            /* Every argument occupies whole slots: one however narrow it is,
             * two for a long, and as many as a struct fills. That is what
             * agondev does, and the narrow ones are read from the low bytes
             * of their slot. */
            if (type_is_struct(ptype)) {
                /* A prototype may name a struct not yet complete (C99
                 * 6.7.5.3p12 asks it only of a definition): whether this is
                 * one is known at the `)`, and it is asked then. */
                if (!ext_complete(pext) && !incomplete_line) {
                    incomplete_line = pline;
                    incomplete_spot = pspot;
                    incomplete_x = pext;
                }
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
                       name, line, spot);
        if ((sym_at(fn)->quals ^ decl_bottom_const) & SQ_CONST)
            acc_error_spot(line, spot, "'%s' is declared again with another "
                                       "result type", name_text(name));
        if (!params != !(sym_flags(fn) & SYMF_PARAMS)) {
            if (params)
                unlisted_agrees(params_first, nparams, variadic, 0, name,
                                line, spot);
            else
                unlisted_agrees(sym_params_first(fn), sym_nparams(fn),
                                sym_flags(fn) & SYMF_VARIADIC, 1, name,
                                line, spot);
        }
        if (!in_body)
            same_linkage(fn, decl_static, 1, line, spot);
        if (params && (sym_flags(fn) & SYMF_PARAMS)
            && (sym_flags(fn) & SYMF_VARIADIC) != variadic)
            acc_error_spot(line, spot, "'%s' is declared again with%s '...'",
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
            sym_set_flags(fn, (params ? SYMF_DECLARED | SYMF_PARAMS | variadic
                                      : SYMF_DECLARED)
                              | (decl_static ? SYMF_STATIC : 0));
        }

        return 0;
    }

    /* Asked of the flag and not of the address, because compiling to an
     * object puts the first function at offset zero, and zero is what a
     * function that has not been defined has. */
    if (sym_flags(fn) & SYMF_DEFINED)
        acc_error_spot(line, spot, "'%s' is defined twice", name_text(name));
    if (unnamed)
        acc_error_spot(line, spot, "a parameter of a function's definition "
                                   "needs a name");
    sym_stamp_params(mark);     /* see local_not_redeclared */

    if (incomplete_line)
        record_complete(incomplete_x, incomplete_line, incomplete_spot);

    sym_set_params(fn, params_first, nparams);
    sym_set_flags(fn, SYMF_DECLARED | SYMF_PARAMS | SYMF_DEFINED | variadic
                      | (decl_static ? SYMF_STATIC : 0));
    current_fn = fn;
    gen_func_begin(fn, nparams, ret_type);

    if (nstruct_params)
        struct_params_copy();
    in_body = 1;
    if (nvla_params | nvla_texts)
        vla_params_enter();
    next();                     /* the body's `{` */
    if (tok == TK_KW_RETURN && decl_static && decl_inline_fn
        && !variadic && !(nvla_params | nvla_texts) && !nstruct_params)
        inline_capture = inline_fits(fn, ret_type);
    body_mark = mark;
    block();
    body_end(fn);
    in_body = 0;
    labels_end();
    gen_func_end();

    sym_drop_locals();

    return 1;
}

/* That a variable declared again at file scope is declared the same way,
 * and whether this declaration gives it its value -- which it may only do
 * once. */
static int global_again(int sym, Type type, int ext, int count, int line,
                        const char *spot)
{
    const Sym *g = sym_at(sym);
    NameRef name = g->name;

    if (g->kind == SYM_FUNC)
        acc_error_spot(line, spot, "'%s' is already a function",
                       name_text(name));
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
        || g->type != type || !ext_compatible(g->ext, ext)
        || (count && count != sym_count(sym)))
        acc_error_spot(line, spot, "'%s' is declared again with another type",
                       name_text(name));
    if (tok != TK_ASSIGN)
        return 0;
    if (sym_flags(sym) & SYMF_DEFINED)
        acc_error_spot(line, spot, "'%s' is defined twice", name_text(name));

    return 1;
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
                             int line, const char *spot, int is_extern)
{
    int kind = count ? SYM_GLOBAL_ARRAY
             : decl_const && !type_is_struct(type) ? SYM_GLOBAL_CONST
             : SYM_GLOBAL;
    int sym;

    not_void(type, "a variable", line, spot);
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
                            int line, const char *spot)
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
            global_undefined(type, ext, name, count, line, spot, 1);

            return;
        }
        (void) global_again(known, type, ext, count, line, spot);
        sym_set_flags(known, SYMF_EXTERN);

        return;
    }

    /* A name is in scope from the end of its declarator, so an initialiser
     * may use it -- `struct node n = { &n };` -- and its address is not
     * known until the bytes it names have been written. It is declared
     * first, as `extern` would declare it, and every use in its own
     * initialiser is filled in when the definition below gives it one. */
    if (init && !static_local && name_global(name) == SYM_NONE) {
        global_undefined(type, ext, name, count, line, spot, 1);
        sym_clear_flags(name_global(name), SYMF_EXTERN);
    }

    if (!static_local && (sym = name_global(name)) != SYM_NONE) {
        int saved, again = global_again(sym, type, ext, count, line, spot);

        same_linkage(sym, decl_static, decl_extern, line, spot);

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
             * room is abandoned, and whatever was compiled in between to
             * find it there is pointed at the bytes. */
            if (sym_in_bss(sym_at(sym)->val)) {
                int at = sym_bss_at(sym_at(sym)->val);

                gen_bss_forget(sym);
                sym_at(sym)->val = -1;
                redefining = sym;
                global_emit(type, ext, name, count, line, spot);
                redefining = SYM_NONE;
                sym_set_flags(sym, SYMF_DEFINED);
                gen_bss_move(at, global_bytes(sym), sym_at(sym)->val);

                return;
            }
            redefining = sym;
            global_emit(type, ext, name, count, line, spot);
            redefining = SYM_NONE;
            sym_set_flags(sym, SYMF_DEFINED);

            return;
        }
        if (!again)
            return;
        saved = out_here();
        out_seek(sym_at(sym)->val);
        redefining = sym;
        global_emit(type, ext, name, count, line, spot);
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
        global_undefined(type, ext, name, count, line, spot, 0);

        return;
    }

    /* A block's static with a value, declared first for the same reason a
     * file-scope one is above, and its uses in its own initialiser filled in
     * here: its name does not outlive the function. */
    if (static_local) {
        global_undefined(type, ext, name, count, line, spot, 1);
        sym = sym_find(name);
        sym_clear_flags(sym, SYMF_EXTERN);
        redefining = sym;
        global_emit(type, ext, name, count, line, spot);
        redefining = SYM_NONE;
        gen_settle(sym);

        return;
    }
    global_emit(type, ext, name, count, line, spot);
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
void bss_end(void)
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
        /* `int a[];` at file scope and never given a size is an array of
         * one, as if its initialiser had given it one (C99 6.9.2p5). */
        if (sym->kind == SYM_GLOBAL_ARRAY && sym_count(s) < 0)
            sym_set_count(s, 1);
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
int function_from_type(int x, NameRef name, int line, const char *spot)
{
    Type ret = ext_elem(x);
    int ret_x = ext_elem_x(x), first = ext_func_first(x);
    int count = ext_func_count(x), params = ext_func_declared(x);
    int fn = sym_find(name), i, argoff;

    if (fn != SYM_NONE && sym_at(fn)->kind != SYM_FUNC)
        acc_error_spot(line, spot, "'%s' is already declared",
                       name_text(name));
    if (fn != SYM_NONE && (sym_flags(fn) & SYMF_DECLARED))
        same_signature(fn, ret, ret_x, first, count, params, name, line, spot);
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
        acc_error_spot(line, spot, "'%s' is defined twice", name_text(name));
    argoff = 2 * ACC_PTR_SIZE + (type_is_struct(ret) ? ACC_PTR_SIZE : 0);
    nstruct_params = 0;
    for (i = 0; i < count; i++) {
        Type t = sym_param_type(first, i);
        int e = sym_param_ext(first, i), psym;

        if (i >= param_names_cap || !param_names[first + i])
            acc_error_spot(line, spot, "a parameter of a function's "
                                       "definition needs a name");
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
                      | (ext_func_variadic(x) ? SYMF_VARIADIC : 0)
                      | (decl_static ? SYMF_STATIC : 0));
    current_fn = fn;
    gen_func_begin(fn, count, ret);
    if (nstruct_params)
        struct_params_copy();
    in_body = 1;
    block();
    body_end(fn);
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
/* `_Static_assert(expression, "what is wrong")`: a claim about a constant
 * that the compile has to agree with, and stops over if it does not. Nothing
 * is emitted either way -- what it leaves behind is the message, or nothing
 * at all. */
void static_assert_declaration(void)
{
    int line = tok_line, value;
    const char *spot = tok_at;

    next();
    expect(TK_LPAREN, "'('");
    value = constant_int("a _Static_assert's condition", line);
    expect(TK_COMMA, "','");
    if (tok != TK_STRING)
        acc_error_at(tok_line, "a _Static_assert needs a message to give if "
                               "it does not hold");
    if (!value)
        acc_error_spot(line, spot, "%.*s", tok_str_len, tok_str);
    next();
    while (tok == TK_STRING)             /* "a" "b", joined as C joins them */
        next();
    expect(TK_RPAREN, "')'");
    expect(TK_SEMI, "';'");
}

static void external_declaration(void)
{
    Type base, type;
    int line, count = 0, ext, bx;
    const char *spot;
    unsigned char bc;
    NameRef name;

    if (tok == TK_KW_STATIC_ASSERT) {
        static_assert_declaration();

        return;
    }

    /* The storage class, wherever among the specifiers it is. At file scope
     * static keeps a name to this file, and extern declares what any
     * declaration here does: storage that the one that gives the value, if
     * any does, fills in. inline asks that calls be fast, and a call is
     * what they are: C lets that be the answer. */
    storage_ok = 1;
    decl_storage = 0;
    decl_inline = 0;
    base = base_type();
    storage_ok = 0;
    bx = ext = base_ext;
    bc = base_const;
    line = tok_line;
    spot = tok_at;
    if (decl_storage == TK_KW_TYPEDEF) {
        typedef_declarators(base, bx, bc);

        return;
    }
    if (decl_storage == TK_KW_AUTO || decl_storage == TK_KW_REGISTER)
        acc_error_spot(line, spot, "%s is for a variable in a block, not at "
                                   "file scope", tok_spelling(decl_storage));
    decl_extern = decl_storage == TK_KW_EXTERN;
    decl_static = decl_storage == TK_KW_STATIC;
    decl_inline_fn = decl_inline;
    if (accept(TK_SEMI))
        return;
    /* A name and then '(' is a function -- a prototype, or a definition if
     * a body follows; anything else is a variable. */
    for (;;) {
        Type stars;

        line = tok_line;
        spot = tok_at;
        stars = declarator_stars(base);
        name = direct_declarator(stars, bx, &type, &ext, &count);
        decl_const = stars != base ? stars_const : bc;
        decl_bottom_const = bc ? SQ_CONST : 0;
        if (tok == TK_LPAREN) {
            if (count)
                acc_error_spot(line, spot, "a function cannot return an "
                                           "array");
            if (function_declarator(type, ext, name, line, spot))
                return;
        } else if (type_is_func(type)) {
            if (function_from_type(ext, name, line, spot))
                return;
        } else {
            global_variable(type, ext, name, count, line, spot);
        }
        if (!accept(TK_COMMA))
            break;
    }
    expect(TK_SEMI, "';'");
}

#if defined(AGONDEV) && defined(ACC_CYCLES)
void cycles_poll(void);            /* main.c: see there */
#else
#define cycles_poll()
#endif

void translation_unit(void)
{
    main_name = name_intern("main", 4);
    while (tok != TK_EOF) {
        cycles_poll();
        external_declaration();
    }
}
