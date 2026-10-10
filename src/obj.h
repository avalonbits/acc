/*
 * Object files and libraries. See obj.c and archive.c.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ACC_OBJ_H
#define ACC_OBJ_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* What a symbol in an object is: OBJ_DEFINED when this object has the thing
 * and says where, and OBJ_FUNC when it is a function rather than data. */
enum {
    OBJ_DEFINED = 1,
    OBJ_FUNC    = 2,
    OBJ_BSS     = 4,            /* a variable with no room in the text: its
                                 * value is where in this object's bss it
                                 * starts, and what address that comes to is
                                 * the linker's to work out */
    OBJ_WEAK    = 8,            /* wanted only if something else wants it:
                                 * a link does not go looking for it, and
                                 * left without one it is at zero */
    OBJ_WANT    = 16            /* a name the object holds no address of,
                                 * but wants in the program if a library
                                 * has it: see gen_want */
};

/* An object read in: the file's bytes, and where each part of it starts.
 * Nothing is copied out -- the accessors below read the bytes in place. */
typedef struct {
    unsigned char *all;
    const char    *path;
    unsigned char *syms, *relocs, *relocs_a, *deps, *items, *text;
    char          *strings;
    int            build;    /* the acc that made it: see src/build_id.sh */
    int            text_len, bss_len, bss_align, nsyms, nrelocs, nrelocs_a;
    int            ndeps, nitems, strings_len;
    void          *file;     /* open at the text, when it was left to be read */
} Object;

/* What a relocation's slot is made of: see the note on the format in
 * src/obj.c. acc writes ABS24 alone; the rest are for objects an
 * assembler writes. */
enum {
    REL_ABS24, REL_LOW8, REL_HIGH8, REL_UPPER8, REL_PCREL8, REL_ABS16,
    REL_KINDS
};

/* A library: objects end to end, with a list of what each defines in front
 * of them. Only that list is read when one is opened; a member is read when
 * the link turns out to want it, from the file kept open for it -- opening
 * one on the card is a search of its directory, and a link takes dozens. */
typedef struct {
    const char    *path;
    void          *file;                 /* a FILE, open until ar_close */
    unsigned char *front;                /* the list, as it is in the file */
    unsigned char *members, *defs;
    char          *strings;
    int            build, nmembers, ndefs, strings_len, size;
} Archive;

void        ar_write(const char *path, const char **members, int nmembers);
int         ar_open_if(const char *path, Archive *a);  /* 0 if not there */
void        ar_close(Archive *a);
int         ar_find(const Archive *a, const char *name);  /* which member, or -1 */
void        ar_member(const Archive *a, int i, Object *o);
const char *ar_member_name(const Archive *a, int i);

void obj_write(const char *path);
void obj_print_version(void);     /* -v */
extern const char *obj_map_path;  /* -map: the items by name, beside the object */
void obj_link_map_open(void);           /* and for a link, where each went */
int  obj_link_map_on(void);
void obj_link_map_item(int at, int size, const char *name, const char *from,
                       int offset);
void obj_link_map_close(void);
int  obj_item(const Object *o, int i);  /* where item i starts in the text */
int  obj_item_align(const Object *o, int i); /* log2 of its alignment */
int  obj_nrelocs(const Object *o);      /* both tables of them together */
int  obj_reloc_kind(const Object *o, int i);
long obj_reloc_addend(const Object *o, int i, const unsigned char *slot);
int  obj_reloc_width(int kind);         /* how many bytes its slot is */

/* One relocation, read whole: what obj_reloc_at, obj_reloc_sym and
 * obj_reloc_kind say of it, in one call where those are three. */
typedef struct {
    int  at;                /* where its slot is in the text */
    int  sym;               /* 0, 1, or one more than a symbol */
    int  kind;
    int  own;               /* its addend is its own (relocs_a): `addend` */
    long addend;
} ObjReloc;
void obj_reloc_read(const Object *o, int i, ObjReloc *r);
long obj_reloc_add(const ObjReloc *r, const unsigned char *slot);
const char *obj_c_name(const char *in_object);  /* _f is f; g is @g */
const char *obj_object_name(const char *in_acc);  /* and back */
void obj_take(unsigned char *all, int len, const char *path, Object *o);
int  obj_current(const char *path, const char *source);
void obj_read(const char *path, Object *o, int front);
void obj_abandon(void);             /* an object or library half written */
void obj_free(Object *o);

const char *obj_sym_name(const Object *o, int i);
int         obj_sym_value(const Object *o, int i);
int         obj_sym_flags(const Object *o, int i);
int         obj_reloc_at(const Object *o, int i);
int         obj_reloc_sym(const Object *o, int i);   /* 0, or one more than a symbol */
const char *obj_dep_path(const Object *o, int i);
void        obj_dep_marks(const Object *o, int i, unsigned *size,
                          unsigned *sum, unsigned *weighted);

#endif
