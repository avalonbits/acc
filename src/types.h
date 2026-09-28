/*
 * Types: how acc represents a C type, and the widths of the target.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ACC_TYPES_H
#define ACC_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* The target's own widths, which are also the host's when acc is compiled by
 * agondev to run on the Agon. Named rather than assumed, because the whole
 * difficulty of the previous compiler was code that assumed 4. */
#define ACC_INT_SIZE   3
#define ACC_PTR_SIZE   3
#define ACC_LONG_SIZE  4



/* An integer type is its width in bytes and whether it is signed, and that is
 * everything the code generator needs: the width says how many bytes a load
 * or a store touches, and the sign says how a narrow value is widened and how
 * two values compare.
 *
 * The widths are agondev's, so that a program compiled by both comes out the
 * same: char 1, short 2, int and pointers 3. long is 4 and long long 8, and
 * neither is here yet -- both need a value to live in more than one register,
 * which is a different piece of work from this one. */
typedef unsigned char Type;

/* Every identifier in the program is stored once, in one arena that grows and
 * is never freed. A name is referred to by its offset into that arena, which
 * is three bytes rather than a pointer plus a length plus a hash link. */
typedef unsigned int NameRef;
#define NAME_NONE ((NameRef) 0)

#define TY_SIZE_MASK  0x07              /* the width, in bytes */
#define TY_UNSIGNED   0x08

#define TY_CHAR     ((Type) 1)
#define TY_UCHAR    ((Type) (1 | TY_UNSIGNED))
#define TY_SHORT    ((Type) 2)
#define TY_USHORT   ((Type) (2 | TY_UNSIGNED))

/* _Bool: one byte, unsigned, read and stored as unsigned char is -- and
 * converting anything to it gives 0 or 1, which is the one thing about it
 * that is its own. The floating bit makes it a code no other type has. */
#define TY_BOOL     ((Type) (TY_FLOATING | TY_UNSIGNED | 1))
#define TY_INT      ((Type) ACC_INT_SIZE)
#define TY_UINT     ((Type) (ACC_INT_SIZE | TY_UNSIGNED))
#define TY_LONG     ((Type) ACC_LONG_SIZE)
#define TY_ULONG    ((Type) (ACC_LONG_SIZE | TY_UNSIGNED))

/* long long: eight bytes, which the three bits of the width cannot say. So
 * it has a width no other type has, 6, and type_bytes turns that into the
 * eight it stores as. type_size is left alone: it is on every path for every
 * other type, and a long long never needs its answer except where it is
 * stored, which is what type_bytes and type_wide_bytes are for. */
#define TY_LLONG    ((Type) 6)
#define TY_ULLONG   ((Type) (6 | TY_UNSIGNED))
#define type_eight(ty)     (type_size(ty) == 6)

/* float and double are one type here: agondev makes both four-byte IEEE 754
 * single precision, and compiles double arithmetic to the same routines. So
 * there is one floating type, four bytes wide, and the flag that marks it has
 * to live outside the width field -- which is why it is 0x10 rather than a
 * size the width could be confused with. */
#define TY_FLOATING 0x10
#define TY_FLOAT    ((Type) (ACC_LONG_SIZE | TY_FLOATING))

/* A pointer is never a floating type, however floating the thing it points
 * at, and an array type is never one either, although some of the codes
 * arrays are given have the floating bit set. So it is the whole byte that is
 * compared: float is one type, and nothing else is equal to it. One compare,
 * as the mask it replaces was. */
#define type_float(ty)    ((Type) (ty) == TY_FLOAT)
#define TY_VOID     ((Type) 0)

/* How many times a type is a pointer, in the top three bits.
 *
 * A type is one byte and has to stay one byte: a Sym is eight, which is what
 * makes indexing the symbol table a shift rather than a multiply, and a
 * ninth byte would round it up to twelve. So the pointer depth shares the
 * byte with what it points at -- `int *` is a depth of one over TY_INT, and
 * `char **` a depth of two over TY_CHAR -- and the width of the pointer
 * itself is not stored at all, because every pointer on this machine is the
 * same three bytes an int is.
 *
 * Seven levels. Nobody writing C for a machine with 512 KB of address space
 * is going to miss the eighth.
 */
