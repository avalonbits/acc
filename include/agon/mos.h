/* MOS, as libagon names it.
 *
 * The names, the arguments and the numbers they answer with are libagon's,
 * so that a program written against one can be built against the other. What
 * is behind them is not: libagon has a routine of assembly per call, and
 * these are C over one routine that makes a MOS call with whatever registers
 * that call wants.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ACC_AGON_MOS_H
#define ACC_AGON_MOS_H

#include <stdint.h>

/* How a file is opened, as MOS's own FatFS names them. */
#define FA_READ           0x01
#define FA_WRITE          0x02
#define FA_OPEN_EXISTING  0x00
#define FA_CREATE_NEW     0x04
#define FA_CREATE_ALWAYS  0x08
#define FA_OPEN_ALWAYS    0x10
#define FA_OPEN_APPEND    0x30

/* What a FatFS call answers with. MOS's own numbering, since these come
 * back from it. */
typedef enum {
    FR_OK = 0,
    FR_DISK_ERR,
    FR_INT_ERR,
    FR_NOT_READY,
    FR_NO_FILE,
    FR_NO_PATH,
    FR_INVALID_NAME,
    FR_DENIED,
    FR_EXIST,
    FR_INVALID_OBJECT,
    FR_WRITE_PROTECTED,
    FR_INVALID_DRIVE,
    FR_NOT_ENABLED,
    FR_NO_FILESYSTEM,
    FR_MKFS_ABORTED,
    FR_TIMEOUT,
    FR_LOCKED,
    FR_NOT_ENOUGH_CORE,
    FR_TOO_MANY_OPEN_FILES,
    FR_INVALID_PARAMETER
} FRESULT;

/* What a directory entry says about itself, as MOS's FatFS says it. */
#define AM_RDO  0x01
#define AM_HID  0x02
#define AM_SYS  0x04
#define AM_DIR  0x10
#define AM_ARC  0x20

/* Where each system variable is, counting from the start of them. */
#define sysvar_time            0x00    /* 4: centiseconds, twice a frame */
#define sysvar_vdp_pflags      0x04    /* 1: which VDP answers have arrived */
#define sysvar_keyascii        0x05    /* 1: the key held, or zero */
#define sysvar_keymods         0x06    /* 1: shift, control and the rest */
#define sysvar_cursorX         0x07    /* 1 */
#define sysvar_cursorY         0x08    /* 1 */
#define sysvar_scrchar         0x09    /* 1: what was read off the screen */
#define sysvar_scrpixel        0x0a    /* 3: and its colour */
#define sysvar_audioChannel    0x0d    /* 1 */
#define sysvar_audioSuccess    0x0e    /* 1 */
#define sysvar_scrWidth        0x0f    /* 2: pixels */
#define sysvar_scrHeight       0x11    /* 2: pixels */
#define sysvar_scrCols         0x13    /* 1: characters */
#define sysvar_scrRows         0x14    /* 1: characters */
#define sysvar_scrColours      0x15    /* 1 */
#define sysvar_scrpixelIndex   0x16    /* 1 */
#define sysvar_vkeycode        0x17    /* 1 */
#define sysvar_vkeydown        0x18    /* 1 */
#define sysvar_vkeycount       0x19    /* 1 */
#define sysvar_rtc             0x1a    /* 6 */
#define sysvar_keydelay        0x22    /* 2 */
#define sysvar_keyrate         0x24    /* 2 */
#define sysvar_keyled          0x26    /* 1 */

/* Which of the VDP's answers a flag in sysvar_vdp_pflags stands for. */
#define vdp_pflag_cursor       0x01
#define vdp_pflag_scrchar      0x02
#define vdp_pflag_point        0x04
#define vdp_pflag_audio        0x08
#define vdp_pflag_mode         0x10
#define vdp_pflag_rtc          0x20
#define vdp_pflag_mouse        0x40

/* What a file or a directory is, laid out as MOS lays it out: these are
 * passed to it and filled in by it, so the shape is not ours to choose. */
typedef struct {
    uint24_t *fs;
    uint16_t  id;
    uint8_t   attr;
    uint8_t   stat;
    uint32_t  sclust;
    uint32_t  objsize;
} FFOBJID;

typedef struct {
    FFOBJID   obj;
    uint8_t   flag;
    uint8_t   err;
    uint32_t  fptr;
    uint32_t  clust;
    uint32_t  sect;
    uint32_t  dir_sect;
    uint24_t *dir_ptr;
} FIL;

typedef struct {
    uint32_t fsize;
    uint16_t fdate;
    uint16_t ftime;
    uint8_t  fattrib;
    char     altname[13];
    char     fname[256];
} FILINFO;

typedef struct {
    FFOBJID     obj;
    uint32_t    dptr;
    uint32_t    clust;
    uint32_t    sect;
    uint8_t    *dir;
    uint8_t     fn[12];
    const char *pat;
} DIR;

/* The console. */
int      putch(int c);
char     getch(void);
void     mos_puts(const char *buffer, uint24_t size, char delimiter);
uint8_t  mos_editline(char *buffer, uint24_t size, uint8_t clear);

/* Files and directories. */
uint8_t  mos_del(const char *filename);
uint8_t  mos_ren(const char *filename, const char *newname);
uint8_t  mos_mkdir(const char *path);
uint8_t  mos_fopen(const char *filename, uint8_t mode);
uint8_t  mos_fclose(uint8_t fh);
uint24_t mos_fread(uint8_t fh, char *buffer, uint24_t numbytes);
uint24_t mos_fwrite(uint8_t fh, char *buffer, uint24_t numbytes);
uint8_t  mos_flseek(uint8_t fh, uint32_t offset);
FIL     *mos_getfil(uint8_t fh);

uint8_t  ffs_dopen(DIR *dir_handle, const char *dir_path);
uint8_t  ffs_dclose(DIR *dir_handle);
uint8_t  ffs_dread(DIR *dir_handle, FILINFO *fil_handle);
uint8_t  ffs_stat(FILINFO *fil_handle, const char *filename);

/* The system variables, and the ones a program usually wants out of them. */
uint8_t *mos_sysvars(void);
void     waitvblank(void);
uint8_t  getsysvar_keyascii(void);
uint8_t  getsysvar_keymods(void);
uint8_t  getsysvar_cursorX(void);
uint8_t  getsysvar_cursorY(void);
uint16_t getsysvar_scrwidth(void);
uint16_t getsysvar_scrheight(void);
uint8_t  getsysvar_scrCols(void);
uint8_t  getsysvar_scrRows(void);
uint8_t  getsysvar_scrColours(void);

#endif
