/*
 * MOS, as libagon names it, and the rest of MOS 3.0.2's API.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Every call here is one MOS call, with the registers MOS's own source says
 * that call takes -- src/mos_api.asm in agon-mos, which is where each
 * register below was read off. Most go through acc_rt_mos_call, which sets
 * every register from a structure and stores every register back into it;
 * the few that libagon has always had, and that the file layer uses, keep
 * the older and smaller routines that set only HL, DE and BC.
 */

#include <stdarg.h>
#include <stdio.h>

#include <agon/mos.h>

/* acc's runtime: see src/rt/helpers.s. */
int acc_rt_mos(int fn, int hl, int de, int bc);
int acc_rt_mos_de(int fn, int hl, int de, int bc);
int acc_rt_mos_hl(int fn, int hl, int de, int bc);
int acc_rt_mos_ix(int fn, int hl, int de, int bc);
int acc_rt_puts(int hl, int bc, int a);
int acc_rt_putch(int c);
int acc_rt_port_in(int port);

/* The registers of one call, going in and coming out. */
typedef struct {
    uint24_t hl, de, bc, ix, iy;
    uint8_t  a, carry;
} Regs;

int acc_rt_mos_call(Regs *r);

/* A call with all its registers zero but the ones the caller sets. */
#define REGS(r) Regs r = { 0, 0, 0, 0, 0, 0, 0 }

static uint8_t call(Regs *r, int fn)
{
    r->a = (uint8_t) fn;

    return (uint8_t) acc_rt_mos_call(r);
}

/* A register's low byte, and a register made of two bytes, as MOS reads
 * B and C out of BC. */
#define LOW(v)      ((uint8_t) ((v) & 0xff))
#define BC(b, c)    ((uint24_t) (((b) & 0xff) << 8 | ((c) & 0xff)))

/* Which call is which, as MOS numbers them. */
enum {
    MOS_GETKEY = 0x00, MOS_LOAD, MOS_SAVE, MOS_CD, MOS_DIR, MOS_DEL, MOS_REN,
    MOS_MKDIR, MOS_SYSVARS, MOS_EDITLINE, MOS_FOPEN, MOS_FCLOSE, MOS_FGETC,
    MOS_FPUTC, MOS_FEOF, MOS_GETERROR, MOS_OSCLI, MOS_COPY, MOS_GETRTC,
    MOS_SETRTC, MOS_SETINTVECTOR, MOS_UOPEN, MOS_UCLOSE, MOS_UGETC,
    MOS_UPUTC, MOS_GETFIL, MOS_FREAD, MOS_FWRITE, MOS_FLSEEK,
    MOS_SETKBVECTOR, MOS_GETKBMAP, MOS_I2C_OPEN, MOS_I2C_CLOSE,
    MOS_I2C_WRITE, MOS_I2C_READ, MOS_UNPACKRTC, MOS_FLSEEK_P,
    MOS_PMATCH = 0x28, MOS_GETARGUMENT, MOS_EXTRACTSTRING,
    MOS_EXTRACTNUMBER, MOS_ESCAPESTRING,
    MOS_SETVARVAL = 0x30, MOS_READVARVAL, MOS_GSINIT, MOS_GSREAD,
    MOS_GSTRANS, MOS_SUBSTITUTEARGS,
    MOS_RESOLVEPATH = 0x38, MOS_GETDIRFORPATH, MOS_GETLEAFNAME,
    MOS_ISDIRECTORY, MOS_GETABSOLUTEPATH,
    MOS_CLEARVDPFLAGS = 0x40, MOS_WAITFORVDPFLAGS,
    MOS_GETFUNCTION = 0x50,
    SD_GETUNLOCKCODE = 0x70, SD_INIT, SD_READBLOCKS, SD_WRITEBLOCKS,
    FFS_FOPEN = 0x80, FFS_FCLOSE, FFS_FREAD, FFS_FWRITE, FFS_FLSEEK,
    FFS_FTRUNCATE, FFS_FSYNC, FFS_FFORWARD, FFS_FEXPAND, FFS_FGETS,
    FFS_FPUTC, FFS_FPUTS, FFS_FPRINTF, FFS_FTELL, FFS_FEOF, FFS_FSIZE,
    FFS_FERROR, FFS_DOPEN, FFS_DCLOSE, FFS_DREAD, FFS_DFINDFIRST,
    FFS_DFINDNEXT, FFS_STAT, FFS_UNLINK, FFS_RENAME, FFS_CHMOD, FFS_UTIME,
    FFS_MKDIR, FFS_CHDIR, FFS_CHDRIVE, FFS_GETCWD, FFS_MOUNT, FFS_MKFS,
    FFS_FDISK, FFS_GETFREE, FFS_GETLABEL, FFS_SETLABEL, FFS_SETCP,
    FFS_FLSEEK_P
};

