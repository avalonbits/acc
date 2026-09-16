/*
 *  eZ80 relocations for acc
 *
 *  Copyright (c) 2026 Igor Cananea
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef TARGET_DEFS_ONLY

/* Read off agondev's own objects: ELF32, little endian, machine 220. The
 * relocation numbering is binutils' elf32-z80. R_Z80_24 is the one that
 * matters -- a 24-bit absolute address is what an ADL-mode `call` or `ld`
 * carries, and it is 574 of the 574 relocations in a sample object. */
#define EM_TCC_TARGET   EM_Z80

#define R_Z80_NONE      0
#define R_Z80_8         1
#define R_Z80_8_DIS     2
#define R_Z80_8_PCREL   3
#define R_Z80_16        4
#define R_Z80_24        5
#define R_Z80_32        6
#define R_Z80_NUM       7

/* A pointer is three bytes here, so the relocation that writes one is the
 * 24-bit form and not the 32-bit one. */
#define R_DATA_32   R_Z80_32
#define R_DATA_PTR  R_Z80_24
#define R_JMP_SLOT  R_Z80_24
#define R_GLOB_DAT  R_Z80_24
#define R_COPY      R_Z80_NONE
#define R_RELATIVE  R_Z80_24

#define R_NUM       R_Z80_NUM

/* MOS loads a program at 0x40000 and there are no pages to align to. */
#define ELF_START_ADDR 0x00040000
#define ELF_PAGE_SIZE  0x1

#define PCRELATIVE_DLLPLT 0
#define RELOCATE_DLLPLT 0

#else /* !TARGET_DEFS_ONLY */

/* tcc_error and friends reach the compiler state through a local s1 unless a
 * translation unit asks for the global one. A backend has no s1 to hand. */
#define USING_GLOBALS
#include "tcc.h"

ST_FUNC int code_reloc (int reloc_type)
{
    switch (reloc_type) {
        case R_Z80_8_PCREL:
            return 1;

        case R_Z80_NONE:
        case R_Z80_8:
        case R_Z80_8_DIS:
        case R_Z80_16:
        case R_Z80_24:
        case R_Z80_32:
            return 0;
    }

    return -1;
}

/* There is no dynamic linking on this target: a MOS binary is a flat image
 * loaded at a fixed address, with nothing to resolve at load time. Every
 * relocation is therefore a plain one. */
ST_FUNC int gotplt_entry_type (int reloc_type)
{
    switch (reloc_type) {
        case R_Z80_NONE:
        case R_Z80_8:
        case R_Z80_8_DIS:
        case R_Z80_8_PCREL:
        case R_Z80_16:
        case R_Z80_24:
        case R_Z80_32:
            return NO_GOTPLT_ENTRY;
    }

    return -1;
}

ST_FUNC unsigned create_plt_entry(TCCState *s1, unsigned got_offset, struct sym_attr *attr)
{
    tcc_error("ez80: no procedure linkage table on this target");

    return 0;
}

ST_FUNC void relocate_plt(TCCState *s1)
{
}

ST_FUNC void relocate(TCCState *s1, ElfW_Rel *rel, int type, unsigned char *ptr, addr_t addr, addr_t val)
{
    switch (type) {
        case R_Z80_NONE:
            return;

        case R_Z80_8:
            ptr[0] = (unsigned char) val;
            return;

        case R_Z80_8_DIS:
            /* The signed displacement in (ix+d). Out of range is a real
             * error and not something to truncate silently: a frame deeper
             * than 128 bytes is the usual cause, and the guide has a whole
             * section on why that matters here. */
            if ((int) val < -128 || (int) val > 127)
                tcc_error("ez80: displacement %d out of range for (ix+d)", (int) val);
            ptr[0] = (unsigned char) val;
            return;

        case R_Z80_8_PCREL:
            /* jr/djnz, relative to the byte after the displacement. */
            {
                int d = (int) (val - addr - 1);
                if (d < -128 || d > 127)
                    tcc_error("ez80: relative jump out of range (%d bytes)", d);
                ptr[0] = (unsigned char) d;
            }
            return;

        case R_Z80_16:
            write16le(ptr, val);
            return;

        case R_Z80_24:
            /* ADL-mode addresses. No write24le in tccelf.c, and the eZ80 has
             * no alignment requirement, so it is written a byte at a time. */
            ptr[0] = (unsigned char) val;
            ptr[1] = (unsigned char) (val >> 8);
            ptr[2] = (unsigned char) (val >> 16);
            return;

        case R_Z80_32:
            write32le(ptr, val);
            return;
    }

    tcc_error("ez80: unknown relocation type %d", type);
}

#endif /* !TARGET_DEFS_ONLY */