#define TY_PTR_SHIFT  5
#define TY_PTR_MASK   0xe0
#define TY_PTR_MAX    7

#define type_pointer(ty)   ((ty) & TY_PTR_MASK)
#define type_ptr_to(ty)    ((Type) ((ty) + (1 << TY_PTR_SHIFT)))
#define type_deref(ty)     ((Type) ((ty) - (1 << TY_PTR_SHIFT)))
#define type_ptr_depth(ty) (((ty) & TY_PTR_MASK) >> TY_PTR_SHIFT)

/* A long is wider than any register, so it never lives in one: it stays in
 * the frame and the code generator works on it there. type_wide says which
 * values that applies to. A pointer never does, however wide the thing it
 * points at. */
#define type_wide(ty)     (type_size(ty) > ACC_INT_SIZE)

/* The bytes a wide value takes in the frame: four, or eight for a long long
 * long. Only ever asked of a type that is already known to be
 * wide, so it reads the width field directly rather than going through
 * type_size, whose answer for a pointer cannot arise here. */
#define type_wide_bytes(ty) (((ty) & TY_SIZE_MASK) == 6 ? 8 : ACC_LONG_SIZE)

#define type_size(ty)     (type_pointer(ty) ? ACC_INT_SIZE \
                                            : (int) ((ty) & TY_SIZE_MASK))
/* An address has no sign, so a pointer is unsigned -- which is one mask,
 * for the same reason. */
#define type_unsigned(ty) ((ty) & (TY_PTR_MASK | TY_UNSIGNED))

/* Extended types: arrays as types, and later structs.
 *
 * A type is one byte, and has to stay one: every value the compiler handles
 * carries one, and the tests on it -- its width, its sign, whether it is a
 * pointer -- run constantly. Widening it to 16 bits measured 1.5-4% slower
 * on every program, and to 24 bits 2.5-5.4%, whether or not the program had
 * anything a byte could not say.
 *
 * So the byte stays, and one code in it, TY_EXT, means "look at the other
 * byte": an extension, carried beside the type in a Sym and a Value, that
 * indexes a table of what the byte cannot say. The hot code only ever sees
 * the first byte, which is why this costs nothing where it is not used. The
 * pointer depth still sits on top, so a pointer to a row is a pointer like
 * any other, and a value's extension always describes the TY_EXT at the
 * bottom of its chain of pointers: taking an address or reading through one
 * changes the depth and leaves the extension alone.
 *
 * An array is not a type a value ever has -- it becomes the address of its
 * first element wherever it is used -- so a one-dimensional array needs no
 * extension: its symbol records the element type. What needs one is an
 * array of arrays, whose elements are rows, and a pointer to a whole array,
 * which steps by one. The table has 255 entries. */
#define TY_EXT      ((Type) 7)          /* a width no scalar has */

#define type_is_array(ty)   ((Type) (ty) == TY_EXT)

/* A struct or a union, whose extension says which and what is in it.
 *
 * A value of one is never loaded: it is carried as its address, which is
 * what an assignment copies from, a member is found from, and an argument is
 * copied out of. So it has the width of an address, which keeps it off every
 * path for four-byte values, and the floating bit, which no int has and
 * which float, being four bytes, is not confused with. Wherever a value is
 * loaded as the number it is, a struct is refused: see struct_used. */
#define TY_STRUCT   ((Type) (TY_FLOATING | ACC_INT_SIZE))

#define type_is_struct(ty)  ((Type) (ty) == TY_STRUCT)

/* A function, as a type: what a function's name is before it becomes the
 * address it always becomes, and what a pointer to a function points at.
 * Its extension says what it returns and takes. No value ever has it: a
 * function's name is its address, a pointer, straight away. */