/* ------------------------------------------------------------------ */
/* the console                                                         */

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

/* A C string: RST 18h's own way of stopping at a byte, with no count. */
void mos_putstring(const char *string)
{
    acc_rt_puts((int) string, 0, 0);
}

/* A line from the keyboard, with MOS's editing, into `buffer`; what comes
 * back is the key that ended it -- 13 for return, 27 for escape. */
uint8_t mos_editline(char *buffer, uint24_t bufferlength, uint8_t clearbuffer)
{
    return (uint8_t) acc_rt_mos(MOS_EDITLINE, (int) buffer, clearbuffer,
                                (int) bufferlength);
}

/* ------------------------------------------------------------------ */
/* files and directories by name                                       */

uint8_t mos_load(const char *filename, char *address, uint24_t maxsize)
{
    return (uint8_t) acc_rt_mos(MOS_LOAD, (int) filename, (int) address,
                                (int) maxsize);
}

uint8_t mos_save(const char *filename, char *address, uint24_t nbytes)
{
    return (uint8_t) acc_rt_mos(MOS_SAVE, (int) filename, (int) address,
                                (int) nbytes);
}

uint8_t mos_cd(const char *path)
{
    return (uint8_t) acc_rt_mos(MOS_CD, (int) path, 0, 0);
}

uint8_t mos_dir(const char *path)
{
    return (uint8_t) acc_rt_mos(MOS_DIR, (int) path, 0, 0);
}

uint8_t mos_del(const char *filename)
{
    return (uint8_t) acc_rt_mos(MOS_DEL, (int) filename, 0, 0);
}

uint8_t mos_ren(const char *filename, const char *newname)
{
    return (uint8_t) acc_rt_mos(MOS_REN, (int) filename, (int) newname, 0);
}

uint8_t mos_copy(const char *source, const char *destination)
{
    return (uint8_t) acc_rt_mos(MOS_COPY, (int) source, (int) destination, 0);
}

uint8_t mos_mkdir(const char *path)
{
    return (uint8_t) acc_rt_mos(MOS_MKDIR, (int) path, 0, 0);
}

/* ------------------------------------------------------------------ */
/* files by handle                                                     */

/* The mode goes in C, which is the low byte of BC. */
uint8_t mos_fopen(const char *filename, uint8_t mode)
{
    return (uint8_t) acc_rt_mos(MOS_FOPEN, (int) filename, 0, mode);
}

uint8_t mos_fclose(uint8_t fh)
{
    return (uint8_t) acc_rt_mos(MOS_FCLOSE, 0, 0, fh);
}

char mos_fgetc(uint8_t fh)
{
    return (char) acc_rt_mos(MOS_FGETC, 0, 0, fh);
}

/* The handle in C and the character in B. */
void mos_fputc(uint8_t fh, char c)
{
    acc_rt_mos(MOS_FPUTC, 0, 0, (int) BC(c, fh));
}

