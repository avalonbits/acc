/* MOS, as libagon names it, and all of the API of MOS 3.0.2.
 *
 * The names, the types, the arguments and the numbers they answer with are
 * libagon's, so that a program written against one can be built against
 * the other. Past what libagon has, the rest of MOS 3.0.2's API is here
 * too, named the way MOS's own documentation names it: mos_getfunction,
 * and the FatFS calls MOS answers with "not implemented" (23), which are
 * declared so that a program can ask.
 *
 * What is behind them is not libagon's: each is C over one routine of acc's
 * runtime that makes a MOS call with whatever registers that call wants and
 * hands back every register it answers in -- see acc_rt_mos_call in
 * src/rt/helpers.s. A call marked "MOS 3" is not in MOS 2; it answers 23
 * there.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#ifndef ACC_AGON_MOS_H
#define ACC_AGON_MOS_H

#include <stdint.h>

/* How MOS's FatFS was configured, which says what its structures hold. */
#define FFCONF_DEF          86631
#define FF_FS_READONLY      0
#define FF_FS_MINIMIZE      0
#define FF_USE_FIND         1
#define FF_USE_MKFS         0
#define FF_USE_FASTSEEK     0
#define FF_USE_EXPAND       0
#define FF_USE_CHMOD        0
#define FF_USE_LABEL        1
#define FF_USE_FORWARD      0
#define FF_USE_STRFUNC      1
#define FF_CODE_PAGE        437
#define FF_USE_LFN          2
#define FF_MAX_LFN          255
#define FF_LFN_UNICODE      0
#define FF_LFN_BUF          255
#define FF_SFN_BUF          12
#define FF_FS_RPATH         2
#define FF_VOLUMES          1
#define FF_MULTI_PARTITION  0
#define FF_MIN_SS           512
#define FF_MAX_SS           512
#define FF_LBA64            0
#define FF_FS_TINY          1
#define FF_FS_EXFAT         0
#define FF_FS_NORTC         0
#define FF_FS_NOFSINFO      0
#define FF_FS_LOCK          0
#define FF_FS_REENTRANT     0

/* How a file is opened, as MOS's own FatFS names them. */
#define FA_READ           0x01
#define FA_WRITE          0x02
#define FA_OPEN_EXISTING  0x00
#define FA_CREATE_NEW     0x04
#define FA_CREATE_ALWAYS  0x08
#define FA_OPEN_ALWAYS    0x10
#define FA_OPEN_APPEND    0x30

/* What a directory entry says about itself. */
#define AM_RDO  0x01
#define AM_HID  0x02
#define AM_SYS  0x04
#define AM_DIR  0x10
#define AM_ARC  0x20

/* The I2C bus: its speeds, and what a transfer answers. */
#define I2C_SPEED_57600     0x01
#define I2C_SPEED_115200    0x02
#define I2C_SPEED_230400    0x03
#define RET_OK              0x00
#define RET_NORESPONSE      0x01
#define RET_DATA_NACK       0x02
#define RET_ARB_LOST        0x04
#define RET_BUS_ERROR       0x08

/* What a FatFS call answers with. */
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

/* And what a MOS call does, past FatFS's: 20 for a call made from Z80
 * mode, 23 for a call this MOS does not have. */
#define MOS_INVALID_COMMAND     20
#define MOS_NOT_IMPLEMENTED     23

