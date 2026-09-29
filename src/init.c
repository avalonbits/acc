/*
 * Initialisers: braces, designators and strings, laid down in a local's
 * frame or in a global's bytes.
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
static int  init_string(Type elem, int count, int offset, InitPut put);
static int  braced_string(Type elem);
static void braced_string_end(void);
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

/* What a wide string may initialise: an array of wchar_t, which is short
 * here, or of the unsigned short compatible with it. */
#define type_is_wchar(ty) ((ty) == TY_SHORT || (ty) == TY_USHORT)

/* That the string just gathered is the kind the array's elements want: a
 * narrow one for chars, a wide one for wchar_t. C99 6.7.8p14 and p15 allow
 * no other pairing. */
static void string_for(Type elem, int line, const char *spot)
{
    if (str_joined_wide && !type_is_wchar(elem))
        acc_error_spot(line, spot, "a wide string initialises an array of "
                                   "wchar_t, and this is not one");
    if (!str_joined_wide && !type_is_char(elem))
        acc_error_spot(line, spot, "a string initialises an array of char, "
                                   "and this is not one; a wide string, "
                                   "L\"...\", is for wchar_t");
}

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
    const char *spot = tok_at;

    next();                             /* the '[' */
    i = constant_int("the element a designator names", line);
    expect(TK_RBRACKET, "']'");
    if (i < 0)
        acc_error_spot(line, spot, "a designator names element %d, and there "
                                   "is no such element", i);
    if (count >= 0 && i >= count)
        acc_error_spot(line, spot, "a designator names element %d of an array "
                                   "of %d", i, count);
    init_designated(elem, elem_x, offset + i * type_bytes(elem, elem_x), put);

    return i;
}

/* `.name = v` in a record's list: the member is returned, so that the list
 * goes on with the one after it. */
