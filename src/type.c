/*
 * Types: specifiers and qualifiers, struct, union and enum, tags and their
 * scopes, constant expressions, arrays and variable-length arrays,
 * declarators and type names.
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

/* ------------------------------------------------------------------ */
/* types                                                               */

/* The qualifiers the type base_type last read had, as SQ_CONST and
 * SQ_VOLATILE bits. const is kept for the variable a declaration names,
 * which is refused as the target of an assignment; what a pointer points
 * at being const is taken and not checked. */
unsigned char base_const;
typedef char qualifiers_are_adjacent[(TK_KW_VOLATILE == TK_KW_CONST + 1
                                      && TK_KW_RESTRICT == TK_KW_CONST + 2)
                                     ? 1 : -1];

/* The qualifiers, as many as there are, with the base_type's const and
 * volatile marked: see base_const.
 *
 * volatile asks that every access the program says be made. It is kept as
 * a bit, SQ_VOLATILE, on what is declared with it anywhere in its type --
 * the object and all it leads to taken as volatile, more than C asks and
 * never less -- with SQ_OWN_VOLATILE beside it where the object itself is
 * (decl_quals_of), and a value read through it takes it as VQ_VOLATILE: such
 * a local is read from its slot every time, never taken from the register
 * that wrote it, and read though nothing uses it (gen_discard); opt-acc
 * keeps the first pass's code for any function touching one. restrict
 * promises something acc makes no use of, so it changes nothing. */
/* And the rest of a declaration's specifiers after its first, in any
 * order, as C allows: `const static int`, `int static`, `struct s extern`.
 * The storage classes and inline follow the qualifiers in the token list,
 * so one compare covers the nine. C99 6.11.5 calls a storage class that is
 * not first obsolescent, and obsolescent is still C. */
#define tok_specifier_word()  ((unsigned char) (tok_low - TK_KW_TYPEDEF) < 9u)
#define tok_storage()         ((unsigned char) (tok_low - TK_KW_TYPEDEF) < 5u)
typedef char storage_then_qualifiers[(TK_KW_STATIC == TK_KW_TYPEDEF + 1
                                      && TK_KW_REGISTER == TK_KW_TYPEDEF + 4
                                      && TK_KW_CONST == TK_KW_TYPEDEF + 5
                                      && TK_KW_INLINE == TK_KW_TYPEDEF + 8)
                                     ? 1 : -1];

/* Whether the specifiers being read are a declaration's, which is the only
 * place a storage class or inline may be, and the storage class they have
 * said: the one in front, which the caller read and put here, or one found
 * among them. A declaration sets storage_ok around its base_type. A struct's
 * or an enum's body, which reads types of its own inside those specifiers,
 * clears it for the length of the body -- once a body rather than once a
 * type, since base_type runs for every declaration in the program. */
unsigned char storage_ok;
int           decl_storage;
unsigned char decl_inline;       /* `inline` was among them */

