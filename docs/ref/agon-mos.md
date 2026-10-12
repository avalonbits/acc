# `agon/mos.h`

MOS, the Agon's operating system: the console, the system variables,
files by name and by handle, the clock, the serial port, I2C, MOS's
strings, variables and paths, the SD card a block at a time, and MOS's
FatFS. A program includes it as `#include <agon/mos.h>`; the functions
are in acc's `libc.a`.

The names, the types, the arguments and the numbers the functions
answer with are libagon's, the library agondev's programs link with, so
that a program written against one builds against the other. Past what
libagon has, the rest of MOS 3.0.2's API is here too, named as MOS's
own documentation names it.

What is behind them is acc's own: each function is C over one routine
of acc's runtime that makes one MOS call (`rst.lil $08`, the call's
number in A) with the registers that call wants, and hands back the
register it answers in. The numbers below are those calls'.

A call marked "MOS 3" is not in MOS 2, and answers
`MOS_NOT_IMPLEMENTED` (23) there.

`stdio.h`'s files ([`fopen`](stdio.md#fopen) and the rest) are built
on these; a program that uses `stdio.h` need not call them.

## `FRESULT`, `FR_OK`, `FR_DISK_ERR`, `FR_INT_ERR`, `FR_NOT_READY`, `FR_NO_FILE`, `FR_NO_PATH`, `FR_INVALID_NAME`, `FR_DENIED`, `FR_EXIST`, `FR_INVALID_OBJECT`, `FR_WRITE_PROTECTED`, `FR_INVALID_DRIVE`, `FR_NOT_ENABLED`, `FR_NO_FILESYSTEM`, `FR_MKFS_ABORTED`, `FR_TIMEOUT`, `FR_LOCKED`, `FR_NOT_ENOUGH_CORE`, `FR_TOO_MANY_OPEN_FILES`, `FR_INVALID_PARAMETER`, `MOS_INVALID_COMMAND`, `MOS_NOT_IMPLEMENTED`

Most calls answer a status: 0 for success, or one of FatFS's codes, or
one of MOS's own two past them. `mos_getError` turns one into words.

| Code | Name                 | Code | Name                     |
|------|----------------------|------|--------------------------|
| 0    | `FR_OK`              | 11   | `FR_INVALID_DRIVE`       |
| 1    | `FR_DISK_ERR`        | 12   | `FR_NOT_ENABLED`         |
| 2    | `FR_INT_ERR`         | 13   | `FR_NO_FILESYSTEM`       |
| 3    | `FR_NOT_READY`       | 14   | `FR_MKFS_ABORTED`        |
| 4    | `FR_NO_FILE`         | 15   | `FR_TIMEOUT`             |
| 5    | `FR_NO_PATH`         | 16   | `FR_LOCKED`              |
| 6    | `FR_INVALID_NAME`    | 17   | `FR_NOT_ENOUGH_CORE`     |
| 7    | `FR_DENIED`          | 18   | `FR_TOO_MANY_OPEN_FILES` |
| 8    | `FR_EXIST`           | 19   | `FR_INVALID_PARAMETER`   |
| 9    | `FR_INVALID_OBJECT`  | 20   | `MOS_INVALID_COMMAND`    |
| 10   | `FR_WRITE_PROTECTED` | 23   | `MOS_NOT_IMPLEMENTED`    |

`FR_` codes are an enum, `FRESULT`. `MOS_INVALID_COMMAND` is what MOS
answers a call made from Z80 mode; `MOS_NOT_IMPLEMENTED` a call this
MOS does not have.

## `putch`, `getch`

    int  putch(int c);
    char getch(void);

`putch` writes the low byte of `c` to the console (`rst.lil $10`) and
answers `c`. `getch` answers a key from the keyboard: call 0x00,
`mos_getkey`.

## `mos_puts`, `mos_putstring`

    void mos_puts(const char *buffer, uint24_t size, char delimiter);
    void mos_putstring(const char *string);

Write a run of bytes to the console with `rst.lil $18`: `buffer` in
HL, `size` in BC, `delimiter` in A. `mos_putstring` writes a C string:
a size of 0 and a delimiter of 0, which is that restart's way of
stopping at the terminator.

## `mos_editline`

    uint8_t mos_editline(char *buffer, uint24_t bufferlength,
                         uint8_t clearbuffer);

A line from the keyboard into `buffer`, with MOS's editing: call 0x09,
`mos_editline`, `buffer` in HL, `bufferlength` in BC and
`clearbuffer`, whether to clear `buffer` first, in E. The answer is
the key that ended the line: 13 for Return, 27 for Escape.

## `waitvblank`

    void waitvblank(void);

Wait for the end of the frame: it watches the low byte of
`sysvar_time`, which MOS moves on twice a frame, until it changes. It
makes no MOS call past the first `mos_sysvars`.

## `mos_sysvars`, `sys_vars`, `SYSVAR`

    uint8_t *mos_sysvars(void);
    #define sys_vars (__acc_sys_vars())     /* volatile SYSVAR * */

`mos_sysvars` answers where MOS keeps its system variables: call 0x08,
`mos_sysvars`, which answers in IX. It is asked once and the address
kept, since it does not change while a program runs. Index it with
the `sysvar_` offsets.

`sys_vars` is libagon's name for the same address as a
`volatile SYSVAR *`, a structure laid out as MOS lays them out, with
one member to each variable (`sys_vars->scrCols`). libagon's startup sets
the pointer; acc's has nothing to set it with, so the name is a call
that asks MOS. `scrpixel` is a `uint24_t` and also three bytes
`scrpixelR`, `scrpixelB`, `scrpixelG`; `rtc` is a `SYSVAR_RTCDATA`.

## `sysvar_time`, `sysvar_vdp_pflags`, `sysvar_keyascii`, `sysvar_keymods`, `sysvar_cursorX`, `sysvar_cursorY`, `sysvar_scrchar`, `sysvar_scrpixel`, `sysvar_audioChannel`, `sysvar_audioSuccess`, `syscar_audioSuccess`, `sysvar_scrWidth`, `sysvar_scrHeight`, `sysvar_scrCols`, `sysvar_scrRows`, `sysvar_scrColours`, `sysvar_scrpixelIndex`, `sysvar_vkeycode`, `sysvar_vkeydown`, `sysvar_vkeycount`, `sysvar_rtc`, `sysvar_spare`, `sysvar_keydelay`, `sysvar_keyrate`, `sysvar_keyled`, `sysvar_scrMode`, `sysvar_rtcEnable`, `sysvar_mouseX`, `sysvar_mouseY`, `sysvar_mouseButtons`, `sysvar_mouseWheel`, `sysvar_mouseXDelta`, `sysvar_mouseYDelta`, `sysvar_gp`