#define TY_FUNC     ((Type) (TY_FLOATING | 5))
#define type_is_func(ty)    ((Type) (ty) == TY_FUNC)

/* The size of any type, arrays and structs included, given its extension. */
#define type_bytes(ty, x)   (type_is_array(ty) || type_is_struct(ty) \
                             ? ext_bytes(x) : type_scalar_bytes(ty))
#define type_scalar_bytes(ty) (type_eight(ty) ? 8 : type_size(ty))

/* What `p + 1` moves by, which is the width of what p points at -- a whole
 * row, when that is an array. */
#define type_step(ty, x)    type_bytes(type_deref(ty), x)

int  ext_array(Type elem, int elem_x, int count);   /* elem[count]: its extension */
Type ext_elem(int x);           /* an array's element type */
int  ext_elem_x(int x);         /* and its extension */
int  ext_count(int x);
int  ext_bytes(int x);
int  ext_vla(Type elem, int elem_x, int length_slot, int size_slot);
int  ext_vla_size(int x);       /* the frame slot of a VLA's size, or 0 */
int  ext_vla_length(int x);     /* and of its length */
int  ext_vla_pending(int x);    /* a parameter's row, till its function starts */
int  ext_variably_modified(int x);  /* a VLA, or a type made from one */
void ext_vla_fill(int x, int length_slot, int size_slot);
int  ext_compatible(int a, int b);  /* the same, or arrays that could be */

int     ext_record(int is_union, NameRef tag);   /* a new, incomplete record */
int     ext_func(Type ret, int ret_x, int first, int count, int declared);
int     ext_func_first(int x);          /* its parameters' run */
int     ext_func_count(int x);
int     ext_func_declared(int x);       /* whether it gives them */
int     ext_func_variadic(int x);       /* and more, with `...` */
void    ext_record_done(int x, int first, int bytes);
int     ext_is_union(int x);
int     ext_complete(int x);
void    ext_set_bits(int x);            /* it has bit-fields */
int     ext_has_bits(int x);
void    ext_set_flex(int x);            /* and an array with no size last */
int     ext_has_flex(int x);
NameRef ext_tag(int x);                 /* NAME_NONE when it has none */

/* A record's members, in the order they were declared: member_first, then
 * member_next until -1. */
int     member_add(NameRef name, Type type, int ext, int offset, int quals);
int     member_quals(int member);
void    member_set_bits(int member, int bits);
int     member_bits(int member);

/* A bit-field: `width` bits from bit `pos` of the `bytes` bytes at its
 * member's offset. */
typedef struct {
    unsigned char pos, width, bytes, is_signed;
} BitField;

int             bitfield_intern(int pos, int width, int is_signed);
const BitField *bitfield_at(int i);
void    member_link(int member, int next);
int     member_first(int x);
int     member_next(int member);
int     member_find(int x, NameRef name);      /* -1 if it has none so named */
NameRef member_name(int member);
Type    member_type(int member);
int     member_ext(int member);
int     member_offset(int member);

/* C promotes anything narrower than int to int before doing arithmetic on it,
 * so a value in a register is always int-wide. Only loads, stores and casts
 * deal in the narrow widths. */
#define type_promote(ty)  ((Type) (type_size(ty) < ACC_INT_SIZE ? TY_INT : (ty)))

/* The type a byte or two of arithmetic may be done in, when the result is
 * on its way into an object of that type: see narrow_dest. Not _Bool, which
 * does not keep the low bits of what it is given: 2 in a _Bool is 1. */
#define type_narrow(ty)   ((Type) (type_size(ty) < ACC_INT_SIZE \
                                   && (ty) != TY_BOOL ? (ty) : 0))

/* Two pointers are the same type when they agree all the way down. Compared
 * whole rather than piecewise: the encoding puts the depth and what it points
 * at in the one byte, so equality of the byte is equality of the type. */
#define type_same(a, b)   ((a) == (b))

#endif