/* Where each system variable is, counting from the start of them. */
#define sysvar_time            0x00    /* 4: centiseconds, twice a frame */
#define sysvar_vdp_pflags      0x04    /* 1: which VDP answers have arrived */
#define sysvar_keyascii        0x05    /* 1: the key held, or zero */
#define sysvar_keymods         0x06    /* 1: shift, control and the rest */
#define sysvar_cursorX         0x07    /* 1 */
#define sysvar_cursorY         0x08    /* 1 */
#define sysvar_scrchar         0x09    /* 1: what was read off the screen */
#define sysvar_scrpixel        0x0a    /* 3: and its colour, R, B, G */
#define sysvar_audioChannel    0x0d    /* 1 */
#define sysvar_audioSuccess    0x0e    /* 1: 1 if the note was queued */
#define syscar_audioSuccess    0x0e    /* libagon's spelling */
#define sysvar_scrWidth        0x0f    /* 2: pixels */
#define sysvar_scrHeight       0x11    /* 2: pixels */
#define sysvar_scrCols         0x13    /* 1: characters */
#define sysvar_scrRows         0x14    /* 1: characters */
#define sysvar_scrColours      0x15    /* 1 */
#define sysvar_scrpixelIndex   0x16    /* 1: the palette index of that pixel */
#define sysvar_vkeycode        0x17    /* 1: FabGL's virtual key code */
#define sysvar_vkeydown        0x18    /* 1: 1 for down, 0 for up */
#define sysvar_vkeycount       0x19    /* 1: one more at every key packet */
#define sysvar_rtc             0x1a    /* 6: the clock, packed: see mos_unpackrtc */
#define sysvar_spare           0x20    /* 2 */
#define sysvar_keydelay        0x22    /* 2: keyboard repeat delay */
#define sysvar_keyrate         0x24    /* 2: and rate */
#define sysvar_keyled          0x26    /* 1: keyboard LEDs */
#define sysvar_scrMode         0x27    /* 1: screen mode */
#define sysvar_rtcEnable       0x28    /* 1: 1 to use the ESP32's clock */
#define sysvar_mouseX          0x29    /* 2 */
#define sysvar_mouseY          0x2b    /* 2 */
#define sysvar_mouseButtons    0x2d    /* 1 */
#define sysvar_mouseWheel      0x2e    /* 1: wheel delta */
#define sysvar_mouseXDelta     0x2f    /* 2 */
#define sysvar_mouseYDelta     0x31    /* 2 */
#define sysvar_gp              0x37    /* 1: the general poll's answer */

/* Which of the VDP's answers a flag in sysvar_vdp_pflags stands for. */
#define vdp_pflag_cursor       0x01
#define vdp_pflag_scrchar      0x02
#define vdp_pflag_point        0x04
#define vdp_pflag_audio        0x08
#define vdp_pflag_mode         0x10
#define vdp_pflag_rtc          0x20
#define vdp_pflag_mouse        0x40

/* libagon's reading of the six bytes at sysvar_rtc. MOS has kept them
 * packed since 1.04 -- see mos_unpackrtc, whose vdp_time_t is what they
 * mean -- so this is here for programs that name it, and is not what the
 * bytes hold. */
typedef struct {
    uint8_t year;               /* since 1980 */
    uint8_t month;              /* 0 to 11 */
    uint8_t day;                /* 1 to 31 */
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} SYSVAR_RTCDATA;

/* The clock, unpacked: what mos_unpackrtc fills in. MOS 3. */
typedef struct {
    uint16_t year;
    uint8_t  month;             /* 0 to 11 */
    uint8_t  day;               /* 1 to 31 */
    uint8_t  dayOfWeek;         /* 0 to 6, from Sunday */
    uint16_t dayOfYear;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
} vdp_time_t;

/* The flags mos_unpackrtc takes. */
#define RTC_REFRESH_BEFORE     0x01    /* ask the VDP first */
#define RTC_REFRESH_AFTER      0x02    /* and again afterwards */

/* The system variables as one structure, laid out as MOS lays them out. */
typedef struct {
    uint32_t time;
    uint8_t  vdp_pflags;
    uint8_t  keyascii;
    uint8_t  keymods;
    uint8_t  cursorX;
    uint8_t  cursorY;
    uint8_t  scrchar;
    union {
        uint24_t scrpixel;
        struct {
            uint8_t scrpixelR;
            uint8_t scrpixelB;
            uint8_t scrpixelG;
        };
    };
    uint8_t  audioChannel;
    uint8_t  audioSuccess;
    uint16_t scrWidth;
    uint16_t scrHeight;
    uint8_t  scrCols;
    uint8_t  scrRows;
    uint8_t  scrColours;
    uint8_t  scrpixelIndex;
    uint8_t  vkeycode;
    uint8_t  vkeydown;
    uint8_t  vkeycount;
    SYSVAR_RTCDATA rtc;
    uint16_t spare;
    uint16_t keydelay;
    uint16_t keyrate;
    uint8_t  keyled;
    uint8_t  scrMode;
    uint8_t  rtcEnable;
    uint16_t mouseX;
    uint16_t mouseY;
    uint8_t  mouseButtons;
    uint8_t  mouseWheel;
    uint16_t mouseXDelta;
    uint16_t mouseYDelta;
    uint8_t  reserved[4];
    uint8_t  gp;                /* MOS 3 */
} SYSVAR;

/* libagon's pointer to them, which its startup sets. acc's has nothing to
 * set it with, so the name asks MOS once and remembers the answer. */