uint8_t mos_feof(uint8_t fh)
{
    return (uint8_t) acc_rt_mos(MOS_FEOF, 0, 0, fh);
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

/* The same, with the offset where HL points. */
uint8_t mos_flseek_p(uint8_t fh, uint32_t offset)
{
    return (uint8_t) acc_rt_mos(MOS_FLSEEK_P, (int) &offset, 0, fh);
}

FIL *mos_getfil(uint8_t fh)
{
    return (FIL *) acc_rt_mos_hl(MOS_GETFIL, 0, 0, fh);
}

/* ------------------------------------------------------------------ */
/* the rest of the numbered calls                                      */

/* The code in E, the buffer in HL and its size in BC. */
void mos_getError(uint8_t code, char *buffer, uint24_t bufferlength)
{
    acc_rt_mos(MOS_GETERROR, (int) buffer, code, (int) bufferlength);
}

uint8_t mos_oscli(char *command, char **argv, uint24_t argc)
{
    return (uint8_t) acc_rt_mos(MOS_OSCLI, (int) command, (int) argv, (int) argc);
}

uint8_t mos_getrtc(char *buffer)
{
    return (uint8_t) acc_rt_mos(MOS_GETRTC, (int) buffer, 0, 0);
}

void mos_setrtc(uint8_t *timedata)
{
    acc_rt_mos(MOS_SETRTC, (int) timedata, 0, 0);
}

void mos_unpackrtc(vdp_time_t *buffer, uint8_t flags)
{
    acc_rt_mos(MOS_UNPACKRTC, (int) buffer, 0, flags);
}

/* The vector's number in E, the handler in HL, and the old handler back in
 * HL. The handler is an interrupt routine, called as MOS calls one. */
void *mos_setintvector(uint8_t vector, void (*handler)(void))
{
    return (void *) acc_rt_mos_hl(MOS_SETINTVECTOR, (int) handler, vector, 0);
}

/* Called with the address of each keyboard packet in DE, from MOS's
 * interrupt; C non-zero says the handler is a 16-bit address. */
void mos_setkbvector(void (*handler)(void), uint8_t addresslength)
{
    acc_rt_mos(MOS_SETKBVECTOR, (int) handler, 0, addresslength);
}

uint8_t *mos_getkbmap(void)
{
    return (uint8_t *) acc_rt_mos_ix(MOS_GETKBMAP, 0, 0, 0);
}

/* UART1: the settings pointed at by IX. */
uint8_t mos_uopen(UART *settings)
{
    REGS(r);

    r.ix = (uint24_t) settings;

    return call(&r, MOS_UOPEN);
}

void mos_uclose(void)
{
    acc_rt_mos(MOS_UCLOSE, 0, 0, 0);
}

/* A character, with the carry saying there was one: 256 when the port is
 * not open, which is past every character, as libagon has it. */
int mos_ugetc(void)
{
    REGS(r);

    call(&r, MOS_UGETC);

    return r.carry ? r.a : 256;
}

/* Without waiting: the UART's own registers, read directly. A byte is
 * waiting when bit 0 of the line status register is set, and -1 says there
 * is none, which is what libagon's answers. */
#define UART1_RBR   0xd0
#define UART1_LSR   0xd5

int mos_ugetc_nb(void)
{
    if (!(acc_rt_port_in(UART1_LSR) & 1))
        return -1;

    return acc_rt_port_in(UART1_RBR) & 0xff;
}

uint8_t mos_uputc(int c)
{
    REGS(r);

    r.bc = LOW(c);
    call(&r, MOS_UPUTC);

    return r.carry;
}

/* I2C: the frequency, or the address, in C; a count in B; the bytes where
 * HL points. */
void mos_i2c_open(uint8_t frequency)
{
    acc_rt_mos(MOS_I2C_OPEN, 0, 0, frequency);
}

void mos_i2c_close(void)
{
    acc_rt_mos(MOS_I2C_CLOSE, 0, 0, 0);
}

uint8_t mos_i2c_write(uint8_t i2c_address, uint8_t size, unsigned char *buffer)
{
    return (uint8_t) acc_rt_mos(MOS_I2C_WRITE, (int) buffer, 0,
                                (int) BC(size, i2c_address));
}

uint8_t mos_i2c_read(uint8_t i2c_address, uint8_t size, unsigned char *buffer)
{
    return (uint8_t) acc_rt_mos(MOS_I2C_READ, (int) buffer, 0,
                                (int) BC(size, i2c_address));
}

/* ------------------------------------------------------------------ */
/* strings                                                             */

int8_t mos_pmatch(const char *pattern, const char *string, uint8_t flags)
{
    return (int8_t) acc_rt_mos(MOS_PMATCH, (int) pattern, (int) string, flags);
}

/* The argument's start comes back in HL and its end in DE. */
void mos_getargument(char **arg, char **argend, const char *source,
                     uint24_t argnumber)
{
    REGS(r);

    r.hl = (uint24_t) source;
    r.bc = argnumber;
    call(&r, MOS_GETARGUMENT);
    if (arg)
        *arg = (char *) r.hl;
    if (argend)
        *argend = (char *) r.de;
}

uint8_t mos_extractstring(char **result, char **next, const char *source,
                          const char *dividers, uint8_t flags)
{
    REGS(r);
    uint8_t status;

    r.hl = (uint24_t) source;
    r.de = (uint24_t) dividers;
    r.bc = flags;
    status = call(&r, MOS_EXTRACTSTRING);
    if (result)
        *result = (char *) r.hl;
    if (next)
        *next = (char *) r.de;

    return status;
}

uint8_t mos_extractnumber(uint24_t *result, char **next, const char *source,
                          const char *dividers, uint8_t flags)
{
    REGS(r);
    uint8_t status;

    r.hl = (uint24_t) source;
    r.de = (uint24_t) dividers;
    r.bc = flags;
    status = call(&r, MOS_EXTRACTNUMBER);
    if (result)
        *result = r.hl;
    if (next)
        *next = (char *) r.de;

    return status;
}

uint8_t mos_escapestring(uint24_t *resultlength, const char *source,
                         char *buffer, uint24_t bufferlength)
{
    REGS(r);
    uint8_t status;

    r.hl = (uint24_t) source;
    r.de = (uint24_t) buffer;
    r.bc = bufferlength;
    status = call(&r, MOS_ESCAPESTRING);
    if (resultlength)
        *resultlength = r.bc;

    return status;
}

/* ------------------------------------------------------------------ */
/* MOS's variables, and GSTrans                                        */

/* The name to set in HL, the value in IX, the name found last time in IY
 * (0 the first time), the type in C; the type MOS used and the name it set
 * come back in C and IY. */
int mos_setvarval(char *name, void *value, char **actualName, uint8_t *type)
{
    REGS(r);
    uint8_t status;

    r.hl = (uint24_t) name;
    r.ix = (uint24_t) value;
    r.iy = actualName ? (uint24_t) *actualName : 0;
    r.bc = type ? *type : 0;
    status = call(&r, MOS_SETVARVAL);
    if (actualName)
        *actualName = (char *) r.iy;
    if (type)
        *type = LOW(r.bc);

    return status;
}

int mos_readvarval(char *namePattern, void *value, char **actualName,
                   int *length, uint8_t *typeFlag)
{
    REGS(r);
    uint8_t status;

    r.hl = (uint24_t) namePattern;
    r.ix = (uint24_t) value;
    r.de = length ? (uint24_t) *length : 0;
    r.iy = actualName ? (uint24_t) *actualName : 0;
    r.bc = typeFlag ? *typeFlag : 0;
    status = call(&r, MOS_READVARVAL);
    if (actualName)
        *actualName = (char *) r.iy;
    if (length)
        *length = (int) r.de;
    if (typeFlag)
        *typeFlag = LOW(r.bc);

    return status;
}

uint8_t mos_gsinit(const char *source, char **transinfo_ptr, uint8_t flags)
{
    return (uint8_t) acc_rt_mos(MOS_GSINIT, (int) source, (int) transinfo_ptr,
                                flags);
}

/* The character read comes back in C. */
uint8_t mos_gsread(char *char_read, char **transinfo_ptr)
{
    REGS(r);
    uint8_t status;

    r.de = (uint24_t) transinfo_ptr;
    status = call(&r, MOS_GSREAD);
    if (char_read)
        *char_read = (char) LOW(r.bc);

    return status;
}

int mos_gstrans(char *source, char *dest, int destLen, int *read, uint8_t flags)
{
    REGS(r);
    uint8_t status;

    r.hl = (uint24_t) source;
    r.ix = (uint24_t) dest;
    r.de = (uint24_t) destLen;
    r.bc = flags;
    status = call(&r, MOS_GSTRANS);
    if (read)
        *read = (int) r.bc;

    return status;
}

/* What MOS answers here is the length, in BC, rather than a status. */
int mos_substituteargs(char *tpl, char *args, char *dest, int length,
                       uint8_t flags)
{
    REGS(r);

    r.hl = (uint24_t) tpl;
    r.ix = (uint24_t) args;
    r.de = (uint24_t) length;
    r.iy = (uint24_t) dest;
    r.bc = flags;
    call(&r, MOS_SUBSTITUTEARGS);

    return (int) r.bc;
}

/* ------------------------------------------------------------------ */
/* paths                                                               */

int mos_resolvepath(char *filepath, char *resolvedPath, int *length,
                    uint8_t *index, DIR *dir, uint8_t flags)
{
    REGS(r);
    uint8_t status;

    r.hl = (uint24_t) filepath;
    r.ix = (uint24_t) resolvedPath;
    r.de = length ? (uint24_t) *length : 0;
    r.iy = (uint24_t) dir;
    r.bc = BC(flags, index ? *index : 0);
    status = call(&r, MOS_RESOLVEPATH);
    if (index)
        *index = LOW(r.bc);
    if (length)
        *length = (int) r.de;

    return status;
}

int mos_getdirforpath(char *srcPath, char *dir, int *length, uint8_t index)
{
    REGS(r);
    uint8_t status;

    r.hl = (uint24_t) srcPath;
    r.ix = (uint24_t) dir;
    r.de = length ? (uint24_t) *length : 0;
    r.bc = index;
    status = call(&r, MOS_GETDIRFORPATH);
    if (length)
        *length = (int) r.de;

    return status;
}

char *mos_getleafname(const char *pathname)
{
    return (char *) acc_rt_mos_hl(MOS_GETLEAFNAME, (int) pathname, 0, 0);
}

uint8_t mos_isdirectory(const char *pathname)
{
    return (uint8_t) acc_rt_mos(MOS_ISDIRECTORY, (int) pathname, 0, 0);
}

int mos_getabsolutepath(char *path, char *resolved, int *length)
{
    REGS(r);

    r.hl = (uint24_t) path;
    r.ix = (uint24_t) resolved;
    r.de = length ? (uint24_t) *length : 0;

    return call(&r, MOS_GETABSOLUTEPATH);
}

/* ------------------------------------------------------------------ */
/* the VDP's answers, MOS's functions, the card                        */

uint8_t mos_clearvdpflags(uint8_t bitmask)
{
    return (uint8_t) acc_rt_mos(MOS_CLEARVDPFLAGS, 0, 0, bitmask);
}

uint8_t mos_waitforvdpflags(uint8_t bitmask)
{
    return (uint8_t) acc_rt_mos(MOS_WAITFORVDPFLAGS, 0, 0, bitmask);
}

/* The number in B and the flags in C. */
void *mos_getfunction(uint8_t number, uint8_t flags)
{
    return (void *) acc_rt_mos_hl(MOS_GETFUNCTION, 0, 0, (int) BC(number, flags));
}

/* MOS writes the code where HL points. */
uint24_t sd_getunlockcode(void)
{
    uint24_t code = 0;

    acc_rt_mos(SD_GETUNLOCKCODE, (int) &code, 0, 0);

    return code;
}

uint8_t sd_init(uint24_t unlockcode)
{
    return (uint8_t) acc_rt_mos(SD_INIT, (int) &unlockcode, 0, 0);
}

/* The block number and, four bytes on, the unlock code, both where HL
 * points; the buffer in DE and the count in BC. The code is asked for here,
 * as libagon asks for it.
 *
 * That is the layout MOS documents. The 3.0.2 release does not check it:
 * its SD_readBlocks_API compares the code with the first three bytes of the
 * block number -- `addr + sizeof(DWORD)` on a void pointer, which its
 * compiler took as addr -- so a block can only be read whose number's low
 * 24 bits are the code; and its sd_api_writeblocks calls the read. Both
 * are MOS's to fix, and this does what MOS says rather than what it does. */
typedef struct {
    uint32_t sector;
    uint24_t unlock;
} Block;

uint8_t sd_readblocks(uint32_t sector, uint8_t *buf, uint24_t count)
{
    Block b;

    b.sector = sector;
    b.unlock = sd_getunlockcode();

    return (uint8_t) acc_rt_mos(SD_READBLOCKS, (int) &b, (int) buf, (int) count);
}

uint8_t sd_writeblocks(uint32_t sector, uint8_t *buf, uint24_t count)
{
    Block b;

    b.sector = sector;
    b.unlock = sd_getunlockcode();

    return (uint8_t) acc_rt_mos(SD_WRITEBLOCKS, (int) &b, (int) buf, (int) count);
}

/* ------------------------------------------------------------------ */
/* MOS's FatFS                                                         */

uint8_t ffs_fopen(FIL *fh, const char *filename, uint8_t mode)
{
    return (uint8_t) acc_rt_mos(FFS_FOPEN, (int) fh, (int) filename, mode);
}

uint8_t ffs_fclose(FIL *fh)
{
    return (uint8_t) acc_rt_mos(FFS_FCLOSE, (int) fh, 0, 0);
}

/* The count comes back in BC. */
uint24_t ffs_fread(FIL *fh, char *buffer, uint24_t numbytes)
{
    REGS(r);

    r.hl = (uint24_t) fh;
    r.de = (uint24_t) buffer;
    r.bc = numbytes;
    call(&r, FFS_FREAD);

    return r.bc;
}

uint24_t ffs_fwrite(FIL *fh, const char *buffer, uint24_t numbytes)
{
    REGS(r);

    r.hl = (uint24_t) fh;
    r.de = (uint24_t) buffer;
    r.bc = numbytes;
    call(&r, FFS_FWRITE);

    return r.bc;
}

/* The low three bytes of the offset in DE and the top one in C. */
uint8_t ffs_flseek(FIL *fh, uint32_t offset)
{
    return (uint8_t) acc_rt_mos(FFS_FLSEEK, (int) fh, (int) (offset & 0xffffffUL),
                                (int) ((offset >> 24) & 0xffUL));
}

uint8_t ffs_flseek_p(FIL *fh, uint32_t *offset)
{
    return (uint8_t) acc_rt_mos(FFS_FLSEEK_P, (int) fh, (int) offset, 0);
}

uint8_t ffs_ftruncate(FIL *fh)
{
    return (uint8_t) acc_rt_mos(FFS_FTRUNCATE, (int) fh, 0, 0);
}

uint8_t ffs_fsync(FIL *fh)
{
    return (uint8_t) acc_rt_mos(FFS_FSYNC, (int) fh, 0, 0);
}

/* The buffer back in DE, or null. */
uint8_t *ffs_fgets(FIL *fh, char *buffer, uint24_t buffersize)
{
    REGS(r);

    r.hl = (uint24_t) fh;
    r.de = (uint24_t) buffer;
    r.bc = buffersize;
    call(&r, FFS_FGETS);

    return (uint8_t *) r.de;
}

uint24_t ffs_fputc(FIL *fh, char c)
{
    REGS(r);

    r.hl = (uint24_t) fh;
    r.bc = LOW(c);
    call(&r, FFS_FPUTC);

    return r.bc;
}

uint24_t ffs_fputs(FIL *fh, const char *string)
{
    REGS(r);

    r.hl = (uint24_t) fh;
    r.de = (uint24_t) string;
    call(&r, FFS_FPUTS);

    return r.bc;
}

/* Formatted by acc's own printf, which knows every conversion, and written
 * a block at a time. MOS's f_printf is a C function of its own, which
 * mos_getfunction(MOS_FUNC_F_PRINTF, 0) will find for a program that wants
 * it. */
extern char  *acc_sink_buf;
extern FILE  *acc_sink_file;
extern int  (*acc_sink_fn)(int);
int acc_format(const char *fmt, va_list ap);

static FIL  *fprintf_to;
static char  fprintf_buf[64];
static int   fprintf_held;

static void fprintf_flush(void)
{
    if (fprintf_held)
        ffs_fwrite(fprintf_to, fprintf_buf, (uint24_t) fprintf_held);
    fprintf_held = 0;
}

static int fprintf_put(int c)
{
    if (fprintf_held == (int) sizeof fprintf_buf)
        fprintf_flush();
    fprintf_buf[fprintf_held++] = (char) c;

    return c;
}

int ffs_fprintf(FIL *fp, const char *str, ...)
{
    va_list ap;
    int n;

    fprintf_to = fp;
    fprintf_held = 0;
    acc_sink_buf = NULL;
    acc_sink_file = NULL;
    acc_sink_fn = fprintf_put;
    va_start(ap, str);
    n = acc_format(str, ap);
    va_end(ap);
    acc_sink_fn = NULL;
    fprintf_flush();

    return n;
}

uint8_t ffs_ftell(FIL *fh, uint32_t *result)
{
    return (uint8_t) acc_rt_mos(FFS_FTELL, (int) fh, (int) result, 0);
}

uint8_t ffs_feof(FIL *fh)
{
    return (uint8_t) acc_rt_mos(FFS_FEOF, (int) fh, 0, 0);
}

uint8_t ffs_fsize(FIL *fh, uint32_t *result)
{
    return (uint8_t) acc_rt_mos(FFS_FSIZE, (int) fh, (int) result, 0);
}

uint8_t ffs_ferror(FIL *fh)
{
    return (uint8_t) acc_rt_mos(FFS_FERROR, (int) fh, 0, 0);
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
    return (uint8_t) acc_rt_mos(FFS_DREAD, (int) dir_handle, (int) fil_handle, 0);
}

/* The pattern in IX. */
uint8_t ffs_dfindfirst(DIR *dir_handle, FILINFO *fil_handle,
                       const char *dirpath, const char *pattern)
{
    REGS(r);

    r.hl = (uint24_t) dir_handle;
    r.de = (uint24_t) fil_handle;
    r.bc = (uint24_t) dirpath;
    r.ix = (uint24_t) pattern;

    return call(&r, FFS_DFINDFIRST);
}

uint8_t ffs_dfindnext(DIR *dir_handle, FILINFO *fil_handle)
{
    return (uint8_t) acc_rt_mos(FFS_DFINDNEXT, (int) dir_handle, (int) fil_handle, 0);
}

uint8_t ffs_stat(FILINFO *fil_handle, const char *filename)
{
    return (uint8_t) acc_rt_mos(FFS_STAT, (int) fil_handle, (int) filename, 0);
}

uint8_t ffs_unlink(const char *filepath)
{
    return (uint8_t) acc_rt_mos(FFS_UNLINK, (int) filepath, 0, 0);
}

uint8_t ffs_rename(const char *sourcefilepath, const char *destfilepath)
{
    return (uint8_t) acc_rt_mos(FFS_RENAME, (int) sourcefilepath,
                                (int) destfilepath, 0);
}

uint8_t ffs_mkdir(const char *dirname)
{
    return (uint8_t) acc_rt_mos(FFS_MKDIR, (int) dirname, 0, 0);
}

uint8_t ffs_chdir(const char *dirname)
{
    return (uint8_t) acc_rt_mos(FFS_CHDIR, (int) dirname, 0, 0);
}

uint8_t ffs_getcwd(char *dirpath, uint24_t bufferlength)
{
    return (uint8_t) acc_rt_mos(FFS_GETCWD, (int) dirpath, 0, (int) bufferlength);
}

uint8_t ffs_mount(FATFS *fs, const char *volpath, uint8_t options)
{
    return (uint8_t) acc_rt_mos(FFS_MOUNT, (int) fs, (int) volpath, options);
}

uint8_t ffs_getfree(char *path, uint32_t *freeclusters, uint32_t *clustersize)
{
    return (uint8_t) acc_rt_mos(FFS_GETFREE, (int) path, (int) freeclusters,
                                (int) clustersize);
}

uint8_t ffs_getlabel(char *path, char *label, uint32_t *volserial)
{
    return (uint8_t) acc_rt_mos(FFS_GETLABEL, (int) path, (int) label,
                                (int) volserial);
}

uint8_t ffs_setlabel(const char *volumelabel)
{
    return (uint8_t) acc_rt_mos(FFS_SETLABEL, (int) volumelabel, 0, 0);
}

/* The ones MOS numbers and answers MOS_NOT_IMPLEMENTED for. They are asked
 * all the same, so that a MOS that one day has them answers for itself. */
uint8_t ffs_fforward(FIL *fh, uint24_t (*func)(const uint8_t *, uint24_t),
                     uint24_t btf, uint24_t *bf)
{
    (void) func;
    (void) btf;
    (void) bf;

    return (uint8_t) acc_rt_mos(FFS_FFORWARD, (int) fh, 0, 0);
}

uint8_t ffs_expand(FIL *fh, uint32_t fsz, uint8_t opt)
{
    (void) fsz;
    (void) opt;

    return (uint8_t) acc_rt_mos(FFS_FEXPAND, (int) fh, 0, 0);
}

uint8_t ffs_chmod(const char *path, uint8_t attr, uint8_t mask)
{
    return (uint8_t) acc_rt_mos(FFS_CHMOD, (int) path, attr, mask);
}

uint8_t ffs_utime(const char *path, const FILINFO *fno)
{
    return (uint8_t) acc_rt_mos(FFS_UTIME, (int) path, (int) fno, 0);
}

uint8_t ffs_chdrive(const char *path)
{
    return (uint8_t) acc_rt_mos(FFS_CHDRIVE, (int) path, 0, 0);
}

uint8_t ffs_mkfs(const char *path, const void *opt, void *work, uint24_t len)
{
    (void) work;
    (void) len;

    return (uint8_t) acc_rt_mos(FFS_MKFS, (int) path, (int) opt, 0);
}

uint8_t ffs_fdisk(uint8_t pdrv, const uint32_t *ptbl, void *work)
{
    (void) work;

    return (uint8_t) acc_rt_mos(FFS_FDISK, (int) ptbl, 0, pdrv);
}

uint8_t ffs_setcp(uint16_t cp)
{
    return (uint8_t) acc_rt_mos(FFS_SETCP, 0, 0, cp);
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

volatile SYSVAR *__acc_sys_vars(void)
{
    return (volatile SYSVAR *) mos_sysvars();
}

#define B(at)   (((volatile uint8_t *) mos_sysvars())[at])
#define W(at)   ((uint16_t) (B(at) | B((at) + 1) << 8))

uint32_t getsysvar_time(void)
{
    return (uint32_t) B(sysvar_time) | (uint32_t) B(sysvar_time + 1) << 8
           | (uint32_t) B(sysvar_time + 2) << 16
           | (uint32_t) B(sysvar_time + 3) << 24;
}

uint8_t  getsysvar_vdp_pflags(void)    { return B(sysvar_vdp_pflags); }
uint8_t  getsysvar_keyascii(void)      { return B(sysvar_keyascii); }
uint8_t  getsysvar_keymods(void)       { return B(sysvar_keymods); }
uint8_t  getsysvar_cursorX(void)       { return B(sysvar_cursorX); }
uint8_t  getsysvar_cursorY(void)       { return B(sysvar_cursorY); }
uint8_t  getsysvar_scrchar(void)       { return B(sysvar_scrchar); }
uint8_t  getsysvar_audioChannel(void)  { return B(sysvar_audioChannel); }
uint8_t  getsysvar_audioSuccess(void)  { return B(sysvar_audioSuccess); }
uint16_t getsysvar_scrwidth(void)      { return W(sysvar_scrWidth); }
uint16_t getsysvar_scrheight(void)     { return W(sysvar_scrHeight); }
uint8_t  getsysvar_scrCols(void)       { return B(sysvar_scrCols); }
uint8_t  getsysvar_scrRows(void)       { return B(sysvar_scrRows); }
uint8_t  getsysvar_scrColours(void)    { return B(sysvar_scrColours); }
uint8_t  getsysvar_scrpixelIndex(void) { return B(sysvar_scrpixelIndex); }
uint8_t  getsysvar_vkeycode(void)      { return B(sysvar_vkeycode); }
uint8_t  getsysvar_vkeydown(void)      { return B(sysvar_vkeydown); }
uint8_t  getsysvar_vkeycount(void)     { return B(sysvar_vkeycount); }
uint16_t getsysvar_keydelay(void)      { return W(sysvar_keydelay); }
uint16_t getsysvar_keyrate(void)       { return W(sysvar_keyrate); }
uint8_t  getsysvar_keyled(void)        { return B(sysvar_keyled); }
uint8_t  getsysvar_scrMode(void)       { return B(sysvar_scrMode); }
uint8_t  getsysvar_rtcEnable(void)     { return B(sysvar_rtcEnable); }
uint16_t getsysvar_mouseX(void)        { return W(sysvar_mouseX); }
uint16_t getsysvar_mouseY(void)        { return W(sysvar_mouseY); }
uint8_t  getsysvar_mouseButtons(void)  { return B(sysvar_mouseButtons); }
int8_t   getsysvar_mouseWheel(void)    { return (int8_t) B(sysvar_mouseWheel); }
int16_t  getsysvar_mouseXDelta(void)   { return (int16_t) W(sysvar_mouseXDelta); }
int16_t  getsysvar_mouseYDelta(void)   { return (int16_t) W(sysvar_mouseYDelta); }
uint8_t  getsysvar_gp(void)            { return B(sysvar_gp); }

/* Three bytes, R, B, G, as one number, lowest first. */
uint24_t getsysvar_scrpixel(void)
{
    return (uint24_t) B(sysvar_scrpixel) | (uint24_t) B(sysvar_scrpixel + 1) << 8
           | (uint24_t) B(sysvar_scrpixel + 2) << 16;
}

volatile SYSVAR_RTCDATA *getsysvar_rtc(void)
{
    return (volatile SYSVAR_RTCDATA *) (mos_sysvars() + sysvar_rtc);
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