__attribute__((noinline))
static void qualifiers(void)
{
    while (tok_specifier_word()) {
        if (tok == TK_KW_CONST) {
            base_const |= SQ_CONST;
        } else if (tok == TK_KW_VOLATILE) {
            base_const |= SQ_VOLATILE | SQ_OWN_VOLATILE;
        } else if (!tok_qualifier()) {
            if (!storage_ok)
                acc_error_at(tok_line, "'%s' belongs at the front of a "
                                       "declaration, not here",
                             tok_spelling(tok));
            if (tok == TK_KW_INLINE)
                decl_inline = 1;
            if (tok_storage()) {
                if (decl_storage)
                    acc_error_at(tok_line, "a declaration can have one "
                                           "storage class, and this has '%s' "
                                           "and '%s'",
                                 tok_spelling(decl_storage),
                                 tok_spelling(tok));
                decl_storage = tok;
            }
        }
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
static Type type_specifier_slow(int first, int line, const char *spot)
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
        if (tok_specifier_word())
            qualifiers();
        if (!starts_type(tok))
            break;
        token = tok;
        next();
    }

    if (is_bool) {
        if (is_bool > 1 || is_void || is_char || is_short || is_int || is_long
            || is_signed || is_unsigned || is_float)
            acc_error_spot(line, spot, "'_Bool' with another type");

        return TY_BOOL;
    }
    if (is_long > 2)
        acc_error_spot(line, spot, "'long long long' is not a type");
    if (is_signed && is_unsigned)
        acc_error_spot(line, spot, "'signed' and 'unsigned' together");
    if (is_void && (is_char || is_short || is_int || is_long || is_signed || is_unsigned))
        acc_error_spot(line, spot, "'void' with another type");
    if (is_char && (is_short || is_long))
        acc_error_spot(line, spot, "'char' with another width");
    if (is_short && is_long)
        acc_error_spot(line, spot, "'short' and 'long' together");

    if (is_float) {
        if (is_char || is_short || is_int || is_signed || is_unsigned)
            acc_error_spot(line, spot, "a floating type with an integer one");
        if (is_long)
            acc_error_spot(line, spot, "'long double' is not supported: "
                                       "agondev's library has no arithmetic "
                                       "for one, so a program that asks for "
                                       "it does not link");

        return TY_FLOAT;
    }
    if (is_long > 1) {
        if (is_char || is_short)
            acc_error_spot(line, spot, "'long long' with another width");

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
    const char *spot = tok_at;
    int first = tok;
    unsigned char alone = spec_alone[first];

    next();
    if (!starts_type(tok)) {
        if (!tok_specifier_word())
            return (Type) (alone - 1);  /* one keyword, which is most of them */
        qualifiers();                   /* `int const`, `unsigned const int` */
        if (!starts_type(tok))
            return (Type) (alone - 1);
    }

    return type_specifier_slow(first, line, spot);
}

/* ------------------------------------------------------------------ */
/* enums, tags and scopes                                              */

/* Whether the parser is inside a function body, where an enum constant, a
 * typedef or a tag belongs to the block it is declared in. */
int in_body;

/* The mark of the innermost block, for telling a redeclaration in the same
 * scope -- an error -- from one that shadows an outer name. -1 at file
 * scope. */
int scope_mark = -1;

/* Where a function's parameters begin, for its body's block to take as
 * where its scope does: they are one scope (C99 6.2.1p4). -1 otherwise. */
int body_mark = -1;

int push_here(NameRef name, int kind, int val)
{
    if (!in_body)
        return sym_push(name, kind, val);
    return sym_push_local(name, kind, val);
}

/* That `name` is not already declared in the scope being declared into. */
void not_redeclared(NameRef name, int line, const char *spot)
{
    int sym = sym_find(name);

    if (sym != SYM_NONE && sym_declared_in(sym, scope_mark))
        acc_error_spot(line, spot, "'%s' is already declared",
                       name_text(name));
}

/* The same for a block's own variable, which has no linkage and so may be
 * declared once in its scope (C99 6.7p3) -- where a function's outermost
 * block is the scope its parameters are in too (6.2.1p4): `void f(int x) {
 * int x; }` declares x twice. A load says so: see sym_find_in. */
__attribute__((noinline))
void local_redeclared(NameRef name, int line, const char *spot)
{
    if (sym_find_in(name, scope_mark) != SYM_NONE)
        acc_error_spot(line, spot, "'%s' is already declared",
                       name_text(name));
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
static int tag_find(NameRef tag, int kind, NameRef name, int line,
                    const char *spot)
{
    int sym = sym_find(tag);

    if (sym != SYM_NONE && sym_at(sym)->val != kind)
        acc_error_spot(line, spot, "'%s' is declared as a %s tag, not a %s "
                                   "one", name_text(name),
                       tag_keyword[sym_at(sym)->val], tag_keyword[kind]);

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
    const char *spot = tok_at;
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
            acc_error_spot(line, spot, "'enum' needs a name or a list of "
                                       "constants");
        sym = tag_find(tag, TAG_ENUM, name, line, spot);
        if (sym == SYM_NONE)
            acc_error_spot(line, spot, "'enum %s' is not defined",
                           name_text(name));

        return sym_at(sym)->type;
    }

    while (tok != TK_RBRACE) {
        int cline = tok_line;
        const char *cspot = tok_at;
        NameRef constant = declared_name();

        if (accept(TK_ASSIGN)) {
            unsigned char outer = storage_ok;

            storage_ok = 0;             /* a cast in it is not a declaration */
            val = constant_int("an enum constant's value", cline);
            storage_ok = outer;
        }
        if (val > 0x7fffff)
            acc_error_spot(cline, cspot, "an enum constant has to fit in an "
                                         "int");
        not_redeclared(constant, cline, cspot);

        /* The push on its own line, and its answer used after it. Written as
         * `sym_at(push_here(...))->type = ...` the compiler was free to read
         * the table's address before making the call that moves it, and then
         * wrote the type into memory the table no longer owned. Which is the
         * hazard sym.c names: a Sym * is only good until the next push, and
         * that holds for one the same expression is still working out. */
        sym = push_here(constant, SYM_CONST, val);
        sym_at(sym)->type = TY_INT;
        if (val < 0)
            negative = 1;
        val++;
        if (!accept(TK_COMMA))
            break;
    }
    expect(TK_RBRACE, "'}'");

    type = negative ? TY_INT : TY_UINT;
    if (tag) {
        not_redeclared(tag, line, spot);
        sym = push_here(tag, SYM_TAG, TAG_ENUM);
        sym_at(sym)->type = type;
    }

    return type;
}

int  base_ext;
static Type declarator_stars_out(Type base);
static NameRef direct_declarator_out(Type t, int tx, Type *type, int *ext,
                                     int *count);

/* The name a record is called by in a diagnostic: `struct point`, or
 * `struct` alone when it has no tag. */
const char *record_name(int x)
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
/* Record x has its members, or an error at `at`, on `line`: the token that
 * needs them. */
void record_complete(int x, int line, const char *spot)
{
    if (!ext_complete(x))
        acc_error_spot(line, spot, "'%s' is declared but its members are not "
                                   "given", record_name(x));
}

/* A record's members, from just past its `{`. Each is laid where the last
 * ended, with no padding: every type on this machine is aligned to a byte,
 * which is how agondev lays them out too. A union's are all at its start,
 * and it is as big as the biggest. */
/* A bit-field's width, from just past its `:`: an integer type, and no more
 * bits than it has -- one for a _Bool. Zero, which ends the byte the last
 * was in, only without a name. */
static int bitfield_width(Type type, NameRef name, int line,
                          const char *spot)
{
    int width = constant_int("a bit-field's width", line);
    int most = type == TY_BOOL ? 1 : type_scalar_bytes(type) * 8;

    if (type_pointer(type) || type_float(type) || type_is_struct(type)
        || type_is_array(type) || type == TY_VOID)
        acc_error_spot(line, spot, "a bit-field has to have an integer type");
    if (width < 0 || width > most)
        acc_error_spot(line, spot, "a bit-field of this type is 0 to %d bits "
                                   "wide", most);
    if (!width && name)
        acc_error_spot(line, spot, "a bit-field of no bits cannot have a "
                                   "name");

    return width;
}

static void record_members(int x, int is_union, int line, const char *spot)
{
    /* Where the next member goes: a byte, and a bit within it, for a
     * bit-field to continue from. */
    int first = -1, last = -1, size = 0, bit = 0, has_bits = 0, flexible = 0;
    int holds_flex = 0;

    while (tok != TK_RBRACE) {
        Type base = base_type();
        int bx = base_ext;
        unsigned char bc = base_const;

        /* A struct or union with no tag and no name: C11's anonymous
         * member, whose own members are reached as though they were this
         * record's -- see member_lookup. agondev's <agon/mos.h> has one in
         * SYSVAR. Otherwise a nested record declared, and nothing more. */
        if (accept(TK_SEMI)) {
            int m;

            if (!type_is_struct(base) || ext_tag(bx) != NAME_NONE)
                continue;
            record_complete(bx, tok_prev_line, NULL);
            if (!is_union && bit) {
                size++;
                bit = 0;
            }
            m = member_add(NAME_NONE, base, bx, is_union ? 0 : size,
                           bc);
            if (is_union) {
                if (type_bytes(base, bx) > size)
                    size = type_bytes(base, bx);
            } else {
                size += type_bytes(base, bx);
            }
            if (last >= 0)
                member_link(last, m);
            else
                first = m;
            last = m;
            continue;
        }
        for (;;) {
            int mline = tok_line, count = 0, ext = bx, bytes, m, width = -1;
            int at, pos;
            const char *mspot = tok_at; /* for an error about the member */
            Type type = base;
            NameRef name = NAME_NONE;

            if (tok != TK_COLON)        /* `int : 3` has no name */
                name = direct_declarator_out(declarator_stars_out(base),
                                             bx, &type, &ext, &count);
            if (vla_length)             /* C99 6.7.2.1p8 */
                acc_error_spot(mline, mspot, "a member's size has to be known "
                                             "as the program is compiled, and "
                                             "this array's length is worked "
                                             "out as it runs");
            if (!count)
                func_suffix(&type, &ext);
            if (type_is_func(type))
                acc_error_spot(mline, mspot, "a member cannot be a function; "
                                             "a pointer to one can");
            if (accept(TK_COLON))
                width = bitfield_width(type, name, mline, mspot);
            /* `char b[];` last in a struct: C99's flexible array member,
             * which takes no room and stands for whatever was allocated
             * past the struct. Only last, only in a struct, and only where
             * something else came first -- a struct of nothing but one has
             * no size to allocate from. */
            if (count < 0) {
                if (is_union)
                    acc_error_spot(mline, mspot,
                                   "a union's member that is an array needs "
                                   "its size");
                if (first < 0)
                    acc_error_spot(mline, mspot,
                                   "an array with no size has to come after "
                                   "another member");
                if (width >= 0 || !name)
                    acc_error_spot(mline, mspot,
                                   "an array with no size cannot be a "
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
                acc_error_spot(mline, mspot, "'void' is not a type a member "
                                             "can have");
            /* A struct that ends in an array with no size may not be a
             * struct's member (C99 6.7.2.1p2), but a union may hold one --
             * and is then held to the same rule itself, as the struct would
             * be: no member of a struct, no element of an array. */
            if (type_is_struct(type)) {
                record_complete(ext, mline, mspot);
                if (ext_has_flex(ext) && !is_union)
                    acc_error_spot(mline, mspot,
                                   "'%s' ends in an array with no size, so it "
                                   "cannot be a member", record_name(ext));
                if (ext_has_flex(ext))
                    holds_flex = 1;
            }
            for (m = first; name && m >= 0; m = member_next(m))
                if (member_name(m) == name)
                    acc_error_spot(mline, mspot,
                                   "'%s' is already a member of '%s'",
                                   name_text(name), record_name(x));

            if (width >= 0) {
                /* A bit-field: where the last ended, if it fits in a unit
                 * of its type from the byte that is in, and from the next
                 * byte if not -- every type is aligned to a byte, so a
                 * unit can start at any of them. Measured against agondev
                 * on a thousand and a half structs made at random. */
                int unit = type == TY_BOOL ? 8 : type_scalar_bytes(type) * 8;

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
                m = member_add(name, type, ext, at, bc | (stars_const & SQ_VOLATILE));
                member_set_bits(m, bitfield_intern(pos, width,
                                                   !type_unsigned(type)));
            } else {
                if (!is_union && bit) {         /* after bit-fields */
                    size++;
                    bit = 0;
                }
                bytes = type_bytes(type, ext);
                m = member_add(name, type, ext, is_union ? 0 : size,
                               bc | (stars_const & SQ_VOLATILE));
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
                acc_error_spot(mline, mspot, "an array with no size has to be "
                                             "the last member");
          placed:
            if (size > 0x7fffff)
                acc_error_spot(mline, mspot, "a struct this large does not "
                                             "fit in memory");
            if (!accept(TK_COMMA))
                break;
        }
        expect(TK_SEMI, "';'");
    }
    if (bit)
        size++;
    if (first < 0)
        acc_error_spot(line, spot, "a struct or union needs at least one "
                                   "member");
    ext_record_done(x, first, size);
    if (has_bits)
        ext_set_bits(x);
    if (flexible || holds_flex)
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
    const char *spot = tok_at;
    NameRef name = NAME_NONE, tag = NAME_NONE;

    kind = is_union ? TAG_UNION : TAG_STRUCT;
    next();
    if (tok == TK_IDENT) {
        name = tok_name;
        tag = tag_name(name);
        next();
    }

    if (tag) {
        sym = tag_find(tag, kind, name, line, spot);
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
            acc_error_spot(line, spot, "'%s' needs a name or a list of "
                                       "members",
                           is_union ? "union" : "struct");
        x = ext_record(is_union, NAME_NONE);
    }

    if (accept(TK_LBRACE)) {
        unsigned char outer = storage_ok, outer_storage = (unsigned char) decl_storage;

        if (ext_complete(x))
            acc_error_spot(line, spot, "'%s' is defined twice",
                           record_name(x));
        storage_ok = 0;
        record_members(x, is_union, line, spot);
        storage_ok = outer;
        decl_storage = outer_storage;
        expect(TK_RBRACE, "'}'");
    }
    base_ext = x;

    return TY_STRUCT;
}

__attribute__((noinline))
static Type base_type_other(void)
{
    int line = tok_line;
    const char *spot = tok_at;

    switch (tok) {
    case TK_KW_CONST:                   /* `const int`, `const struct s` */
    case TK_KW_VOLATILE:
    case TK_KW_RESTRICT:
    case TK_KW_TYPEDEF:                 /* and a declaration's storage class, */
    case TK_KW_STATIC:                  /* in front or anywhere else among */
    case TK_KW_EXTERN:                  /* its specifiers: see qualifiers */
    case TK_KW_AUTO:
    case TK_KW_REGISTER:
    case TK_KW_INLINE: {
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

        base_const = 0;
        qualifiers();

        return t;
    }
    case TK_KW_STRUCT:
    case TK_KW_UNION: {
        Type t = struct_specifier();

        /* A body of its own was read here, and every member in it went
         * through base_type -- so what base_const holds now is the last
         * member's and has nothing to do with this declaration. Left alone,
         * `struct { const char *p; }` made a const of everything declared
         * with it, and a typedef of it made a const of everything declared
         * with that. The same goes for an enum, whose constants can be
         * written with a sizeof or a cast in them. */
        base_const = 0;
        qualifiers();

        return t;
    }
    case TK_IDENT: {
        int sym = sym_find(tok_name);

        if (sym == SYM_NONE || sym_at(sym)->kind != SYM_TYPEDEF)
            break;
        next();
        base_ext = sym_at(sym)->ext;
        base_const = sym_at(sym)->quals & (SQ_CONST | SQ_VOLATILE | SQ_OWN_VOLATILE);
        qualifiers();

        return sym_at(sym)->type;
    }
    case TK_KW_RESERVED:
        reserved_word();
    }
    acc_error_spot(line, spot, "expected a type, found %s", tok_spelling(tok));
}

Type base_type(void)
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
unsigned char decl_start[TK_COUNT] = {
    [TK_KW_VOID] = 1, [TK_KW_CHAR] = 1, [TK_KW_SHORT] = 1, [TK_KW_INT] = 1,
    [TK_KW_SIGNED] = 1, [TK_KW_UNSIGNED] = 1, [TK_KW_LONG] = 1,
    [TK_KW_FLOAT] = 1, [TK_KW_DOUBLE] = 1,
    [TK_KW_ENUM] = 1, [TK_KW_STRUCT] = 1, [TK_KW_UNION] = 1,
    [TK_KW_TYPEDEF] = 1, [TK_KW_STATIC] = 1, [TK_KW_EXTERN] = 1,
    [TK_KW_STATIC_ASSERT] = 1,
    [TK_KW_AUTO] = 1, [TK_KW_REGISTER] = 1, [TK_KW_CONST] = 1,
    [TK_KW_VOLATILE] = 1, [TK_KW_RESTRICT] = 1, [TK_KW_INLINE] = 1,
    [TK_KW_BOOL] = 1,
    [TK_KW_VA_LIST] = 1
};

__attribute__((noinline))
int is_typedef_name(NameRef name)
{
    int sym = sym_find(name);

    return sym != SYM_NONE && sym_at(sym)->kind == SYM_TYPEDEF;
}

/* The qualifiers after the stars read: SQ_CONST where the last was
 * followed by `const` -- `int *const p`, which makes p itself const, where
 * `const int *p` does not -- and SQ_VOLATILE where any was by `volatile`. */
unsigned char stars_const;

/* The qualifiers after a star, and the volatile of those before it,
 * `prev`'s. */
__attribute__((noinline))
unsigned char star_qualifiers(unsigned char prev)
{
    unsigned char quals = prev & SQ_VOLATILE;

    while (tok_qualifier()) {
        if (tok == TK_KW_CONST)
            quals |= SQ_CONST;
        if (tok == TK_KW_VOLATILE)
            quals |= SQ_VOLATILE | SQ_OWN_VOLATILE;
        next();
    }

    return quals;
}

/* The SQ_* a declaration records of what it names: its base type's, the
 * volatile of any of its stars, and SQ_OWN_VOLATILE where what it names is
 * itself volatile -- by its last star where it has stars, by its base type
 * where it has none. A pointer to volatile is not: what it leads to is read
 * and written each time, the pointer itself is a value like any other. */
__attribute__((noinline))
unsigned char decl_quals_of(Type stars, Type base, unsigned char bc)
{
    return (bc & ~SQ_OWN_VOLATILE) | (stars_const & SQ_VOLATILE)
           | ((stars != base ? stars_const : bc) & SQ_OWN_VOLATILE);
}

/* ------------------------------------------------------------------ */
/* statements and declarations                                         */

/* A constant integer a declaration needs now: an array's size. Parsed as an
 * expression, which has to fold to a constant with nothing emitted, as a
 * global's initial value does. */
/* A long or a long long constant where an int one is wanted, which C
 * allows of any integer constant expression: `-8388608`, which is <limits.h>'s
 * INT_MIN and a long because 8388608 is too big for an int before the minus
 * makes it fit, or `2L`. Its value, if an int holds it. */
__attribute__((noinline))
static int constant_wide(const char *what, int line, const char *spot)
{
    uint64_t bits;
    int64_t v;
    Type type;

    if (!vconst_wide(&bits, &type) || type_float(type))
        acc_error_spot(line, spot, "%s has to be a constant integer", what);
    v = type_unsigned(type) ? (int64_t) bits
      : type_scalar_bytes(type) == 4 ? (int64_t) (int32_t) (uint32_t) bits
      : (int64_t) bits;
    /* Written as long long: on the Agon 0x800000 does not fit an int, so
     * it is unsigned, and minus it is 8388608 again. */
    if (v < -0x800000LL || v > 0xffffffLL)
        acc_error_spot(line, spot, "%s has to fit in an int", what);

    return (int) v;
}

/* The type of the constant constant_folded read last. */
Type folded_type;

/* The constant that was read from `at`, on `line`. */
int constant_folded(const char *what, int line, const char *spot,
                           int before)
{
    int val;
    Type type;

    if (!vconst_top(&val, &type)) {
        val = constant_wide(what, line, spot);
        type = TY_INT;
    }
    if (out_here() != before || type_pointer(type) || type_float(type))
        acc_error_spot(line, spot, "%s has to be a constant integer", what);
    vdrop();
    folded_type = type;

    return val;
}

/* An error about it is at the expression, whatever line the caller was
 * given for the construct it is part of. */
int constant_int(const char *what, int line)
{
    int before = out_here();
    Type outer = narrow_dest;
    const char *spot = tok_at;

    line = tok_line;
    narrow_dest = 0;
    conditional();              /* `?:` too: it is a constant expression when
                                 * its condition and the side it picks are */
    narrow_dest = outer;

    return constant_folded(what, line, spot, before);
}

/* Whether the array just read has a length the program works out: its
 * value is on the value stack, waiting for the declaration to take the room
 * for it. */
int vla_length;

int vla_mark = NO_VLA_MARK;

/* vla_top is one past the innermost open block, and is what every block
 * touches: an entry is six bytes, and `vla_blocks[nvla_blocks]` scales
 * the count by six, which on this target is a call into the runtime --
 * at the opening of every block in the program. The pointer is stepped
 * instead, and the count kept beside it for the rarer goto. */
VlaBlock *vla_blocks, *vla_top;
int       nvla_blocks, vla_blocks_cap, vla_serial;

/* Room for more blocks, out of the way of every block's opening. */
__attribute__((noinline))
static void vla_blocks_grow(void)
{
    vla_blocks_cap = vla_blocks_cap ? vla_blocks_cap * 2 : 16;
    vla_blocks = realloc(vla_blocks,
                         (size_t) vla_blocks_cap * sizeof *vla_blocks);
    if (!vla_blocks)
        acc_error("out of memory for blocks");

    /* No block has declared anything yet: see VlaBlock's vm_last. */
    memset(vla_blocks + nvla_blocks, 0,
           (size_t) (vla_blocks_cap - nvla_blocks) * sizeof *vla_blocks);
    vla_top = vla_blocks + nvla_blocks;
}

void vla_block_open(void)
{
    if (nvla_blocks == vla_blocks_cap)
        vla_blocks_grow();
    vla_top->serial = ++vla_serial;
    vla_top->mark = NO_VLA_MARK;
    vla_top++;
    nvla_blocks++;
}

/* A variably modified declaration in the innermost block: an array whose
 * length is worked out, a pointer to one, a typedef of one. */
void vm_declared(void)
{
    if (nvla_blocks)
        vla_top[-1].vm_last = ++vla_serial;
}

/* The same for one whose type may be: asked only of a declaration with an
 * extension, which a plain int has not. */
__attribute__((noinline))
void vm_maybe(int ext)
{
    if (ext_variably_modified(ext))
        vm_declared();
}

/* The block's mark, made if this is the first array in it to need one. */
void block_vla_mark(void)
{
    if (vla_mark != NO_VLA_MARK)
        return;
    vla_mark = gen_local(ACC_PTR_SIZE);
    if (nvla_blocks)
        vla_top[-1].mark = vla_mark;
    gen_stack_mark(vla_mark);
}

/* Whether a parameter list is being read, which is the only place C99 lets
 * `static` and the qualifiers stand inside an array's brackets. */
int in_params;

/* A parameter's array size that is not a constant: `int a[][n]`, whose
 * rows are n ints long. C99 6.9.1p10 works it out each time the function
 * is entered, and acc reads the parameters before there is a function to
 * work it out in -- so the size is kept as text, its tokens spelled again
 * into vla_texts as they are read, each ended by a `;`, and a definition
 * reads them all again once its frame is made: vla_params_enter. Every one
 * is worked out there, a first dimension's too, since `int a[n++]`
 * increments n whatever the parameter is.
 *
 * A row made with one -- or made of rows that are -- is an array of unknown
 * size until then, which is all a prototype's is: vla_param makes it a
 * type of its own, noted here with the text its length is (or the
 * constant, for a row of such rows) for vla_params_enter to fill in. The
 * notes are in the order the rows are made, which is inner rows first. */
#define VLA_PARAMS 16

static char          vla_texts[256];
int           vla_texts_used;
unsigned char nvla_texts;
static unsigned char vla_text_now;      /* the text array_size just kept, +1 */
static unsigned char vla_marked;        /* array_brackets began keeping one */
static unsigned char vla_keeping;       /* and array_size is reading it */

static unsigned char vla_param_ext[VLA_PARAMS];
static signed char   vla_param_text[VLA_PARAMS];    /* -1: the count is it */
static int           vla_param_count[VLA_PARAMS];
unsigned char nvla_params;

/* An array's size from inside its brackets: the number if it is a constant,
 * and -1 with the length left on the value stack if it is not, which is
 * C99's variable-length array.
 *
 * A length that is not a constant is only an array inside a function. At
 * file scope there is nothing to work it out with, and in a parameter it
 * says nothing -- the parameter is a pointer either way -- so there the
 * expression is read for its syntax and thrown away with the code it
 * emitted. */
static int constant_wide(const char *what, int line, const char *spot);

static int array_size(const char *what, int line, int *variable)
{
    GenMark mark;
    char *kept = NULL;
    int before = out_here(), effects, recording;
    Type outer = narrow_dest;
    int val;
    Type type;
    const char *spot = tok_at;          /* the size's, for an error */

    line = tok_line;
    *variable = 0;
    vla_text_now = 0;
    recording = vla_marked;
    vla_marked = 0;
    vla_keeping = (unsigned char) recording;
    gen_mark(&mark);
    effects = gen_effects;
    narrow_dest = 0;
    conditional();              /* `?:` as well, which is how C99 asserts at
                                 * build time: an array of `cond ? 1 : -1` */
    narrow_dest = outer;
    if (recording) {
        kept = lex_record_take();
        vla_keeping = 0;
    }

    if (vconst_top(&val, &type) && out_here() == before
        && !type_pointer(type) && !type_float(type)) {
        vdrop();

        return val;
    }
    {
        uint64_t bits;                  /* a long constant: `a[2L]` */

        if (out_here() == before && vconst_wide(&bits, &type)
            && !type_float(type)) {
            val = constant_wide(what, line, spot);
            vdrop();

            return val;
        }
    }
    if (!in_body || in_params) {
        gen_rollback(&mark);

        /* C99 6.9.1 evaluates a parameter's array size on entry to the
         * function -- `int a[n++]` increments n -- and acc reads the size
         * before there is a function to evaluate it in. So its text is
         * kept, for the definition to read again then: see vla_texts. */
        if (kept) {
            memcpy(kept, "\n;", 3);            /* after a `//` comment too */
            vla_texts_used = (int) (kept + 2 - vla_texts);
            vla_text_now = ++nvla_texts;

            return -1;
        }
        /* One not kept -- too long for vla_texts, or ended inside a
         * macro it did not begin in -- is thrown away, which is only right
         * when there was nothing in it to lose. */
        if (in_params && gen_effects != effects)
            acc_error_spot(line, spot, "a parameter's array size is evaluated "
                                       "when the function is entered, and acc "
                                       "could not keep this one to do that");
        if (in_params)
            return -1;                  /* `int a[n]` is `int *a` */
        acc_error_spot(line, spot, "%s has to be a constant integer", what);
    }
    if (type_pointer(vtype()) || type_float(vtype()))
        acc_error_spot(line, spot, "an array's length has to be an integer");
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
/* That the brackets just read held `[*]`: a length, not said, rather than
 * none at all -- which only the first dimension may leave out. */
static unsigned char star_length;

static void array_brackets(int line, const char *spot)
{
    int had_static = 0, had_qualifier = 0;

    for (;;) {
        /* A parameter's size is kept as the text after the `[` and the
         * words inside it, for array_size: see vla_texts. */
        if (in_params && !vla_keeping && nvla_texts < VLA_PARAMS) {
            lex_record_from(vla_texts + vla_texts_used,
                            vla_texts + sizeof vla_texts - 2);  /* `{` */
            vla_marked = 1;
        }
        next();
        if (tok == TK_KW_STATIC) {
            if (had_static)
                acc_error_spot(line, spot, "'static' twice inside []");
            had_static = 1;
        } else if (tok_qualifier()) {
            had_qualifier = 1;
        } else {
            break;
        }
    }
    if (tok == TK_RBRACKET) {
        vla_marked = 0;                 /* no size to keep */
        lex_record = NULL;
    }

    /* `[*]`, `[const *]`: a length that varies and is not said, which C99
     * 6.7.5.2p4 allows in a prototype's parameter. A parameter's first
     * dimension is a pointer whatever it says, so it is read as `[]`.
     * c-testsuite's 00162 declares one. */
    star_length = 0;
    if (tok == TK_STAR && lex_rbracket_follows()) {
        if (!in_params)
            acc_error_spot(line, spot, "'[*]' belongs in a function's "
                                       "parameters, where its length is said "
                                       "elsewhere");
        if (had_static)
            acc_error_spot(line, spot, "'static' says how many elements there "
                                       "are at least, and '[*]' says nothing");
        next();
        star_length = 1;
        vla_marked = 0;
        lex_record = NULL;

        return;
    }
    if (!had_static && !had_qualifier)
        return;
    if (!in_params)
        acc_error_spot(line, spot, "'static' and qualifiers inside [] say "
                                   "something about a parameter, and this is "
                                   "not one");
    if (had_static && tok == TK_RBRACKET)
        acc_error_spot(line, spot, "'static' inside [] needs the number the "
                                   "caller passes at least");
}
/* The dimensions after a name being declared: `[N]`, `[]`, `[N][M]` and so
 * on. Returns how many there were. When there were any, *count is the number
 * of elements -- -1 for `[]`, which only the first may be -- and *elem their
 * type, which for an array of arrays is itself an array type: `int m[3][4]`
 * is three elements of int[4]. */

/* A row whose length or whose element's size the program works out: its
 * length -- the slot `length` holds it, or it is the constant `count` --
 * and its size, that times its element's, each put in a frame slot where
 * the declaration is, which is when C99 says they are worked out. */
__attribute__((noinline))
static int vla_size(Type elem, int elem_x, int *length, int count)
{
    int size = gen_local(ACC_INT_SIZE), step = vla_size_slot(elem, elem_x);

    if (!*length) {
        *length = gen_local(ACC_INT_SIZE);
        vpush_const(count, TY_INT);
        vstore_local(*length, TY_INT);
        vdrop();
    }
    vpush_local(*length, TY_INT);
    if (step)
        vpush_local(step, TY_INT);
    else
        vpush_const(type_bytes(elem, elem_x), TY_INT);
    vapply(TK_STAR, 0);
    vstore_local(size, TY_INT);
    vdrop();

    return size;
}

int vla_type(Type elem, int elem_x, int length, int count)
{
    int size = vla_size(elem, elem_x, &length, count);

    return ext_vla(elem, elem_x, length, size);
}

/* A parameter's row whose length is the text vla_texts has at `text`, +1,
 * or -- when that is 0 -- `count`, a row of rows that are: see
 * vla_texts. */
__attribute__((noinline))
static int vla_param(Type elem, int elem_x, int text, int count)
{
    int x;

    if (nvla_params == VLA_PARAMS)
        acc_error_at(tok_line, "more than %d of a function's parameters' "
                               "rows have lengths worked out on entry",
                     VLA_PARAMS);
    x = ext_vla(elem, elem_x, -1, 0);
    vla_param_ext[nvla_params] = (unsigned char) x;
    vla_param_text[nvla_params] = (signed char) (text - 1);
    vla_param_count[nvla_params] = count;
    nvla_params++;

    return x;
}

/* A parameter's row: one with a length vla_texts has, or made of rows that
 * do. */
static inline __attribute__((always_inline))
int vla_param_row(Type elem, int elem_x, int text)
{
    return text || (type_is_array(elem) && ext_vla_pending(elem_x));
}

/* Their lengths and sizes, worked out on entry to the function, where C99
 * 6.9.1p10 says: each text read again, after the parameters and before the
 * body's `{`, which the current token is and which comes back after them. */
__attribute__((noinline))
void vla_params_enter(void)
{
    static int slots[VLA_PARAMS];
    int i;

    if (nvla_texts) {
        memcpy(vla_texts + vla_texts_used, "{", 2);
        lex_push_record(vla_texts, vla_texts_used + 1);
        next();
        for (i = 0; i != nvla_texts; i++) {
            int line = tok_line;
            const char *spot = tok_at;

            conditional();
            if (type_pointer(vtype()) || type_float(vtype()))
                acc_error_spot(line, spot, "an array's length has to be an "
                                           "integer");
            vconvert(TY_INT);
            slots[i] = gen_local(ACC_INT_SIZE);
            vstore_local(slots[i], TY_INT);
            vdrop();
            expect(TK_SEMI, "';'");
        }
    }
    for (i = 0; i != nvla_params; i++) {
        int x = vla_param_ext[i], length = 0, size;

        if (vla_param_text[i] >= 0)
            length = slots[vla_param_text[i]];
        size = vla_size(ext_elem(x), ext_elem_x(x), &length,
                        vla_param_count[i]);
        ext_vla_fill(x, length, size);
    }
}

int array_dims(Type base, int base_x, Type *elem, int *elem_x,
                      int *count)
{
    int dims[12], lengths[12], n = 0, i, first_line = 0;
    unsigned char texts[12];
    const char *first = NULL;           /* the first `[`, for an error */

    vla_length = 0;
    while (tok == TK_LBRACKET) {
        int line = tok_line, d = -1;
        const char *spot = tok_at;

        if (!n) {
            first_line = line;
            first = spot;
        }

        array_brackets(line, spot);
        if (n == 12)                    /* C99 5.2.4.1 asks for 12 */
            acc_error_spot(line, spot, "an array may have at most 12 "
                                       "dimensions");
        lengths[n] = 0;
        texts[n] = 0;
        if (tok != TK_RBRACKET) {
            int variable;

            d = array_size("an array's size", line, &variable);
            texts[n] = vla_text_now;
            if (variable) {
                /* Kept in the frame, since the rows' sizes are worked out
                 * from it once every dimension has been read. */
                lengths[n] = gen_local(ACC_INT_SIZE);
                vstore_local(lengths[n], TY_INT);
                vdrop();
            } else if (d < 0) {
                /* A parameter's `[n]`: the parameter is a pointer, as `[]`
                 * would have made it, and after the first, `int a[][n]`,
                 * the rows are arrays whose length is worked out when the
                 * function is entered -- C99 6.9.1p10 -- from the text
                 * array_size kept: see vla_texts. */
            } else if (d == 0) {
                acc_error_spot(line, spot, "an array needs at least one "
                                           "element");
            }
        } else if (n > 0 && !star_length) {
            acc_error_spot(line, spot, "only an array's first dimension may "
                                       "be left out");
        }
        expect(TK_RBRACKET, "']'");
        dims[n++] = d;
    }
    if (n == 0)
        return 0;

    if (base == TY_VOID)
        acc_error_spot(first_line, first, "an array of void has no elements "
                                          "to hold");
    if (type_is_struct(base)) {
        record_complete(base_x, first_line, first);
        if (ext_has_flex(base_x))
            acc_error_spot(first_line, first, "'%s' ends in an array with no "
                                              "size, so there is no saying "
                                              "where the next element would "
                                              "start", record_name(base_x));
    }
    *elem = base;
    *elem_x = base_x;
    for (i = n - 1; i >= 1; i--) {
        *elem_x = lengths[i] || vla_size_slot(*elem, *elem_x)
                  ? vla_type(*elem, *elem_x, lengths[i], dims[i])
                  : vla_param_row(*elem, *elem_x, texts[i])
                  ? vla_param(*elem, *elem_x, texts[i], dims[i])
                  : ext_array(*elem, *elem_x, dims[i]);
        *elem = TY_EXT;
    }
    *count = dims[0];

    /* A length the program works out, or rows whose size it does: the
     * length goes on the stack, for the declaration to take the room. */
    if (lengths[0] || vla_size_slot(*elem, *elem_x)) {
        if (lengths[0])
            vpush_local(lengths[0], TY_INT);
        else
            vpush_const(dims[0], TY_INT);
        vla_length = 1;
        *count = -1;

        return n;
    }
    if (*count > 0 && (long) *count * type_bytes(*elem, *elem_x) > 0x7fffff)
        acc_error_spot(first_line, first, "an array this large does not fit "
                                          "in memory");

    return n;
}

/* Whether a declarator may leave its name out, as a prototype's parameter
 * may: `int f(char *, int [4])`. */
int abstract_ok;

NameRef declared_name(void)
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
Type type_name_elem(int *x, int *count, Type *elem, int *elem_x)
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

    /* `sizeof(int[n][m])`, `sizeof(row)` with `typedef int row[n];`: an
     * array whose length the program works out, as a type of its own, its
     * size put in the frame here. */
    if (vla_length) {
        int length = gen_local(ACC_INT_SIZE);

        vstore_local(length, TY_INT);
        vdrop();
        vla_length = 0;
        *x = vla_type(type, *x, length, 0);
        *count = 0;

        return TY_EXT;
    }
    if (*count) {
        if (*count > 0)
            *x = ext_array(type, *x, *count);

        return TY_EXT;
    }

    return type;
}

Type type_name(int *x)
{
    Type elem;
    int count, elem_x;
    Type type = type_name_elem(x, &count, &elem, &elem_x);

    if (count < 0)
        acc_error_prev("an array type here needs its size");

    return type;
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
    int a, b, c;        /* an array's count, its length's slot, and its
                         * text in vla_texts; a function's parameter run,
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
        /* An array of unknown size is a type, `int (*p)[]` points at one;
         * but not an array's element, whose size a step needs -- except in
         * a parameter, where array_dims makes one of a row whose length
         * is worked out on entry. */
        if (type_is_array(*t) && !vla_size_slot(*t, *x) && ext_count(*x) < 0
            && !in_params)
            acc_error_at(tok_line, "only an array's first dimension may be "
                                   "left out");
        if (type_is_func(*t))
            acc_error_at(tok_line, "an array of functions is not a thing C "
                                   "has; an array of pointers to them is");
        if (type_is_struct(*t))
            record_complete(*x, tok_line, tok_at);
        *x = op->b || vla_size_slot(*t, *x) ? vla_type(*t, *x, op->b, op->a)
             : vla_param_row(*t, *x, op->c) ? vla_param(*t, *x, op->c, op->a)
                                            : ext_array(*t, *x, op->a);
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
NameRef *param_names;
int      param_names_cap;

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

/* Where the run a parameter list is being added as starts, from before its
 * entry `count` is added: `first` while the run is whole, which is nearly
 * always. A parameter that is a pointer to a function has a list of its
 * own, read -- and added -- in the middle of this one, and left there the
 * run had the other's inside it: `int take(double (*f)(double), long k)`
 * took f's double for take's first parameter and f for its second, so a
 * call converted the function to a float and k to a pointer. When that has
 * happened the run so far is added again past the other one, names and
 * all, and goes on from there; the copy it leaves behind is not used. */
/* How many lists with parameters in them param_types has begun, which is
 * how a list being read knows, with a compare and no call, that one was
 * read inside it and its run may have been broken. */
unsigned nested_lists;

__attribute__((noinline))
int params_keep_run(int first, int count)
{
    int now = sym_params_begin(), i;

    if (now == first + count)
        return first;
    for (i = 0; i < count; i++) {
        param_name_set(now + i, first + i < param_names_cap
                                ? param_names[first + i] : NAME_NONE);
        sym_param_add(sym_param_type(first, i), sym_param_ext(first, i));
    }

    return now;
}
static NameRef decl_full(void);

/* How deep in parentheses the declarator being read is, and how deep the
 * name was when decl_direct met it: the stars beside the name are the
 * object's own, and a `const` among them is what makes it const. In `void *
 * const (*p2)[2]` that const is the elements', and p2 is not. */
static unsigned char decl_depth, decl_name_depth;

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
        decl_name_depth = decl_depth;
        next();
    } else if (!abstract_ok) {
        acc_error_at(tok_line, "expected a name, found %s", tok_spelling(tok));
    }

    suffix = ndecl_ops;
    for (;;) {
        if (tok == TK_LBRACKET) {
            int n = -1, line = tok_line, length = 0, variable, text = 0;
            const char *spot = tok_at;

            array_brackets(line, spot);
            if (tok != TK_RBRACKET) {
                n = array_size("an array's size", line, &variable);
                text = vla_text_now;

                /* `int (*p)[m]`: a length the program works out, kept in
                 * the frame for when the type is put together. */
                if (variable) {
                    length = gen_local(ACC_INT_SIZE);
                    vstore_local(length, TY_INT);
                    vdrop();
                    n = 0;
                } else if (n == 0) {
                    acc_error_spot(line, spot,
                                   "an array needs at least one element");
                }
            }
            expect(TK_RBRACKET, "']'");
            decl_push(DECL_ARRAY, n, length, text);
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
    int stars = 0, n, last_const = 0;
    NameRef name;

    while (accept(TK_STAR)) {
        stars++;
        last_const = star_qualifiers(last_const);
    }
    for (n = stars; n > 0; n--)
        decl_push(DECL_PTR, 0, 0, 0);

    decl_depth++;
    name = decl_direct();
    decl_depth--;
    if (stars && decl_name_depth == decl_depth + 1)
        stars_const = (unsigned char) last_const;

    return name;
}

/* The parameter types of a function type in a declarator, from just past
 * its `(`: added to the parameter table as a run, with their names, if they
 * have them, read and put aside. Returns whether the parameters were given:
 * `()` does not give them. */
static int param_types(int *first, int *count)
{
    int saved_abstract = abstract_ok, saved_params = in_params, variadic = 0;
    int texts_used, ntexts, nrows, mark;
    unsigned seen;

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
    seen = ++nested_lists;
    texts_used = vla_texts_used;
    ntexts = nvla_texts;
    nrows = nvla_params;

    /* The parameters named so far are in scope for the ones after, whose
     * sizes may name them: `int (*f)(int n, int a[][n])`. */
    mark = sym_scope_begin();
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
        not_void(type, "a parameter", tok_line, tok_at);
        if (nested_lists != seen) {
            *first = params_keep_run(*first, *count);
            seen = nested_lists;
        }
        param_name_set(*first + *count, pname);
        sym_param_add(type, ext);
        if (pname) {
            int sym = sym_push(pname, SYM_LOCAL, 0);

            sym_at(sym)->type = type;
            sym_at(sym)->ext = (unsigned char) ext;
        }
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

    /* A list inside another's is a prototype's, whose sizes nothing works
     * out: `void (*p)(int n, int x[n])`. */
    sym_scope_end(mark);
    vla_texts_used = texts_used;
    nvla_texts = ntexts;
    nvla_params = nrows;
    expect(TK_RPAREN, "')'");

    return 1 | variadic;
}

/* A parameter list after a plain name, where it makes a function type:
 * `typedef int binary(int, int)`, a parameter `int g(int)`. Where a
 * function is being declared the caller reads the list itself. */
void func_suffix(Type *type, int *ext)
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
NameRef paren_declarator(Type t, int tx, Type *type, int *ext,
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
            if (op->op == DECL_ARRAY && i == ndecl_ops - 1) {
                if (type_is_func(t))
                    acc_error_at(tok_line, "an array of functions is not a "
                                           "thing C has; an array of pointers "
                                           "to them is");
                ndecl_ops = first;
                *type = t;
                *ext = tx;
                *count = op->a;

                /* An array whose length the program works out: the length
                 * on the stack, as array_dims leaves it. */
                if (op->b || vla_size_slot(t, tx)) {
                    if (op->b)
                        vpush_local(op->b, TY_INT);
                    else
                        vpush_const(op->a, TY_INT);
                    vla_length = 1;
                    *count = -1;
                }

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
void typedef_array(Type *type, int *ext, int *count)
{
    int x = *ext;

    *type = ext_elem(x);
    *ext = ext_elem_x(x);
    *count = ext_count(x);

    /* A typedef of an array whose length the program works out, `typedef
     * int row[n];`: the length it had where the typedef was, for the
     * declaration to take the room. */
    if (ext_vla_size(x)) {
        /* A parameter of the typedef's type is a pointer to its element,
         * as any array parameter is, and its length is nothing to it:
         * `void g1 (A);` with `typedef int A[n];` in a block. */
        if (in_params) {
            *count = -1;

            return;
        }
        if (!in_body)
            acc_error_at(tok_line, "an array whose length is worked out as "
                                   "it runs can only be declared in a "
                                   "function's body");
        vpush_local(ext_vla_length(x), TY_INT);
        vla_length = 1;
        *count = -1;
    }
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