volatile SYSVAR *__acc_sys_vars(void);
#define sys_vars (__acc_sys_vars())

/* UART1's settings, for mos_uopen. */
typedef struct {
    int24_t baudRate;
    uint8_t dataBits;
    uint8_t stopBits;
    uint8_t parity;
    uint8_t flowcontrol;        /* 0 none, 1 hardware */
    uint8_t eir;                /* the interrupts to enable */
} UART;

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

/* A volume, as FatFS keeps it. */
typedef struct {
    uint8_t   fs_type;
    uint8_t   pdrv;
    uint8_t   n_fats;
    uint8_t   wflag;
    uint8_t   fsi_flag;
    uint16_t  id;
    uint16_t  n_rootdir;
    uint16_t  csize;
    uint16_t *lfnbuf;
    uint32_t  last_clst;
    uint32_t  free_clst;
    uint32_t  cdir;
    uint32_t  n_fatent;
    uint32_t  fsize;
    uint32_t  volbase;
    uint32_t  fatbase;
    uint32_t  dirbase;
    uint32_t  database;
    uint32_t  winsect;
    uint8_t   win[FF_MAX_SS];
} FATFS;

/* The console. */
int      putch(int c);
char     getch(void);
void     waitvblank(void);
void     mos_puts(const char *buffer, uint24_t size, char delimiter);
void     mos_putstring(const char *string);

/* The system variables one at a time. */
uint8_t *mos_sysvars(void);
uint32_t getsysvar_time(void);
uint8_t  getsysvar_vdp_pflags(void);
uint8_t  getsysvar_keyascii(void);
uint8_t  getsysvar_keymods(void);
uint8_t  getsysvar_cursorX(void);
uint8_t  getsysvar_cursorY(void);
uint8_t  getsysvar_scrchar(void);
uint24_t getsysvar_scrpixel(void);
uint8_t  getsysvar_audioChannel(void);
uint8_t  getsysvar_audioSuccess(void);
uint16_t getsysvar_scrwidth(void);
uint16_t getsysvar_scrheight(void);
uint8_t  getsysvar_scrCols(void);
uint8_t  getsysvar_scrRows(void);
uint8_t  getsysvar_scrColours(void);
uint8_t  getsysvar_scrpixelIndex(void);
uint8_t  getsysvar_vkeycode(void);
uint8_t  getsysvar_vkeydown(void);
uint8_t  getsysvar_vkeycount(void);
volatile SYSVAR_RTCDATA *getsysvar_rtc(void);
uint16_t getsysvar_keydelay(void);
uint16_t getsysvar_keyrate(void);
uint8_t  getsysvar_keyled(void);
uint8_t  getsysvar_scrMode(void);
uint8_t  getsysvar_rtcEnable(void);
uint16_t getsysvar_mouseX(void);
uint16_t getsysvar_mouseY(void);
uint8_t  getsysvar_mouseButtons(void);
int8_t   getsysvar_mouseWheel(void);
int16_t  getsysvar_mouseXDelta(void);
int16_t  getsysvar_mouseYDelta(void);
uint8_t  getsysvar_gp(void);

/* The MOS calls, by number. */
uint8_t  mos_load(const char *filename, char *address, uint24_t maxsize);
uint8_t  mos_save(const char *filename, char *address, uint24_t nbytes);
uint8_t  mos_cd(const char *path);
uint8_t  mos_dir(const char *path);
uint8_t  mos_del(const char *filename);
uint8_t  mos_ren(const char *filename, const char *newname);
uint8_t  mos_copy(const char *source, const char *destination);
uint8_t  mos_mkdir(const char *path);
uint8_t  mos_editline(char *buffer, uint24_t bufferlength, uint8_t clearbuffer);
uint8_t  mos_fopen(const char *filename, uint8_t mode);    /* a handle, or 0 */
uint8_t  mos_fclose(uint8_t fh);                /* how many are still open */
char     mos_fgetc(uint8_t fh);
void     mos_fputc(uint8_t fh, char c);
uint8_t  mos_feof(uint8_t fh);
void     mos_getError(uint8_t code, char *buffer, uint24_t bufferlength);
uint8_t  mos_oscli(char *command, char **argv, uint24_t argc);
uint8_t  mos_getrtc(char *buffer);
void     mos_setrtc(uint8_t *timedata);
void    *mos_setintvector(uint8_t vector, void (*handler)(void));
uint8_t  mos_uopen(UART *settings);
void     mos_uclose(void);
int      mos_ugetc(void);               /* 0 to 255, or 256 and up for none */
int      mos_ugetc_nb(void);            /* without waiting: -1 for none */
uint8_t  mos_uputc(int c);              /* 0 if the port is not open */
FIL     *mos_getfil(uint8_t fh);
uint24_t mos_fread(uint8_t fh, char *buffer, uint24_t numbytes);
uint24_t mos_fwrite(uint8_t fh, char *buffer, uint24_t numbytes);
uint8_t  mos_flseek(uint8_t fh, uint32_t offset);
void     mos_setkbvector(void (*handler)(void), uint8_t addresslength);
uint8_t *mos_getkbmap(void);
void     mos_i2c_open(uint8_t frequency);
void     mos_i2c_close(void);
uint8_t  mos_i2c_write(uint8_t i2c_address, uint8_t size, unsigned char *buffer);
uint8_t  mos_i2c_read(uint8_t i2c_address, uint8_t size, unsigned char *buffer);
void     mos_unpackrtc(vdp_time_t *buffer, uint8_t flags);            /* MOS 3 */
uint8_t  mos_flseek_p(uint8_t fh, uint32_t offset);                   /* MOS 3 */

