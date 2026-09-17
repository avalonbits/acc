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
/* A single byte or word taken out of a wider value, which is how a 24-bit
   address gets loaded a byte at a time. libagon.a uses r_byte0 seven times,
   all of them in its startup code. */
#define R_Z80_BYTE0     7
#define R_Z80_BYTE1     8
#define R_Z80_BYTE2     9
#define R_Z80_BYTE3    10
#define R_Z80_WORD0    11
#define R_Z80_WORD1    12
#define R_Z80_16_BE    13
#define R_Z80_NUM      14

/* A pointer is three bytes here, so the relocation that writes one is the
 * 24-bit form and not the 32-bit one. */
#define R_DATA_32   R_Z80_32
#define R_DATA_PTR  R_Z80_24
#define R_JMP_SLOT  R_Z80_24
#define R_GLOB_DAT  R_Z80_24
#define R_COPY      R_Z80_NONE
#define R_RELATIVE  R_Z80_24

#define R_NUM       R_Z80_NUM

/* The Agon's memory map: 448 KB of user RAM at 0x40000. That is agondev's own
   RAM_START and RAM_SIZE default, and the whole of the external SRAM the
   machine maps for programs. MOS loads at the bottom of it and there are no
   pages to align to. */
#define EZ80_RAM_START  0x040000
#define EZ80_RAM_SIZE   0x070000
#define EZ80_RAM_END    (EZ80_RAM_START + EZ80_RAM_SIZE)


#define ELF_START_ADDR EZ80_RAM_START
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
        case R_Z80_BYTE0:
        case R_Z80_BYTE1:
        case R_Z80_BYTE2:
        case R_Z80_BYTE3:
        case R_Z80_WORD0:
        case R_Z80_WORD1:
        case R_Z80_16_BE:
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
        case R_Z80_BYTE0:
        case R_Z80_BYTE1:
        case R_Z80_BYTE2:
        case R_Z80_BYTE3:
        case R_Z80_WORD0:
        case R_Z80_WORD1:
        case R_Z80_16_BE:
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

/* ------------------------------------------------------------------ */
/* the symbols agondev's linker script provides                        */

/* This file defines USING_GLOBALS so that tcc_error needs no local state, and
 * that turns the section names into macros reading tcc_state -- which is null
 * while linking, because it is only set while a translation unit is being
 * compiled. Worse, `s1->symtab_section` then expands to
 * `s1->tcc_state->symtab_section`. The fields are reached directly below. */
#undef symtab_section
#undef bss_section
#undef text_section
#undef data_section

/* crt0.o in libagon.a references a dozen symbols that agondev's linker.conf
 * defines rather than any object: where the stack goes, where bss is and how
 * long it is, where the heap starts and ends, and how many initialisers to
 * run. acc has no linker script, so it defines them itself, from the same
 * expressions.
 *
 * The memory map is the Agon's: 448 KB of user RAM at 0x40000, which is
 * agondev's own RAM_START and RAM_SIZE default and the whole of the external
 * SRAM the machine maps for programs. */
#define EZ80_RAM_START  0x040000
#define EZ80_RAM_SIZE   0x070000
#define EZ80_RAM_END    (EZ80_RAM_START + EZ80_RAM_SIZE)

/* s1-> throughout rather than the section macros. This file defines
 * USING_GLOBALS so that tcc_error needs no local state, and that makes
 * `symtab_section` mean `tcc_state->symtab_section` -- which is null during
 * linking, because tcc_state is only set while a translation unit is being
 * compiled. */
static void abs_sym(TCCState *s1, const char *name, addr_t value)
{
    set_elf_sym(s1->symtab_section, value, 0,
                ELFW(ST_INFO)(STB_GLOBAL, STT_NOTYPE), 0, SHN_ABS, name);
}

static void sec_sym(TCCState *s1, const char *name, Section *sec, addr_t off)
{
    set_elf_sym(s1->symtab_section, off, 0,
                ELFW(ST_INFO)(STB_GLOBAL, STT_NOTYPE), 0, sec->sh_num, name);
}

/* The size of a section acc may never have created. */
static addr_t sec_size(TCCState *s1, const char *name)
{
    Section *s = have_section(s1, name);

    return s ? s->data_offset : 0;
}