Where each system variable is, in bytes from `mos_sysvars()`, and how
many bytes it has. Wider ones are low byte first.

| Offset | Name                   | Bytes | Holds                        |
|--------|------------------------|-------|------------------------------|
| 0x00   | `sysvar_time`          | 4     | centiseconds, twice a frame  |
| 0x04   | `sysvar_vdp_pflags`    | 1     | VDP answers arrived          |
| 0x05   | `sysvar_keyascii`      | 1     | the key held, or zero        |
| 0x06   | `sysvar_keymods`       | 1     | shift, control and the rest  |
| 0x07   | `sysvar_cursorX`       | 1     |                              |
| 0x08   | `sysvar_cursorY`       | 1     |                              |
| 0x09   | `sysvar_scrchar`       | 1     | what was read off the screen |
| 0x0a   | `sysvar_scrpixel`      | 3     | and its colour, R, B, G      |
| 0x0d   | `sysvar_audioChannel`  | 1     |                              |
| 0x0e   | `sysvar_audioSuccess`  | 1     | 1 if the note was queued     |
| 0x0f   | `sysvar_scrWidth`      | 2     | pixels                       |
| 0x11   | `sysvar_scrHeight`     | 2     | pixels                       |
| 0x13   | `sysvar_scrCols`       | 1     | characters                   |
| 0x14   | `sysvar_scrRows`       | 1     | characters                   |
| 0x15   | `sysvar_scrColours`    | 1     |                              |
| 0x16   | `sysvar_scrpixelIndex` | 1     | that pixel's palette index   |
| 0x17   | `sysvar_vkeycode`      | 1     | FabGL's virtual key code     |
| 0x18   | `sysvar_vkeydown`      | 1     | 1 for down, 0 for up         |
| 0x19   | `sysvar_vkeycount`     | 1     | one more at every key packet |
| 0x1a   | `sysvar_rtc`           | 6     | the clock, packed            |
| 0x20   | `sysvar_spare`         | 2     |                              |
| 0x22   | `sysvar_keydelay`      | 2     | keyboard repeat delay        |
| 0x24   | `sysvar_keyrate`       | 2     | and rate                     |
| 0x26   | `sysvar_keyled`        | 1     | keyboard LEDs                |
| 0x27   | `sysvar_scrMode`       | 1     | screen mode                  |
| 0x28   | `sysvar_rtcEnable`     | 1     | 1 to use the ESP32's clock   |
| 0x29   | `sysvar_mouseX`        | 2     |                              |
| 0x2b   | `sysvar_mouseY`        | 2     |                              |
| 0x2d   | `sysvar_mouseButtons`  | 1     |                              |
| 0x2e   | `sysvar_mouseWheel`    | 1     | wheel delta                  |
| 0x2f   | `sysvar_mouseXDelta`   | 2     |                              |
| 0x31   | `sysvar_mouseYDelta`   | 2     |                              |
| 0x37   | `sysvar_gp`            | 1     | the general poll's answer    |

`syscar_audioSuccess` is libagon's spelling of `sysvar_audioSuccess`.
`sysvar_gp` is MOS 3's.

## `getsysvar_time`, `getsysvar_vdp_pflags`, `getsysvar_keyascii`, `getsysvar_keymods`, `getsysvar_cursorX`, `getsysvar_cursorY`, `getsysvar_scrchar`, `getsysvar_scrpixel`, `getsysvar_audioChannel`, `getsysvar_audioSuccess`, `getsysvar_scrwidth`, `getsysvar_scrheight`, `getsysvar_scrCols`, `getsysvar_scrRows`, `getsysvar_scrColours`, `getsysvar_scrpixelIndex`, `getsysvar_vkeycode`, `getsysvar_vkeydown`, `getsysvar_vkeycount`, `getsysvar_keydelay`, `getsysvar_keyrate`, `getsysvar_keyled`, `getsysvar_scrMode`, `getsysvar_rtcEnable`, `getsysvar_mouseX`, `getsysvar_mouseY`, `getsysvar_mouseButtons`, `getsysvar_mouseWheel`, `getsysvar_mouseXDelta`, `getsysvar_mouseYDelta`, `getsysvar_gp`

    uint32_t getsysvar_time(void);
    uint8_t  getsysvar_keyascii(void);
    uint16_t getsysvar_scrwidth(void);
    int16_t  getsysvar_mouseXDelta(void);
    ...

One system variable each, read at the moment of the call through
`mos_sysvars()`: `getsysvar_` and the variable's name from the table
above (`sysvar_cursorX` is `getsysvar_cursorX`). The exceptions are
`getsysvar_scrwidth` and `getsysvar_scrheight`, in lower case.

Each answers an unsigned type of the variable's width (`uint8_t`,
`uint16_t`, `uint32_t` for the time, `uint24_t` for `scrpixel`), but
for three that are signed: `getsysvar_mouseWheel` answers an `int8_t`,
and `getsysvar_mouseXDelta` and `getsysvar_mouseYDelta` an `int16_t`.

`getsysvar_scrpixel` answers the three bytes R, B, G as one number,
R the lowest. There is none for `sysvar_spare`; the clock is
`getsysvar_rtc`.

    while (!getsysvar_keyascii())
        ;                           /* wait for a key to be held */

## `getsysvar_rtc`, `SYSVAR_RTCDATA`

    volatile SYSVAR_RTCDATA *getsysvar_rtc(void);

Where the six bytes at `sysvar_rtc` are, as libagon's
`SYSVAR_RTCDATA`: `year` (since 1980), `month` (0 to 11), `day` (1 to
31), `hour`, `minute`, `second`, a byte each.

That is not what the bytes hold: MOS has kept them packed since 1.04.
The type is here for programs that name it. To read the clock, use
`mos_unpackrtc`.