/* Strings. MOS 3. */
int8_t   mos_pmatch(const char *pattern, const char *string, uint8_t flags);
void     mos_getargument(char **arg, char **argend, const char *source,
                         uint24_t argnumber);
uint8_t  mos_extractstring(char **result, char **next, const char *source,
                           const char *dividers, uint8_t flags);
uint8_t  mos_extractnumber(uint24_t *result, char **next, const char *source,
                           const char *dividers, uint8_t flags);
uint8_t  mos_escapestring(uint24_t *resultlength, const char *source,
                          char *buffer, uint24_t bufferlength);

/* System variables of MOS's own -- names and values, as *SET and *SHOW
 * have them -- and GSTrans. MOS 3. */
int      mos_setvarval(char *name, void *value, char **actualName, uint8_t *type);
int      mos_readvarval(char *namePattern, void *value, char **actualName,
                        int *length, uint8_t *typeFlag);
uint8_t  mos_gsinit(const char *source, char **transinfo_ptr, uint8_t flags);
uint8_t  mos_gsread(char *char_read, char **transinfo_ptr);
int      mos_gstrans(char *source, char *dest, int destLen, int *read,
                     uint8_t flags);
int      mos_substituteargs(char *tpl, char *args, char *dest, int length,
                            uint8_t flags);

/* Paths. MOS 3. */
int      mos_resolvepath(char *filepath, char *resolvedPath, int *length,
                         uint8_t *index, DIR *dir, uint8_t flags);
int      mos_getdirforpath(char *srcPath, char *dir, int *length, uint8_t index);
char    *mos_getleafname(const char *pathname);
uint8_t  mos_isdirectory(const char *pathname);
int      mos_getabsolutepath(char *path, char *resolved, int *length);

/* The VDP's answers. MOS 3. */
uint8_t  mos_clearvdpflags(uint8_t bitmask);
uint8_t  mos_waitforvdpflags(uint8_t bitmask);

/* The address of one of MOS's own C functions, which take their arguments
 * as acc passes them and can be called through a pointer. 0 for a number
 * MOS has none for. MOS 3. */
void    *mos_getfunction(uint8_t number, uint8_t flags);

#define MOS_FUNC_SD_INIT              0x00
#define MOS_FUNC_SD_READBLOCKS        0x01
#define MOS_FUNC_SD_WRITEBLOCKS       0x02
#define MOS_FUNC_F_PRINTF             0x05
#define MOS_FUNC_F_FINDFIRST          0x06
#define MOS_FUNC_F_FINDNEXT           0x07
#define MOS_FUNC_OPEN_UART1           0x08
#define MOS_FUNC_SETVARVAL            0x09
#define MOS_FUNC_READVARVAL           0x0a
#define MOS_FUNC_GSTRANS              0x0b
#define MOS_FUNC_SUBSTITUTEARGS       0x0c
#define MOS_FUNC_RESOLVEPATH          0x0d
#define MOS_FUNC_GETDIRECTORYFORPATH  0x0e
#define MOS_FUNC_RESOLVERELATIVEPATH  0x0f
#define MOS_FUNC_GETSYSVARS           0x10
#define MOS_FUNC_GETKBMAP             0x11