ST_FUNC void ez80_add_linker_symbols(TCCState *s1)
{
    addr_t bss_len = s1->bss_section->data_offset;
    addr_t init_array = sec_size(s1, ".init_array");
    addr_t ctors = sec_size(s1, ".ctors");
    addr_t dtors = sec_size(s1, ".dtors");
    addr_t fini_array = sec_size(s1, ".fini_array");
    /* Each entry is a 3-byte function pointer. */
    addr_t init_count = (init_array + ctors) / PTR_SIZE;
    addr_t fini_count = (dtors + fini_array) / PTR_SIZE;

    /* Two of agondev's build options, which its makefile passes as -defsym
     * and crt0 tests. acc takes the simple side of both: no exit handler, and
     * the plain command-line splitter rather than the one that understands
     * quoting and redirection. */
    abs_sym(s1, "_has_exit_handler", 0);

    /* Both at the top of RAM, which is what agondev's linker script does: the
     * heap grows up from the end of bss and the stack grows down from here,
     * out of one region with nothing between them.
     *
     * That means sbrk only refuses once the heap reaches where the stack
     * *starts*, by which point the heap has been overwriting the stack for a
     * long time -- running out of memory shows up as the program going
     * haywire rather than as an error. Fencing the heap below the stack would
     * turn it into a clean "memory full", at the cost of no longer producing
     * the same image as agondev's ld. See docs/porting-notes.md. */
    abs_sym(s1, "__stack", EZ80_RAM_END);
    abs_sym(s1, "___heaptop", EZ80_RAM_END);

    sec_sym(s1, "___low_bss", s1->bss_section, 0);
    abs_sym(s1, "___len_bss", bss_len);
    abs_sym(s1, "___run_clearbss", bss_len > 0);
    sec_sym(s1, "___heapbot", s1->bss_section, bss_len);

    /* crt0 counts initialisers down in three byte-sized registers, and walks
     * the arrays backwards from their ends. The three counts are the linker
     * script's expressions verbatim; when nothing is to be run the guard is
     * zero and their values are never looked at. */
    abs_sym(s1, "___run_init", init_count > 0);
    abs_sym(s1, "___run_fini", fini_count > 0);
    abs_sym(s1, "___init_count_a", (((init_count - 1) >> 16) & 0xff) + 1);
    abs_sym(s1, "___init_count_b", (((init_count - 1)      ) & 0xff) + 1);
    abs_sym(s1, "___init_count_c", (((init_count - 1) >>  8) & 0xff) + 1);
    abs_sym(s1, "___fini_count_a", (((fini_count - 1) >> 16) & 0xff) + 1);
    abs_sym(s1, "___fini_count_b", (((fini_count - 1)      ) & 0xff) + 1);
    abs_sym(s1, "___fini_count_c", (((fini_count - 1) >>  8) & 0xff) + 1);

    /* The end of each array, because crt0 walks them downwards. */
    {
        static const struct { const char *sym, *sec; } ends[] = {
            { "___init_array_functions", ".init_array" },
            { "___init_ctors_functions", ".ctors" },
            { "___fini_dtors_functions", ".dtors" },
            { "___fini_array_functions", ".fini_array" },
        };
        unsigned i;

        for (i = 0; i < sizeof ends / sizeof *ends; i++) {
            Section *sec = have_section(s1, ends[i].sec);

            if (sec)
                sec_sym(s1, ends[i].sym, sec, sec->data_offset);
            else
                abs_sym(s1, ends[i].sym, 0);
        }
    }
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

        /* One byte or one word lifted out of the value, for code that builds
         * an address a piece at a time. */
        case R_Z80_BYTE0: ptr[0] = (unsigned char) val;         return;
        case R_Z80_BYTE1: ptr[0] = (unsigned char)(val >> 8);   return;
        case R_Z80_BYTE2: ptr[0] = (unsigned char)(val >> 16);  return;
        case R_Z80_BYTE3: ptr[0] = (unsigned char)(val >> 24);  return;
        case R_Z80_WORD0: write16le(ptr, val);                  return;
        case R_Z80_WORD1: write16le(ptr, val >> 16);            return;

        case R_Z80_16_BE:
            ptr[0] = (unsigned char)(val >> 8);
            ptr[1] = (unsigned char) val;
            return;
    }

    tcc_error("ez80: unknown relocation type %d", type);
}

#endif /* !TARGET_DEFS_ONLY */