## `vdp_pflag_cursor`, `vdp_pflag_scrchar`, `vdp_pflag_point`, `vdp_pflag_audio`, `vdp_pflag_mode`, `vdp_pflag_rtc`, `vdp_pflag_mouse`

Which of the VDP's answers a bit of `sysvar_vdp_pflags` stands for:

| Bit    | Name                | Bit    | Name                |
|--------|---------------------|--------|-------------------|
| 0x01   | `vdp_pflag_cursor`  | 0x10   | `vdp_pflag_mode`    |
| 0x02   | `vdp_pflag_scrchar` | 0x20   | `vdp_pflag_rtc`     |
| 0x04   | `vdp_pflag_point`   | 0x40   | `vdp_pflag_mouse`   |
| 0x08   | `vdp_pflag_audio`   |        |                     |

`mos_clearvdpflags` and `mos_waitforvdpflags` take them.

## `mos_load`, `mos_save`

    uint8_t mos_load(const char *filename, char *address,
                     uint24_t maxsize);
    uint8_t mos_save(const char *filename, char *address,
                     uint24_t nbytes);

`mos_load` reads the file `filename` into memory at `address`, at
most `maxsize` bytes: call 0x01, `mos_load`. `mos_save` writes
`nbytes` from `address` to the file: call 0x02, `mos_save`. Both
answer a status.

## `mos_cd`, `mos_dir`, `mos_mkdir`

    uint8_t mos_cd(const char *path);
    uint8_t mos_dir(const char *path);
    uint8_t mos_mkdir(const char *path);

Calls 0x03 `mos_cd`, 0x04 `mos_dir` and 0x07 `mos_mkdir`, with `path`
in HL. Each answers a status.

## `mos_del`, `mos_ren`, `mos_copy`

    uint8_t mos_del(const char *filename);
    uint8_t mos_ren(const char *filename, const char *newname);
    uint8_t mos_copy(const char *source, const char *destination);

Delete, rename and copy a file: calls 0x05 `mos_del`, 0x06 `mos_ren`
and 0x11 `mos_copy`, the first name in HL and the second in DE. Each
answers a status.

## `mos_fopen`, `mos_fclose`, `FA_READ`, `FA_WRITE`, `FA_OPEN_EXISTING`, `FA_CREATE_NEW`, `FA_CREATE_ALWAYS`, `FA_OPEN_ALWAYS`, `FA_OPEN_APPEND`

    uint8_t mos_fopen(const char *filename, uint8_t mode);
    uint8_t mos_fclose(uint8_t fh);

`mos_fopen` opens a file and answers its handle, or 0 when it cannot:
call 0x0A, `mos_fopen`, the name in HL and `mode` in C. `mode` is
FatFS's, the same for `ffs_fopen`: one or both of `FA_READ` and
`FA_WRITE`, or'd with one of the rest.

| Name               | Value | Name               | Value |
|--------------------|-------|--------------------|-------|
| `FA_READ`          | 0x01  | `FA_CREATE_ALWAYS` | 0x08  |
| `FA_WRITE`         | 0x02  | `FA_OPEN_ALWAYS`   | 0x10  |
| `FA_OPEN_EXISTING` | 0x00  | `FA_OPEN_APPEND`   | 0x30  |
| `FA_CREATE_NEW`    | 0x04  |                    |       |

`mos_fclose` closes the handle `fh` (call 0x0B, `mos_fclose`, the
handle in C) and answers how many files are still open.

    uint8_t fh = mos_fopen("log.txt", FA_WRITE | FA_CREATE_ALWAYS);

    if (fh) {
        mos_fwrite(fh, "hello\r\n", 7);
        mos_fclose(fh);
    }

## `mos_fgetc`, `mos_fputc`, `mos_feof`

    char    mos_fgetc(uint8_t fh);
    void    mos_fputc(uint8_t fh, char c);
    uint8_t mos_feof(uint8_t fh);

A byte at a time: calls 0x0C `mos_fgetc` (answers the byte), 0x0D
`mos_fputc` (the handle in C, the byte in B) and 0x0E `mos_feof`
(answers whether `fh` is at the end of its file). The handle goes in
C.

## `mos_fread`, `mos_fwrite`

    uint24_t mos_fread(uint8_t fh, char *buffer, uint24_t numbytes);
    uint24_t mos_fwrite(uint8_t fh, char *buffer, uint24_t numbytes);

Read or write up to `numbytes` between the file and `buffer`: calls
0x1A `mos_fread` and 0x1B `mos_fwrite`, the handle in C, `buffer` in
HL and `numbytes` in DE. Each answers how many bytes it moved, which
MOS answers in DE.

## `mos_flseek`, `mos_flseek_p`

    uint8_t mos_flseek(uint8_t fh, uint32_t offset);
    uint8_t mos_flseek_p(uint8_t fh, uint32_t offset);

Move to `offset` bytes from the start of the file, and answer a
status. `mos_flseek` is call 0x1C: the offset's low three bytes in HL
and its top byte in E. `mos_flseek_p` is call 0x24, MOS 3, which takes
HL pointing at the four-byte offset; the function stores `offset` and
passes its address.

## `mos_getfil`

    FIL *mos_getfil(uint8_t fh);

The FatFS `FIL` behind the handle `fh`, for the `ffs_` calls: call
0x19, `mos_getfil`, which answers in HL. Its `fptr` is the place in
the file.

## `mos_getError`

    void mos_getError(uint8_t code, char *buffer,
                      uint24_t bufferlength);

The words for the status `code`, written to `buffer`: call 0x0F,
`mos_geterror`, the code in E, `buffer` in HL and `bufferlength` in BC.
Code 4 is `Could not find file`.