/* The SD card, a block at a time, behind a code that has to be asked for
 * first. MOS 3. In 3.0.2 the block calls check the code against the wrong
 * bytes and the write reads: see lib/mos.c. */
uint24_t sd_getunlockcode(void);
uint8_t  sd_init(uint24_t unlockcode);
uint8_t  sd_readblocks(uint32_t sector, uint8_t *buf, uint24_t count);
uint8_t  sd_writeblocks(uint32_t sector, uint8_t *buf, uint24_t count);

/* MOS's FatFS. */
uint8_t  ffs_fopen(FIL *fh, const char *filename, uint8_t mode);
uint8_t  ffs_fclose(FIL *fh);
uint24_t ffs_fread(FIL *fh, char *buffer, uint24_t numbytes);
uint24_t ffs_fwrite(FIL *fh, const char *buffer, uint24_t numbytes);
uint8_t  ffs_flseek(FIL *fh, uint32_t offset);
uint8_t  ffs_ftruncate(FIL *fh);
uint8_t  ffs_fsync(FIL *fh);                                           /* MOS 3 */
uint8_t *ffs_fgets(FIL *fh, char *buffer, uint24_t buffersize);        /* MOS 3 */
uint24_t ffs_fputc(FIL *fh, char c);                                   /* MOS 3 */
uint24_t ffs_fputs(FIL *fh, const char *string);                       /* MOS 3 */
int      ffs_fprintf(FIL *fp, const char *str, ...);                   /* MOS 3 */
uint8_t  ffs_ftell(FIL *fh, uint32_t *result);                         /* MOS 3 */
uint8_t  ffs_feof(FIL *fh);                                            /* MOS 3 */
uint8_t  ffs_fsize(FIL *fh, uint32_t *result);                         /* MOS 3 */
uint8_t  ffs_ferror(FIL *fh);                                          /* MOS 3 */
uint8_t  ffs_dopen(DIR *dir_handle, const char *dir_path);
uint8_t  ffs_dclose(DIR *dir_handle);
uint8_t  ffs_dread(DIR *dir_handle, FILINFO *fil_handle);
uint8_t  ffs_dfindfirst(DIR *dir_handle, FILINFO *fil_handle,
                        const char *dirpath, const char *pattern);      /* MOS 3 */
uint8_t  ffs_dfindnext(DIR *dir_handle, FILINFO *fil_handle);          /* MOS 3 */
uint8_t  ffs_stat(FILINFO *fil_handle, const char *filename);
uint8_t  ffs_unlink(const char *filepath);                             /* MOS 3 */
uint8_t  ffs_rename(const char *sourcefilepath, const char *destfilepath); /* MOS 3 */
uint8_t  ffs_mkdir(const char *dirname);                               /* MOS 3 */
uint8_t  ffs_chdir(const char *dirname);                               /* MOS 3 */
uint8_t  ffs_getcwd(char *dirpath, uint24_t bufferlength);
uint8_t  ffs_mount(FATFS *fs, const char *volpath, uint8_t options);   /* MOS 3 */
uint8_t  ffs_getfree(char *path, uint32_t *freeclusters, uint32_t *clustersize); /* MOS 3 */
uint8_t  ffs_getlabel(char *path, char *label, uint32_t *volserial);   /* MOS 3 */
uint8_t  ffs_setlabel(const char *volumelabel);                        /* MOS 3 */
uint8_t  ffs_flseek_p(FIL *fh, uint32_t *offset);                      /* MOS 3 */

/* The FatFS calls MOS numbers and has no code behind: each answers
 * MOS_NOT_IMPLEMENTED, and is here so that a program can find that out
 * rather than fail to link. The arguments are FatFS's. */
uint8_t  ffs_fforward(FIL *fh, uint24_t (*func)(const uint8_t *, uint24_t),
                      uint24_t btf, uint24_t *bf);
uint8_t  ffs_expand(FIL *fh, uint32_t fsz, uint8_t opt);
uint8_t  ffs_chmod(const char *path, uint8_t attr, uint8_t mask);
uint8_t  ffs_utime(const char *path, const FILINFO *fno);
uint8_t  ffs_chdrive(const char *path);
uint8_t  ffs_mkfs(const char *path, const void *opt, void *work, uint24_t len);
uint8_t  ffs_fdisk(uint8_t pdrv, const uint32_t *ptbl, void *work);
uint8_t  ffs_setcp(uint16_t cp);

#endif
