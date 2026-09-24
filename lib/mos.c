/*
 * MOS, as libagon names it.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <agon/mos.h>

/* In acc's own runtime, which is where the instruction that calls MOS lives:
 * acc has no way to write one in C. Four ways out, because MOS answers in
 * whichever register suits what was asked. The registers are the ones each
 * call wants, and which is which is the same here as in libagon, whose
 * routines these were read off. */
int acc_rt_mos(int fn, int hl, int de, int bc);
int acc_rt_mos_de(int fn, int hl, int de, int bc);
int acc_rt_mos_hl(int fn, int hl, int de, int bc);
int acc_rt_mos_ix(int fn, int hl, int de, int bc);
int acc_rt_puts(int hl, int bc, int a);
int acc_rt_putch(int c);

/* Which call is which, as MOS numbers them. */
#define MOS_GETKEY   0x00
#define MOS_DEL      0x05
#define MOS_EDITLINE 0x09
#define MOS_REN      0x06
#define MOS_MKDIR    0x07
#define MOS_SYSVARS  0x08
#define MOS_FOPEN    0x0a
#define MOS_FCLOSE   0x0b
#define MOS_GETFIL   0x19
#define MOS_FREAD    0x1a
#define MOS_FWRITE   0x1b
#define MOS_FLSEEK   0x1c
#define FFS_DOPEN    0x91
#define FFS_DCLOSE   0x92
#define FFS_DREAD    0x93
#define FFS_STAT     0x96

int putch(int c)
{
    return acc_rt_putch(c);
}

char getch(void)
{
    return (char) acc_rt_mos(MOS_GETKEY, 0, 0, 0);
}

void mos_puts(const char *buffer, uint24_t size, char delimiter)
{
    acc_rt_puts((int) buffer, (int) size, delimiter);
}

/* A line from the keyboard, with MOS's editing, into `buffer`; what comes
 * back is the key that ended it -- 13 for return, 27 for escape. */
uint8_t mos_editline(char *buffer, uint24_t size, uint8_t clear)
{
    return (uint8_t) acc_rt_mos(MOS_EDITLINE, (int) buffer, clear, (int) size);
}

uint8_t mos_del(const char *filename)
{
    return (uint8_t) acc_rt_mos(MOS_DEL, (int) filename, 0, 0);
}

uint8_t mos_ren(const char *filename, const char *newname)
{
    return (uint8_t) acc_rt_mos(MOS_REN, (int) filename, (int) newname, 0);
}

uint8_t mos_mkdir(const char *path)
{
    return (uint8_t) acc_rt_mos(MOS_MKDIR, (int) path, 0, 0);
}

/* The mode goes in C, which is the low byte of BC. */
uint8_t mos_fopen(const char *filename, uint8_t mode)
{
    return (uint8_t) acc_rt_mos(MOS_FOPEN, (int) filename, 0, mode);
}

uint8_t mos_fclose(uint8_t fh)
{
    return (uint8_t) acc_rt_mos(MOS_FCLOSE, 0, 0, fh);
}

uint24_t mos_fread(uint8_t fh, char *buffer, uint24_t numbytes)
{
    return (uint24_t) acc_rt_mos_de(MOS_FREAD, (int) buffer, (int) numbytes, fh);
}

uint24_t mos_fwrite(uint8_t fh, char *buffer, uint24_t numbytes)
{
    return (uint24_t) acc_rt_mos_de(MOS_FWRITE, (int) buffer, (int) numbytes, fh);
}

/* A four-byte offset in three-byte registers: the low three in HL and the
 * top one in E, which is what MOS reads. */
uint8_t mos_flseek(uint8_t fh, uint32_t offset)
{
    return (uint8_t) acc_rt_mos(MOS_FLSEEK, (int) (offset & 0xffffffUL),
                                (int) ((offset >> 24) & 0xffUL), fh);
}

FIL *mos_getfil(uint8_t fh)
{
    return (FIL *) acc_rt_mos_hl(MOS_GETFIL, 0, 0, fh);
}

uint8_t ffs_dopen(DIR *dir_handle, const char *dir_path)
{
    return (uint8_t) acc_rt_mos(FFS_DOPEN, (int) dir_handle, (int) dir_path, 0);
}

uint8_t ffs_dclose(DIR *dir_handle)
{
    return (uint8_t) acc_rt_mos(FFS_DCLOSE, (int) dir_handle, 0, 0);
}

uint8_t ffs_dread(DIR *dir_handle, FILINFO *fil_handle)
{
    return (uint8_t) acc_rt_mos(FFS_DREAD, (int) dir_handle,
                                (int) fil_handle, 0);
}

uint8_t ffs_stat(FILINFO *fil_handle, const char *filename)
{
    return (uint8_t) acc_rt_mos(FFS_STAT, (int) fil_handle, (int) filename, 0);
}

/* ------------------------------------------------------------------ */
/* the system variables                                                */

/* Asked of MOS once and kept, because every getter below wants it and the
 * answer does not change while a program runs. */
static uint8_t *sysvars;

uint8_t *mos_sysvars(void)
{
    if (!sysvars)
        sysvars = (uint8_t *) acc_rt_mos_ix(MOS_SYSVARS, 0, 0, 0);

    return sysvars;
}

/* Where each of them is, as MOS lays them out. */
#define SYSVAR_KEYASCII   0x05
#define SYSVAR_KEYMODS    0x06
#define SYSVAR_CURSORX    0x07
#define SYSVAR_CURSORY    0x08
#define SYSVAR_SCRWIDTH   0x0f
#define SYSVAR_SCRHEIGHT  0x11
#define SYSVAR_SCRCOLS    0x13
#define SYSVAR_SCRROWS    0x14
#define SYSVAR_SCRCOLOURS 0x15

uint8_t getsysvar_keyascii(void)
{
    return mos_sysvars()[SYSVAR_KEYASCII];
}

uint8_t getsysvar_keymods(void)
{
    return mos_sysvars()[SYSVAR_KEYMODS];
}

uint8_t getsysvar_cursorX(void)
{
    return mos_sysvars()[SYSVAR_CURSORX];
}

uint8_t getsysvar_cursorY(void)
{
    return mos_sysvars()[SYSVAR_CURSORY];
}

/* Two bytes, lowest first. */
uint16_t getsysvar_scrwidth(void)
{
    uint8_t *v = mos_sysvars() + SYSVAR_SCRWIDTH;

    return (uint16_t) (v[0] | (v[1] << 8));
}

uint16_t getsysvar_scrheight(void)
{
    uint8_t *v = mos_sysvars() + SYSVAR_SCRHEIGHT;

    return (uint16_t) (v[0] | (v[1] << 8));
}

uint8_t getsysvar_scrCols(void)
{
    return mos_sysvars()[SYSVAR_SCRCOLS];
}

uint8_t getsysvar_scrRows(void)
{
    return mos_sysvars()[SYSVAR_SCRROWS];
}

uint8_t getsysvar_scrColours(void)
{
    return mos_sysvars()[SYSVAR_SCRCOLOURS];
}

/* The clock MOS keeps ticks twice a frame, so waiting for it to change is
 * waiting for the frame to end. */
void waitvblank(void)
{
    volatile uint8_t *clock = mos_sysvars();
    uint8_t was = *clock;

    while (*clock == was)
        ;
}