MOS writes no more than `bufferlength - 1` characters, and a message
cut short there gets no terminator: what was in `buffer` after them
stays. Clear the buffer first, or give it room. (libagon's
`mos_getError` restarts the machine; acc's makes the call.)

## `mos_oscli`

    uint8_t mos_oscli(char *command, char **argv, uint24_t argc);

Run a MOS command line, as typed at the prompt: call 0x10,
`mos_oscli`, `command` in HL, `argv` in DE, `argc` in BC. The answer
is a status. `mos_oscli("cd /", NULL, 0)` changes directory.

## `mos_getrtc`, `mos_setrtc`

    uint8_t mos_getrtc(char *buffer);
    void    mos_setrtc(uint8_t *timedata);

`mos_getrtc` writes the clock as a string into `buffer` and answers
the byte MOS leaves in A: call 0x12, `mos_getrtc`. `mos_setrtc` sets
the clock from the bytes at `timedata`: call 0x13, `mos_setrtc`. Both
pass the pointer in HL.

## `mos_unpackrtc`, `vdp_time_t`, `RTC_REFRESH_BEFORE`, `RTC_REFRESH_AFTER`

    void mos_unpackrtc(vdp_time_t *buffer, uint8_t flags);

The clock, unpacked into `*buffer`: call 0x23, `mos_unpackrtc`, MOS 3,
`buffer` in HL and `flags` in C.

| `flags`              | Value | Asks the VDP for the time       |
|----------------------|-------|---------------------------------|
| `RTC_REFRESH_BEFORE` | 0x01  | first                           |
| `RTC_REFRESH_AFTER`  | 0x02  | again afterwards                |

| `vdp_time_t` member | Type       | Holds                          |
|---------------------|------------|--------------------------------|
| `year`              | `uint16_t` |                                |
| `month`             | `uint8_t`  | 0 to 11                        |
| `day`               | `uint8_t`  | 1 to 31                        |
| `dayOfWeek`         | `uint8_t`  | 0 to 6, from Sunday            |
| `dayOfYear`         | `uint16_t` |                                |
| `hour`              | `uint8_t`  |                                |
| `minute`            | `uint8_t`  |                                |
| `second`            | `uint8_t`  |                                |

    vdp_time_t t;

    mos_unpackrtc(&t, RTC_REFRESH_BEFORE);
    printf("%02d:%02d\n", t.hour, t.minute);

## `mos_setintvector`

    void *mos_setintvector(uint8_t vector, void (*handler)(void));

Put `handler` on the interrupt `vector` and answer the handler that
was there: call 0x14, `mos_setintvector`, the vector in E and the
handler in HL. The handler is an interrupt routine, called as MOS
calls one, not a plain C function. Put the old one back with a second
call.

## `mos_setkbvector`, `mos_getkbmap`

    void     mos_setkbvector(void (*handler)(void),
                             uint8_t addresslength);
    uint8_t *mos_getkbmap(void);

`mos_setkbvector` has MOS call `handler` from its interrupt for each
keyboard packet, with the packet's address in DE: call 0x1D,
`mos_setkbvector`, `handler` in HL. A non-zero `addresslength` (in C)
says `handler` is a 16-bit address.

`mos_getkbmap` answers where MOS's keyboard map is: call 0x1E,
`mos_getkbmap`, which answers in IX.

## `mos_uopen`, `mos_uclose`, `UART`

    uint8_t mos_uopen(UART *settings);
    void    mos_uclose(void);

Open UART1 with `settings` (call 0x15, `mos_uopen`, `settings` in IX),
and close it (call 0x16, `mos_uclose`). `mos_uopen` answers the byte
MOS leaves in A.

| `UART` member | Type      | Holds                          |
|---------------|-----------|--------------------------------|
| `baudRate`    | `int24_t` |                                |
| `dataBits`    | `uint8_t` |                                |
| `stopBits`    | `uint8_t` |                                |
| `parity`      | `uint8_t` |                                |
| `flowcontrol` | `uint8_t` | 0 none, 1 hardware             |
| `eir`         | `uint8_t` | the interrupts to enable       |

## `mos_ugetc`, `mos_ugetc_nb`, `mos_uputc`

    int     mos_ugetc(void);
    int     mos_ugetc_nb(void);
    uint8_t mos_uputc(int c);

`mos_ugetc` answers the next byte from UART1, 0 to 255, waiting for
it: call 0x17, `mos_ugetc`. MOS says there was one with the carry;
without it, as when the port is not open, the answer is 256.

`mos_ugetc_nb` does not wait and makes no MOS call: it reads the
UART's own registers, answering the byte at port 0xD0 when bit 0 of
the line status register, port 0xD5, says one is waiting, and -1 when
none is.

`mos_uputc` sends the low byte of `c` (call 0x18, `mos_uputc`, in C),
and answers 1 when it went and 0 when the port is not open: MOS's
carry. libagon's answers the other way round.

## `mos_i2c_open`, `mos_i2c_close`, `I2C_SPEED_57600`, `I2C_SPEED_115200`, `I2C_SPEED_230400`

    void mos_i2c_open(uint8_t frequency);
    void mos_i2c_close(void);

Open the I2C bus at a speed (call 0x1F, `mos_i2c_open`, `frequency`
in C), and close it (call 0x20, `mos_i2c_close`).

| `frequency`        | Value |
|--------------------|-------|
| `I2C_SPEED_57600`  | 0x01  |
| `I2C_SPEED_115200` | 0x02  |
| `I2C_SPEED_230400` | 0x03  |

## `mos_i2c_write`, `mos_i2c_read`, `RET_OK`, `RET_NORESPONSE`, `RET_DATA_NACK`, `RET_ARB_LOST`, `RET_BUS_ERROR`

    uint8_t mos_i2c_write(uint8_t i2c_address, uint8_t size,
                          unsigned char *buffer);
    uint8_t mos_i2c_read(uint8_t i2c_address, uint8_t size,
                         unsigned char *buffer);

Write or read `size` bytes at `buffer` to or from the device at
`i2c_address`: calls 0x21 `mos_i2c_write` and 0x22 `mos_i2c_read`, the
address in C, the count in B and `buffer` in HL. Each answers:

| Name             | Value |
|------------------|-------|
| `RET_OK`         | 0x00  |
| `RET_NORESPONSE` | 0x01  |
| `RET_DATA_NACK`  | 0x02  |
| `RET_ARB_LOST`   | 0x04  |
| `RET_BUS_ERROR`  | 0x08  |

## `mos_pmatch`

    int8_t mos_pmatch(const char *pattern, const char *string,
                      uint8_t flags);

Match `string` against the wildcard `pattern`: call 0x28, `mos_pmatch`,
MOS 3, `pattern` in HL, `string` in DE and `flags` in C. The answer is
MOS's, in A, as a signed byte.

## `mos_getargument`

    void mos_getargument(char **arg, char **argend, const char *source,
                         uint24_t argnumber);

Find the argument numbered `argnumber` in the command line `source`:
call 0x29, `mos_getargument`, MOS 3, `source` in HL and `argnumber` in
BC. Its start goes to `*arg` and its end to `*argend`, from HL and DE;
either pointer may be `NULL`.

## `mos_extractstring`, `mos_extractnumber`

    uint8_t mos_extractstring(char **result, char **next,
                              const char *source, const char *dividers,
                              uint8_t flags);
    uint8_t mos_extractnumber(uint24_t *result, char **next,
                              const char *source, const char *dividers,
                              uint8_t flags);

Take the next string, or the next number, off the front of `source`,
the pieces separated by any of `dividers`: calls 0x2A
`mos_extractstring` and 0x2B `mos_extractnumber`, MOS 3, `source` in
HL, `dividers` in DE and `flags` in C. The answer is a status; the
string (a pointer) or the number goes to `*result`, from HL, and where
to go on from to `*next`, from DE. Either pointer may be `NULL`.

    char *word, *rest;

    mos_extractstring(&word, &rest, "alpha,beta", ",", 0);

## `mos_escapestring`

    uint8_t mos_escapestring(uint24_t *resultlength,
                             const char *source, char *buffer,
                             uint24_t bufferlength);

`source` escaped, into `buffer`: call
0x2C, `mos_escapestring`, MOS 3, `source` in HL, `buffer` in DE and
`bufferlength` in BC. The answer is a status; the length written goes
to `*resultlength`, from BC, unless that is `NULL`.

## `mos_setvarval`, `mos_readvarval`

    int mos_setvarval(char *name, void *value, char **actualName,
                      uint8_t *type);
    int mos_readvarval(char *namePattern, void *value,
                       char **actualName, int *length,
                       uint8_t *typeFlag);

MOS's own variables, as `*SET` and `*SHOW` have them. MOS 3.

`mos_setvarval` is call 0x30: `name` in HL, `value` in IX,
`*actualName` in IY (0 the first time) and `*type` in C. A string
type (0) takes `value` as a pointer to the string; a number (1) takes
the number itself in `value`. The type MOS used and the name it set
come back in `*type` and `*actualName`. A type of 255 with a `NULL`
value deletes the variable.

`mos_readvarval` is call 0x31: `namePattern` in HL, `value` (the
buffer) in IX, `*length` (its size) in DE, `*actualName` in IY (0 the
first time) and `*typeFlag` in C. The name found, the length and the
type come back through the same pointers. `actualName`, `length`,
`type` and `typeFlag` may be `NULL`, which passes 0.

Both answer a status.

    char *name = NULL;
    uint8_t type = 0;

    mos_setvarval("Greeting", "hello", &name, &type);

## `mos_gsinit`, `mos_gsread`, `mos_gstrans`

    uint8_t mos_gsinit(const char *source, char **transinfo_ptr,
                       uint8_t flags);
    uint8_t mos_gsread(char *char_read, char **transinfo_ptr);
    int     mos_gstrans(char *source, char *dest, int destLen,
                        int *read, uint8_t flags);

GSTrans, MOS's translation of a string. MOS 3.

`mos_gsinit` (call 0x32: `source` in HL, `transinfo_ptr` in DE,
`flags` in C) starts a translation; each `mos_gsread` (call 0x33,
`transinfo_ptr` in DE) answers a status and puts the next character,
from C, in `*char_read`, 0 at the end.

    char *t = NULL, c;

    mos_gsinit("x<Greeting>", &t, 0);
    while (mos_gsread(&c, &t) == 0 && c)
        putch(c);

`mos_gstrans` translates the whole of `source` into `dest`: call 0x34,
`source` in HL, `dest` in IX, `destLen` in DE, `flags` in C. It
answers a status, and puts what MOS answers in BC in `*read` unless
that is `NULL`.

## `mos_substituteargs`

    int mos_substituteargs(char *tpl, char *args, char *dest,
                           int length, uint8_t flags);

Fill the template `tpl` (with `%0`, `%1`, `%*1` and the like) from the
arguments `args` into `dest`: call 0x35, `mos_substituteargs`, MOS 3,
`tpl` in HL, `args` in IX, `length` in DE, `dest` in IY and `flags`
in C. The answer is not a status but the length, which MOS answers in
BC.

## `mos_resolvepath`, `mos_getdirforpath`, `mos_getabsolutepath`

    int mos_resolvepath(char *filepath, char *resolvedPath,
                        int *length, uint8_t *index, DIR *dir,
                        uint8_t flags);
    int mos_getdirforpath(char *srcPath, char *dir, int *length,
                          uint8_t index);
    int mos_getabsolutepath(char *path, char *resolved, int *length);

Paths, MOS 3. Each answers a status; `*length` goes in as the room in
the buffer, and `length` may be `NULL`, which passes 0.

`mos_resolvepath` (call 0x38) resolves `filepath`, which may hold
wildcards, into `resolvedPath`: `filepath` in HL, `resolvedPath` in
IX, `*length` in DE, `dir` in IY, `flags` in B and `*index` in C. The
index and the length come back through their pointers; `index` may be
`NULL`.

`mos_getdirforpath` (call 0x39) writes the directory of `srcPath` into
`dir`: `srcPath` in HL, `dir` in IX, `*length` in DE, `index` in C.
The length comes back through `length`.

`mos_getabsolutepath` (call 0x3C) writes `path` made absolute into
`resolved`: `path` in HL, `resolved` in IX, `*length` in DE. It does
not write the length back.

## `mos_getleafname`, `mos_isdirectory`

    char   *mos_getleafname(const char *pathname);
    uint8_t mos_isdirectory(const char *pathname);

`mos_getleafname` answers where the last part of `pathname` starts
(`leaf.txt` in `a/b/leaf.txt`): call 0x3A, `mos_getleafname`, MOS 3,
answering in HL. `mos_isdirectory` answers MOS's status for whether
`pathname` is a directory: call 0x3B, `mos_isdirectory`, MOS 3. Both
pass `pathname` in HL.

## `mos_clearvdpflags`, `mos_waitforvdpflags`

    uint8_t mos_clearvdpflags(uint8_t bitmask);
    uint8_t mos_waitforvdpflags(uint8_t bitmask);

Clear the `vdp_pflag_` bits in `bitmask` from `sysvar_vdp_pflags`
(call 0x40, `mos_clearvdpflags`), or wait for them to be set (call
0x41, `mos_waitforvdpflags`). MOS 3. `bitmask` goes in C; each answers
the byte MOS leaves in A.

## `mos_getfunction`, `MOS_FUNC_SD_INIT`, `MOS_FUNC_SD_READBLOCKS`, `MOS_FUNC_SD_WRITEBLOCKS`, `MOS_FUNC_F_PRINTF`, `MOS_FUNC_F_FINDFIRST`, `MOS_FUNC_F_FINDNEXT`, `MOS_FUNC_OPEN_UART1`, `MOS_FUNC_SETVARVAL`, `MOS_FUNC_READVARVAL`, `MOS_FUNC_GSTRANS`, `MOS_FUNC_SUBSTITUTEARGS`, `MOS_FUNC_RESOLVEPATH`, `MOS_FUNC_GETDIRECTORYFORPATH`, `MOS_FUNC_RESOLVERELATIVEPATH`, `MOS_FUNC_GETSYSVARS`, `MOS_FUNC_GETKBMAP`

    void *mos_getfunction(uint8_t number, uint8_t flags);

The address of one of MOS's own C functions, or 0 for a number MOS has
none for: call 0x50, `mos_getfunction`, MOS 3, `number` in B and
`flags` in C, answering in HL. The functions take their arguments as
acc passes them, so a program calls one through a pointer of the
right type.

| `number`                       | Value | `number`               | Value |
|--------------------------------|-------|------------------------|-------|
| `MOS_FUNC_SD_INIT`             | 0x00  | `MOS_FUNC_SETVARVAL`   | 0x09  |
| `MOS_FUNC_SD_READBLOCKS`       | 0x01  | `MOS_FUNC_READVARVAL`  | 0x0a  |
| `MOS_FUNC_SD_WRITEBLOCKS`      | 0x02  | `MOS_FUNC_GSTRANS`     | 0x0b  |
| `MOS_FUNC_F_PRINTF`            | 0x05  | `MOS_FUNC_GETSYSVARS`  | 0x10  |
| `MOS_FUNC_F_FINDFIRST`         | 0x06  | `MOS_FUNC_GETKBMAP`    | 0x11  |
| `MOS_FUNC_F_FINDNEXT`          | 0x07  |                        |       |
| `MOS_FUNC_OPEN_UART1`          | 0x08  |                        |       |
| `MOS_FUNC_SUBSTITUTEARGS`      | 0x0c  |                        |       |
| `MOS_FUNC_RESOLVEPATH`         | 0x0d  |                        |       |
| `MOS_FUNC_GETDIRECTORYFORPATH` | 0x0e  |                        |       |
| `MOS_FUNC_RESOLVERELATIVEPATH` | 0x0f  |                        |       |

    uint8_t *(*sysvars)(void) = (uint8_t *(*)(void))
        mos_getfunction(MOS_FUNC_GETSYSVARS, 0);

## `sd_getunlockcode`, `sd_init`

    uint24_t sd_getunlockcode(void);
    uint8_t  sd_init(uint24_t unlockcode);

The SD card a block at a time sits behind a code that has to be asked
for first. MOS 3. `sd_getunlockcode` answers it: call 0x70,
`sd_getunlockcode`, which writes it where HL points. MOS makes one
code and keeps it. `sd_init` (call 0x71, `sd_init`, HL pointing at
the code) starts the card and answers a status.

## `sd_readblocks`, `sd_writeblocks`

    uint8_t sd_readblocks(uint32_t sector, uint8_t *buf,
                          uint24_t count);
    uint8_t sd_writeblocks(uint32_t sector, uint8_t *buf,
                           uint24_t count);

Read or write `count` blocks from block `sector`, into or from `buf`:
calls 0x72 `sd_readblocks` and 0x73 `sd_writeblocks`, MOS 3. HL points
at the block number and, four bytes on, the unlock code, which the
function asks for itself; `buf` is in DE and `count` in BC. Each
answers a status.

That is the layout MOS documents. MOS 3.0.2 does not check it: its
read compares the code with the first three bytes of the block number,
so it reads only a block whose number's low 24 bits are the code, and
its write calls the read. These do what MOS documents, not what 3.0.2
does.

## `FIL`, `DIR`, `FILINFO`, `FFOBJID`, `FATFS`

FatFS's structures, laid out as MOS lays them out: they are passed to
MOS and filled in by it. A program gives each `ffs_` call room for one
(static, as they are large) and reads its members.

| Type      | What it is                                         |
|-----------|----------------------------------------------------|
| `FIL`     | an open file; `fptr` is the place in it            |
| `DIR`     | an open directory                                  |
| `FILINFO` | a directory entry, from `ffs_dread` or `ffs_stat`  |
| `FFOBJID` | the object inside a `FIL` or a `DIR` (`obj`)       |
| `FATFS`   | a volume, for `ffs_mount`                          |

| `FILINFO` member | Type        | Holds                           |
|------------------|-------------|---------------------------------|
| `fsize`          | `uint32_t`  | the size in bytes               |
| `fdate`, `ftime` | `uint16_t`  | when it was written             |
| `fattrib`        | `uint8_t`   | the `AM_` bits                  |
| `altname`        | `char[13]`  | the short name                  |
| `fname`          | `char[256]` | the name                        |

## `AM_RDO`, `AM_HID`, `AM_SYS`, `AM_DIR`, `AM_ARC`

What a directory entry says about itself, in `FILINFO`'s `fattrib`:

| Name     | Value | Name     | Value |
|----------|-------|----------|-------|
| `AM_RDO` | 0x01  | `AM_DIR` | 0x10  |
| `AM_HID` | 0x02  | `AM_ARC` | 0x20  |
| `AM_SYS` | 0x04  |          |       |

## `ffs_fopen`, `ffs_fclose`

    uint8_t ffs_fopen(FIL *fh, const char *filename, uint8_t mode);
    uint8_t ffs_fclose(FIL *fh);

Open `filename` into the `FIL` at `fh`, with `mode` from the `FA_`
flags (call 0x80, `ffs_fopen`: `fh` in HL, `filename` in DE, `mode`
in C), and close it (call 0x81, `ffs_fclose`). Each answers an
`FRESULT`. Every `ffs_` function passes the `FIL` or `DIR` in HL.

    static FIL f;

    if (ffs_fopen(&f, "out.txt", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
        ffs_fputs(&f, "hello\r\n");
        ffs_fclose(&f);
    }

## `ffs_fread`, `ffs_fwrite`

    uint24_t ffs_fread(FIL *fh, char *buffer, uint24_t numbytes);
    uint24_t ffs_fwrite(FIL *fh, const char *buffer,
                        uint24_t numbytes);

Read or write up to `numbytes`: calls 0x82 `ffs_fread` and 0x83
`ffs_fwrite`, `buffer` in DE and `numbytes` in BC. Each answers how
many bytes it moved, which MOS answers in BC.

## `ffs_flseek`, `ffs_flseek_p`, `ffs_ftell`

    uint8_t ffs_flseek(FIL *fh, uint32_t offset);
    uint8_t ffs_flseek_p(FIL *fh, uint32_t *offset);
    uint8_t ffs_ftell(FIL *fh, uint32_t *result);

`ffs_flseek` moves to `offset` from the start: call 0x84, the
offset's low three bytes in DE and its top byte in C. `ffs_flseek_p`
(call 0xA6, MOS 3) takes the offset where `offset` points, in DE.
`ffs_ftell` (call 0x8D, MOS 3) stores the place at `*result`, in DE.
Each answers an `FRESULT`.

## `ffs_fgets`, `ffs_fputc`, `ffs_fputs`

    uint8_t *ffs_fgets(FIL *fh, char *buffer, uint24_t buffersize);
    uint24_t ffs_fputc(FIL *fh, char c);
    uint24_t ffs_fputs(FIL *fh, const char *string);

MOS 3. `ffs_fgets` reads a line into `buffer` (call 0x89: `buffer` in
DE, `buffersize` in BC) and answers `buffer`, or `NULL`, from DE.
`ffs_fputc` writes `c` (call 0x8A, `c` in C) and `ffs_fputs` writes
`string` (call 0x8B, `string` in DE); both answer what MOS answers in
BC, the count written.

## `ffs_fprintf`

    int ffs_fprintf(FIL *fp, const char *str, ...);

`fprintf` into a `FIL`. MOS 3's call 0x8C is not made: acc formats with
its own `printf`, which knows every conversion (see
[`printf`](stdio.md#printf-fprintf-sprintf-snprintf)), and writes
the text 64 bytes at a time with `ffs_fwrite`. The answer is how many
characters were formatted; a failed write is not seen. MOS's own `f_printf` is
`mos_getfunction(MOS_FUNC_F_PRINTF, 0)`.

## `ffs_ftruncate`, `ffs_fsync`, `ffs_feof`, `ffs_fsize`, `ffs_ferror`

    uint8_t ffs_ftruncate(FIL *fh);
    uint8_t ffs_fsync(FIL *fh);
    uint8_t ffs_feof(FIL *fh);
    uint8_t ffs_fsize(FIL *fh, uint32_t *result);
    uint8_t ffs_ferror(FIL *fh);

| Function        | Call | Does                                      |
|-----------------|------|-------------------------------------------|
| `ffs_ftruncate` | 0x85 | cuts the file off at the place            |
| `ffs_fsync`     | 0x86 | writes what is held; MOS 3                |
| `ffs_feof`      | 0x8E | whether the place is at the end; MOS 3    |
| `ffs_fsize`     | 0x8F | the size at `*result` (in DE); MOS 3      |
| `ffs_ferror`    | 0x90 | whether the file has had an error; MOS 3  |

`ffs_ftruncate`, `ffs_fsync` and `ffs_fsize` answer an `FRESULT`.
(libagon's `ffs_ftruncate` stops the machine with MOS's RST 38 panic;
acc's makes the call.)

## `ffs_dopen`, `ffs_dclose`, `ffs_dread`

    uint8_t ffs_dopen(DIR *dir_handle, const char *dir_path);
    uint8_t ffs_dclose(DIR *dir_handle);
    uint8_t ffs_dread(DIR *dir_handle, FILINFO *fil_handle);

Open the directory `dir_path` into the `DIR` (call 0x91, `ffs_dopen`,
`dir_path` in DE), read its entries one at a time into the `FILINFO`
(call 0x93, `ffs_dread`, `fil_handle` in DE), and close it (call 0x92,
`ffs_dclose`). Each answers an `FRESULT`. An entry with an empty
`fname` is the end of the directory.

    static DIR d;
    static FILINFO fi;

    if (ffs_dopen(&d, "/") == FR_OK) {
        while (ffs_dread(&d, &fi) == FR_OK && fi.fname[0])
            printf("%s %ld\n", fi.fname, (long) fi.fsize);
        ffs_dclose(&d);
    }

## `ffs_dfindfirst`, `ffs_dfindnext`

    uint8_t ffs_dfindfirst(DIR *dir_handle, FILINFO *fil_handle,
                           const char *dirpath, const char *pattern);
    uint8_t ffs_dfindnext(DIR *dir_handle, FILINFO *fil_handle);

The entries of `dirpath` that match `pattern` (as `"*.txt"`): call
0x94, `ffs_dfindfirst`, MOS 3, `fil_handle` in DE, `dirpath` in BC
and `pattern` in IX, opens the directory and reads the first; call
0x95, `ffs_dfindnext`, reads the next. Each answers an `FRESULT`;
close with `ffs_dclose`.

## `ffs_stat`

    uint8_t ffs_stat(FILINFO *fil_handle, const char *filename);

Fill the `FILINFO` with what the directory says of `filename`: call
0x96, `ffs_stat`, `fil_handle` in HL and `filename` in DE. The answer
is an `FRESULT`.

## `ffs_unlink`, `ffs_rename`, `ffs_mkdir`, `ffs_chdir`, `ffs_getcwd`

    uint8_t ffs_unlink(const char *filepath);
    uint8_t ffs_rename(const char *sourcefilepath,
                       const char *destfilepath);
    uint8_t ffs_mkdir(const char *dirname);
    uint8_t ffs_chdir(const char *dirname);
    uint8_t ffs_getcwd(char *dirpath, uint24_t bufferlength);

| Function     | Call | Does                                         |
|--------------|------|----------------------------------------------|
| `ffs_unlink` | 0x97 | removes a file or a directory; MOS 3         |
| `ffs_rename` | 0x98 | renames, or moves; MOS 3                     |
| `ffs_mkdir`  | 0x9B | makes a directory; MOS 3                     |
| `ffs_chdir`  | 0x9C | changes the current directory; MOS 3         |
| `ffs_getcwd` | 0x9E | the current directory into `dirpath`         |

The first path is in HL, `destfilepath` in DE, `bufferlength` in BC.
Each answers an `FRESULT`.

## `ffs_mount`, `ffs_getfree`, `ffs_getlabel`, `ffs_setlabel`

    uint8_t ffs_mount(FATFS *fs, const char *volpath,
                      uint8_t options);
    uint8_t ffs_getfree(char *path, uint32_t *freeclusters,
                        uint32_t *clustersize);
    uint8_t ffs_getlabel(char *path, char *label,
                         uint32_t *volserial);
    uint8_t ffs_setlabel(const char *volumelabel);

The volume, MOS 3. Each answers an `FRESULT`; the arguments go in HL,
DE and BC in order.

| Function       | Call | Does                                       |
|----------------|------|--------------------------------------------|
| `ffs_mount`    | 0x9F | mounts the volume into `*fs`               |
| `ffs_getfree`  | 0xA2 | free clusters and cluster size             |
| `ffs_getlabel` | 0xA3 | the label into `label`, and the serial     |
| `ffs_setlabel` | 0xA4 | sets the label                             |

`path` may be `NULL` for the one volume MOS has.

## `ffs_fforward`, `ffs_expand`, `ffs_chmod`, `ffs_utime`, `ffs_chdrive`, `ffs_mkfs`, `ffs_fdisk`, `ffs_setcp`

    uint8_t ffs_fforward(FIL *fh,
                         uint24_t (*func)(const uint8_t *, uint24_t),
                         uint24_t btf, uint24_t *bf);
    uint8_t ffs_expand(FIL *fh, uint32_t fsz, uint8_t opt);
    uint8_t ffs_chmod(const char *path, uint8_t attr, uint8_t mask);
    uint8_t ffs_utime(const char *path, const FILINFO *fno);
    uint8_t ffs_chdrive(const char *path);
    uint8_t ffs_mkfs(const char *path, const void *opt, void *work,
                     uint24_t len);
    uint8_t ffs_fdisk(uint8_t pdrv, const uint32_t *ptbl, void *work);
    uint8_t ffs_setcp(uint16_t cp);

The FatFS calls MOS 3.0.2 numbers and has no code behind: each
answers `MOS_NOT_IMPLEMENTED` (23). They are here so that a program
can find that out rather than fail to link, and they make the call
all the same, so that a MOS that one day has them answers for itself.

| Function       | Call | Function      | Call |
|----------------|------|---------------|------|
| `ffs_fforward` | 0x87 | `ffs_chdrive` | 0x9D |
| `ffs_expand`   | 0x88 | `ffs_mkfs`    | 0xA0 |
| `ffs_chmod`    | 0x99 | `ffs_fdisk`   | 0xA1 |
| `ffs_utime`    | 0x9A | `ffs_setcp`   | 0xA5 |

The arguments are FatFS's. Not all of them reach MOS: `ffs_fforward`
passes only `fh`, `ffs_expand` only `fh`, `ffs_mkfs` only `path` and
`opt`, `ffs_fdisk` only `ptbl` and `pdrv`.

## `FFCONF_DEF`, `FF_FS_READONLY`, `FF_FS_MINIMIZE`, `FF_USE_FIND`, `FF_USE_MKFS`, `FF_USE_FASTSEEK`, `FF_USE_EXPAND`, `FF_USE_CHMOD`, `FF_USE_LABEL`, `FF_USE_FORWARD`, `FF_USE_STRFUNC`, `FF_CODE_PAGE`, `FF_USE_LFN`, `FF_MAX_LFN`, `FF_LFN_UNICODE`, `FF_LFN_BUF`, `FF_SFN_BUF`, `FF_FS_RPATH`, `FF_VOLUMES`, `FF_MULTI_PARTITION`, `FF_MIN_SS`, `FF_MAX_SS`, `FF_LBA64`, `FF_FS_TINY`, `FF_FS_EXFAT`, `FF_FS_NORTC`, `FF_FS_NOFSINFO`, `FF_FS_LOCK`, `FF_FS_REENTRANT`

How MOS's FatFS was configured, which says what its structures hold.

| Name                 | Value | Name              | Value |
|----------------------|-------|-------------------|-------|
| `FFCONF_DEF`         | 86631 | `FF_LFN_BUF`      | 255   |
| `FF_FS_READONLY`     | 0     | `FF_SFN_BUF`      | 12    |
| `FF_FS_MINIMIZE`     | 0     | `FF_FS_RPATH`     | 2     |
| `FF_USE_FIND`        | 1     | `FF_VOLUMES`      | 1     |
| `FF_USE_MKFS`        | 0     | `FF_MIN_SS`       | 512   |
| `FF_USE_FASTSEEK`    | 0     | `FF_MAX_SS`       | 512   |
| `FF_USE_EXPAND`      | 0     | `FF_LBA64`        | 0     |
| `FF_USE_CHMOD`       | 0     | `FF_FS_TINY`      | 1     |
| `FF_USE_LABEL`       | 1     | `FF_FS_EXFAT`     | 0     |
| `FF_USE_FORWARD`     | 0     | `FF_FS_NORTC`     | 0     |
| `FF_USE_STRFUNC`     | 1     | `FF_FS_NOFSINFO`  | 0     |
| `FF_CODE_PAGE`       | 437   | `FF_FS_LOCK`      | 0     |
| `FF_USE_LFN`         | 2     | `FF_FS_REENTRANT` | 0     |
| `FF_MAX_LFN`         | 255   | `FF_LFN_UNICODE`  | 0     |
| `FF_MULTI_PARTITION` | 0     |                   |       |