static int init_member(int x, int offset, InitPut put)
{
    int line = tok_line, m, saved_bits, at, top;
    const char *spot = tok_at;
    NameRef name;

    next();                             /* the '.' */
    if (tok != TK_IDENT)
        acc_error_at(tok_line, "expected a member's name after '.', found %s",
                     tok_spelling(tok));
    name = tok_name;
    next();
    record_complete(x, line, spot);
    m = member_lookup(x, name, &at, &top);
    if (m < 0)
        acc_error_spot(line, spot, "'%s' has no member '%s'", record_name(x),
                       name_text(name));

    saved_bits = init_bits;
    init_bits = member_bits(m);
    init_designated(member_type(m), member_ext(m), offset + at, put);
    init_bits = saved_bits;

    return top;
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
        if (tok == TK_STRING && !init_pending
            && (type_is_char(ext_elem(x)) || type_is_wchar(ext_elem(x)))) {
            init_string(ext_elem(x), ext_count(x), offset, put);

            return;
        }
        if (!init_pending && braced_string(ext_elem(x))) {
            init_string(ext_elem(x), ext_count(x), offset, put);
            braced_string_end();

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
/* `char s[] = { "foo" }`: a char array's string may be in braces (C99
 * 6.7.8p14). With the `{` next, reads it and answers 1 when a string
 * follows, which then goes the way a string without braces does;
 * braced_string_end reads the `}` after it. acc took the string for the
 * array's first char, and refused every one of these. */
static int braced_string(Type elem)
{
    if (tok != TK_LBRACE || !(type_is_char(elem) || type_is_wchar(elem))
        || !lex_string_follows())
        return 0;
    next();

    return 1;
}

static void braced_string_end(void)
{
    accept(TK_COMMA);
    expect(TK_RBRACE, "'}'");
}

static int init_string(Type elem, int count, int offset, InitPut put)
{
    int line = tok_line, len, i;
    const char *spot = tok_at;

    len = string_gather();

    string_for(elem, line, spot);
    if (str_joined_wide) {
        int units = joined_count(len);

        if (count >= 0 && units > count)
            acc_error_spot(line, spot, "this string is longer than the array "
                                       "it initialises");
        for (i = 0; i < units; i++)
            put(elem, offset + 2 * i,
                (short) ((unsigned char) str_joined[2 * i]
                         | (unsigned char) str_joined[2 * i + 1] << 8));
        if (count < 0 || units < count) {
            put(elem, offset + 2 * units, 0);
            units++;
        }

        return units;
    }
    if (count >= 0 && len > count)
        acc_error_spot(line, spot, "this string is longer than the array it "
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

    record_complete(x, tok_line, tok_at);
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

/* Everything about the local initialiser being read, which a compound
 * literal inside it would otherwise overwrite: the literal is an object of
 * its own, initialised as any local is, and it takes these over to do it.
 * The outer one then stored its remaining members into the literal, and at
 * its end zeroed the ones it had already given, the map of what it had
 * given having been cleared for the literal's use. The map is handed over
 * rather than copied: the literal starts with none and grows its own. */
typedef struct {
    int            array, pending, bits, designator_next;
    unsigned char *given;
    int            given_cap;
} InitState;

static void init_save(InitState *s)
{
    s->array = init_array;
    s->pending = init_pending;
    s->bits = init_bits;
    s->designator_next = init_designator_next;
    s->given = init_given;
    s->given_cap = init_given_cap;
    init_given = NULL;
    init_given_cap = 0;
    init_pending = 0;
    init_bits = 0;
    init_designator_next = 0;
}

static void init_restore(const InitState *s)
{
    free(init_given);
    init_array = s->array;
    init_pending = s->pending;
    init_bits = s->bits;
    init_designator_next = s->designator_next;
    init_given = s->given;
    init_given_cap = s->given_cap;
}

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
    gen_value_end();
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
static int local_struct_object_in(int x, int line, const char *spot,
                                  int braced);

/* A compound literal -- `braced` -- can be inside another initialiser, and
 * saves that one's state round its own: see InitState. */
__attribute__((noinline))
int local_struct_object(int x, int line, const char *spot, int braced)
{
    InitState outer;
    int array;

    if (!braced)
        return local_struct_object_in(x, line, spot, 0);
    init_save(&outer);
    array = local_struct_object_in(x, line, spot, 1);
    init_restore(&outer);

    return array;
}

/* The name a declaration is binding, for the object that is about to be
 * made for it; NAME_NONE for a compound literal, which has none. C puts a
 * name in scope where its declarator ends, before its initialiser, so that
 * `struct node n = { &n };` points at itself: the object's symbol is pushed
 * the moment it has room and before a byte of it is initialised. */
static NameRef decl_name = NAME_NONE;

static int decl_name_push(int kind, int array, Type type, int x)
{
    int sym = sym_push(decl_name, kind, array);

    decl_name = NAME_NONE;
    sym_at(sym)->type = type;
    sym_at(sym)->ext = (unsigned char) x;

    return sym;
}

static int local_struct_object_in(int x, int line, const char *spot,
                                  int braced)
{
    int array = gen_local_array();

    if (decl_name != NAME_NONE)
        decl_name_push(SYM_LOCAL_STRUCT, array, TY_STRUCT, x);
    record_complete(x, line, spot);
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
            gen_value_end();
        }
    }

    return array;
}

void local_struct(int x, NameRef name, int line, const char *spot)
{
    decl_name = name;
    local_struct_object(x, line, spot, 0);
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
    gen_value_end();
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
static int local_array_object_in(Type elem, int elem_x, int *countp,
                                 int line, const char *spot, int braced);

/* As local_struct_object: a compound literal saves the state of the
 * initialiser it may be inside. */
__attribute__((noinline))
int local_array_object(Type elem, int elem_x, int *countp, int line,
                              const char *spot, int braced)
{
    InitState outer;
    int array;

    if (!braced)
        return local_array_object_in(elem, elem_x, countp, line, spot, 0);
    init_save(&outer);
    array = local_array_object_in(elem, elem_x, countp, line, spot, 1);
    init_restore(&outer);

    return array;
}

static int local_array_object_in(Type elem, int elem_x, int *countp, int line,
                                 const char *spot, int braced)
{
    int array = gen_local_array();
    int step = type_bytes(elem, elem_x);
    int count = *countp;
    int init, sym = SYM_NONE, in_braces;

    /* Its count is set again below, once a [] array's initialiser has said
     * what it is. */
    if (decl_name != NAME_NONE) {
        sym = decl_name_push(SYM_LOCAL_ARRAY, array, elem, elem_x);
        sym_set_count(sym, count);
    }
    init = braced || accept(TK_ASSIGN);
    if (count > 0)
        gen_local_array_size(array, count * step);

    /* A string for a char array: its bytes written into the image, where a
     * string constant goes, and copied into the array with ldir, the rest
     * zeroed after. In braces too. */
    in_braces = init && braced_string(elem);
    if (init && tok == TK_STRING && (type_is_char(elem) || type_is_wchar(elem))) {
        int sline = tok_line, len, units, from, n;
        const char *sspot = tok_at;

        len = string_gather();
        string_for(elem, sline, sspot);
        units = joined_count(len);
        if (count >= 0 && units > count)
            acc_error_spot(line, spot, "this string is longer than the array "
                                       "it initialises");
        from = joined_data(len);
        n = (count < 0 || units < count) ? units + 1 : units; /* terminator */
        if (count < 0) {
            count = n;
            gen_local_array_size(array, count * step);
        }
        gen_copy_to_array(array, 0, from, n * step);
        if (n < count)
            gen_zero_array(array, n * step, (count - n) * step);
        if (in_braces)
            braced_string_end();
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
                acc_error_spot(line, spot, "an array needs at least one "
                                           "element");
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
                    acc_error_spot(line, spot,
                                   "an array needs at least one element");
                count = n;
                gen_local_array_size(array, count * step);
            }
        }

        zero_gaps(array, count * step);
    } else if (count < 0) {
        acc_error_spot(line, spot, "an array declared with [] needs initial "
                                   "values to say how long it is");
    }

    if (sym != SYM_NONE)
        sym_set_count(sym, count);
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
void local_vla(Type elem, int elem_x, NameRef name, int line,
                      const char *spot)
{
    vm_declared();
    int step = type_bytes(elem, elem_x);
    int ptr = gen_local(ACC_PTR_SIZE);
    int size = gen_local(ACC_INT_SIZE);
    int sym;

    not_void(elem, "an array's element", line, spot);
    block_vla_mark();

    /* bytes = length * the element's size, and the size local holds it:
     * a row's size read from the frame when the program worked it out. */
    if (vla_size_slot(elem, elem_x)) {
        vpush_local(vla_size_slot(elem, elem_x), TY_INT);
        vapply(TK_STAR, 0);
    } else if (step != 1) {
        vpush_const(step, TY_INT);
        vapply(TK_STAR, 0);
    }
    vstore_local(size, TY_UINT);
    gen_stack_take(ptr);
    gen_stmt_end();

    if (tok == TK_ASSIGN)
        acc_error_spot(line, spot, "an array whose length is worked out as it "
                                   "runs cannot have an initialiser");

    sym = sym_push(name, SYM_LOCAL_VLA, ptr);
    sym_at(sym)->type = type_ptr_to(elem);
    sym_at(sym)->ext = (unsigned char) elem_x;
    sym_set_count(sym, size);
}

void local_array(Type elem, int elem_x, NameRef name, int count,
                        int line, const char *spot)
{
    if (elem_x)
        vm_maybe(elem_x);               /* `int (*a[2])[n]` */
    decl_name = name;
    local_array_object(elem, elem_x, &count, line, spot, 0);
}

/* How the variable being declared at file scope is bound: see push_global. */
int static_local;
int redefining = SYM_NONE;
unsigned char decl_const;    /* the variable being declared is const */
int decl_extern;             /* and its declaration said extern */
int decl_static;             /* or static, which keeps it to this file */
int decl_inline_fn;          /* and inline, which a function may be */
unsigned char decl_bottom_const; /* and SQ_CONST, when its type is */

/* How the variable being declared at file scope is bound to its name.
 *
 * A block's `static` is one too -- its bytes in the image, where they last
 * as long as the program -- but its name is the block's: static_local says
 * so. And a variable declared once already has its storage, allocated when
 * it was first declared: a later definition writes its initial value over
 * those bytes, with the output rewound to them, and binds nothing new --
 * redefining says which symbol it is. */
int push_global(NameRef name, int kind, int at)
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
    if (static_local)
        sym_stamp_name(name);           /* see local_not_redeclared */
    sym = static_local ? sym_push_local(name, kind, at)
                       : sym_push(name, kind, at);
    sym_at(sym)->quals = decl_bottom_const;
    if (decl_static)
        sym_set_flags(sym, SYMF_STATIC);

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
static void global_initializer(Type type, unsigned char *bytes)
{
    int size = type_scalar_bytes(type);
    int before, line = tok_line;        /* an error is at the value */
    uint64_t value;
    Type from;
    int i;
    const char *spot = tok_at;

    /* Read as an expression and folded, whatever it is: a long's arithmetic
     * is worked out by the compiler now, so `2L + 3L` and `1L << 20` are as
     * much a constant here as `5L` is. What a global may not have is
     * anything that leaves code behind, which is what the out_here check
     * catches -- a call, a variable, an address that is not a symbol's. */
    init_address = init_bss = 0;
    gen_data_begin();
    expr();                     /* not comma_expr: in a braced list the
                                 * comma between values is a separator */
    gen_data_end();

    /* From here on nothing may be emitted: what the expression itself left
     * in the image is a string's bytes, which are a constant's and fine.
     * What would not be fine is the conversion below turning into code,
     * which is what an initial value that is not a constant does. */
    before = out_here();
    vconvert(type);
    if (!vconst_wide(&value, &from) || out_here() != before)
        acc_error_spot(line, spot, "a global's initial value has to be a "
                                   "constant");
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
typedef struct { int fn, offset; } WalkFn;

static WalkFn *walk_fns;
static int     nwalk_fns, walk_fns_cap;

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
    gen_pending_clear();
}

static void walk_fns_at(int at)
{
    int i;

    for (i = 0; i != nwalk_fns; i++) {
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
    unsigned char bytes[8] = { 0 };
    uint64_t value = 0;
    int i, n = bf->bytes > ACC_LONG_SIZE ? 8 : ACC_LONG_SIZE;

    global_initializer(scalar == TY_BOOL ? TY_BOOL
                       : n > ACC_LONG_SIZE ? TY_ULLONG : TY_ULONG,
                       bytes);
    for (i = n - 1; i >= 0; i--)
        value = value << 8 | bytes[i];
    if (bf->width < 64)
        value &= ((uint64_t) 1 << bf->width) - 1;
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
        global_initializer(scalar, init_bytes + offset);
    if (gen_pending_sym != SYM_NONE) {
        walk_fn_add(gen_pending_sym, offset);
        gen_pending_clear();
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
static void literal_bytes_in(Type type, int x, int count, Type elem,
                             int elem_x, int line, const char *spot,
                             int address, int *countp);

/* A literal inside a global's initialiser -- `&(struct B) { &(struct A)
 * { 1, 2 } }` -- is built while that one is still being built, and they
 * shared the buffer and the list of addresses in it. The literal started
 * the buffer again from nothing, so whatever the outer one had written was
 * lost, and put down the outer one's addresses at its own place in the
 * image. So the literal gets a buffer and a list of its own, and the outer
 * one's are put back after, as a local literal does with InitState. */
__attribute__((noinline))
void literal_bytes(Type type, int x, int count, Type elem, int elem_x,
                          int line, const char *spot, int address, int *countp)
{
    unsigned char *bytes = init_bytes;
    int bytes_cap = init_bytes_cap, bytes_len = init_bytes_len;
    WalkFn *walks = walk_fns;
    int walks_n = nwalk_fns, walks_cap = walk_fns_cap;
    InitState outer;

    init_save(&outer);
    init_bytes = NULL;
    init_bytes_cap = 0;
    init_bytes_len = 0;
    walk_fns = NULL;
    nwalk_fns = 0;
    walk_fns_cap = 0;

    literal_bytes_in(type, x, count, elem, elem_x, line, spot, address,
                     countp);

    free(init_bytes);
    free(walk_fns);
    init_bytes = bytes;
    init_bytes_cap = bytes_cap;
    init_bytes_len = bytes_len;
    walk_fns = walks;
    nwalk_fns = walks_n;
    walk_fns_cap = walks_cap;
    init_restore(&outer);
}

static void literal_bytes_in(Type type, int x, int count, Type elem,
                             int elem_x, int line, const char *spot,
                             int address, int *countp)
{

        int at, total, i;

        init_bytes_len = 0;
        if (count && braced_string(elem)) {     /* `(char []){ "foo" }` */
            int n = init_string(elem, count, 0, global_put);

            braced_string_end();
            if (count < 0)
                count = n;
            total = count * type_bytes(elem, elem_x);
        } else if (count) {
            int n;

            expect(TK_LBRACE, "'{'");
            n = init_list(elem, elem_x, count, 0, global_put, 0);

            if (count < 0) {
                if (n == 0)
                    acc_error_spot(line, spot,
                                   "a compound literal of an array with no "
                                   "size needs a value to say how long it is");
                count = n;
            }
            total = count * type_bytes(elem, elem_x);
        } else if (type_is_struct(type)) {
            expect(TK_LBRACE, "'{'");
            init_record(x, 0, global_put, 1);
            total = ext_bytes(x);
        } else {
            expect(TK_LBRACE, "'{'");
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
                         int line, const char *spot)
{
    int step = type_bytes(elem, elem_x), total, at, sym, i, braces;
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
        && !(init && (tok == TK_STRING
                      || ((type_is_char(elem) || type_is_wchar(elem))
                          && tok == TK_LBRACE && lex_string_follows())))) {
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
                    global_initializer(elem, bytes);
                    accept(TK_COMMA);
                    expect(TK_RBRACE, "'}'");
                } else {
                    global_initializer(elem, bytes);
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
                    acc_error_spot(line, spot,
                                   "an array needs at least one element");
                count = n;
            }
        } else if (count < 0) {
            acc_error_spot(line, spot, "an array declared with [] needs "
                                       "initial values to say how long it is");
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

    braces = init && braced_string(elem);       /* `{ "foo" }` */
    if (init && tok == TK_STRING && (type_is_char(elem) || type_is_wchar(elem))) {
        int n = init_string(elem, count, 0, global_put);

        if (count < 0)
            count = n;
        if (braces)
            braced_string_end();
    } else if (init) {
        int n;

        expect(TK_LBRACE, "'{'");
        n = init_list(elem, elem_x, count, 0, global_put, 0);
        if (count < 0) {
            if (n == 0)
                acc_error_spot(line, spot, "an array needs at least one "
                                           "element");
            count = n;
        }
    } else if (count < 0) {
        acc_error_spot(line, spot, "an array declared with [] needs initial "
                                   "values to say how long it is");
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
static void global_struct(int x, NameRef name, int line, const char *spot)
{
    int total, at, sym, i;

    record_complete(x, line, spot);
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

/* A file-scope variable's bytes, from its initialiser if it has one, and
 * its name bound to them -- or, when redefining, written over the bytes it
 * already has.
 *
 * Between two functions is as good a place as any: nothing runs into it,
 * since every function ends in a return, and the address is known the moment
 * it is written, so nothing that refers to it ever needs patching. A global
 * with no initial value is zero, as C says, and takes its bytes in the image
 * like any other -- there is no separate zeroed area yet. */
void global_emit(Type type, int ext, NameRef name, int count, int line,
                        const char *spot)
{
    unsigned char bytes[8] = { 0 };
    int size = type_scalar_bytes(type), sym, i, at;

    if (count) {
        global_array(type, ext, name, count, line, spot);

        return;
    }
    if (type_is_struct(type)) {
        global_struct(ext, name, line, spot);

        return;
    }

    not_void(type, "a variable", line, spot);
    if (accept(TK_ASSIGN)) {
        /* A scalar's value may be in braces, `int m = {0};` (C99
         * 6.7.8p11), and a comma may end it. */
        int braced = accept(TK_LBRACE);

        global_initializer(type, bytes);
        if (braced) {
            accept(TK_COMMA);
            expect(TK_RBRACE, "'}'");
        }
    }

    at = out_here();
    data_fn_at(at);
    for (i = 0; i < size; i++)
        out_byte(bytes[i]);

    sym = push_global(name, decl_const ? SYM_GLOBAL_CONST : SYM_GLOBAL, at);
    sym_at(sym)->type = type;
    sym_at(sym)->ext = (unsigned char) ext;
}
