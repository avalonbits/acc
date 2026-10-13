# The Agon

MOS, the VDP and its screen, graphics, bitmaps, buffers and sound, the keyboard, joysticks, GPIO, timers, and the eZ80F92's registers.

## `agon/mos.h`

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

`stdio.h`'s files ([`fopen`](2-io.md#fopen) and the rest) are built
on these; a program that uses `stdio.h` need not call them.

### `FRESULT`, `FR_OK`, `FR_DISK_ERR`, `FR_INT_ERR`, `FR_NOT_READY`, `FR_NO_FILE`, `FR_NO_PATH`, `FR_INVALID_NAME`, `FR_DENIED`, `FR_EXIST`, `FR_INVALID_OBJECT`, `FR_WRITE_PROTECTED`, `FR_INVALID_DRIVE`, `FR_NOT_ENABLED`, `FR_NO_FILESYSTEM`, `FR_MKFS_ABORTED`, `FR_TIMEOUT`, `FR_LOCKED`, `FR_NOT_ENOUGH_CORE`, `FR_TOO_MANY_OPEN_FILES`, `FR_INVALID_PARAMETER`, `MOS_INVALID_COMMAND`, `MOS_NOT_IMPLEMENTED`

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

### `putch`, `getch`

    int  putch(int c);
    char getch(void);

`putch` writes the low byte of `c` to the console (`rst.lil $10`) and
answers `c`. `getch` answers a key from the keyboard: call 0x00,
`mos_getkey`.

### `mos_puts`, `mos_putstring`

    void mos_puts(const char *buffer, uint24_t size, char delimiter);
    void mos_putstring(const char *string);

Write a run of bytes to the console with `rst.lil $18`: `buffer` in
HL, `size` in BC, `delimiter` in A. `mos_putstring` writes a C string:
a size of 0 and a delimiter of 0, which is that restart's way of
stopping at the terminator.

### `mos_editline`

    uint8_t mos_editline(char *buffer, uint24_t bufferlength,
                         uint8_t clearbuffer);

A line from the keyboard into `buffer`, with MOS's editing: call 0x09,
`mos_editline`, `buffer` in HL, `bufferlength` in BC and
`clearbuffer`, whether to clear `buffer` first, in E. The answer is
the key that ended the line: 13 for Return, 27 for Escape.

### `waitvblank`

    void waitvblank(void);

Wait for the end of the frame: it watches the low byte of
`sysvar_time`, which MOS moves on twice a frame, until it changes. It
makes no MOS call past the first `mos_sysvars`.

### `mos_sysvars`, `sys_vars`, `SYSVAR`

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

### `sysvar_time`, `sysvar_vdp_pflags`, `sysvar_keyascii`, `sysvar_keymods`, `sysvar_cursorX`, `sysvar_cursorY`, `sysvar_scrchar`, `sysvar_scrpixel`, `sysvar_audioChannel`, `sysvar_audioSuccess`, `syscar_audioSuccess`, `sysvar_scrWidth`, `sysvar_scrHeight`, `sysvar_scrCols`, `sysvar_scrRows`, `sysvar_scrColours`, `sysvar_scrpixelIndex`, `sysvar_vkeycode`, `sysvar_vkeydown`, `sysvar_vkeycount`, `sysvar_rtc`, `sysvar_spare`, `sysvar_keydelay`, `sysvar_keyrate`, `sysvar_keyled`, `sysvar_scrMode`, `sysvar_rtcEnable`, `sysvar_mouseX`, `sysvar_mouseY`, `sysvar_mouseButtons`, `sysvar_mouseWheel`, `sysvar_mouseXDelta`, `sysvar_mouseYDelta`, `sysvar_gp`

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

### `getsysvar_time`, `getsysvar_vdp_pflags`, `getsysvar_keyascii`, `getsysvar_keymods`, `getsysvar_cursorX`, `getsysvar_cursorY`, `getsysvar_scrchar`, `getsysvar_scrpixel`, `getsysvar_audioChannel`, `getsysvar_audioSuccess`, `getsysvar_scrwidth`, `getsysvar_scrheight`, `getsysvar_scrCols`, `getsysvar_scrRows`, `getsysvar_scrColours`, `getsysvar_scrpixelIndex`, `getsysvar_vkeycode`, `getsysvar_vkeydown`, `getsysvar_vkeycount`, `getsysvar_keydelay`, `getsysvar_keyrate`, `getsysvar_keyled`, `getsysvar_scrMode`, `getsysvar_rtcEnable`, `getsysvar_mouseX`, `getsysvar_mouseY`, `getsysvar_mouseButtons`, `getsysvar_mouseWheel`, `getsysvar_mouseXDelta`, `getsysvar_mouseYDelta`, `getsysvar_gp`

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

### `getsysvar_rtc`, `SYSVAR_RTCDATA`

    volatile SYSVAR_RTCDATA *getsysvar_rtc(void);

Where the six bytes at `sysvar_rtc` are, as libagon's
`SYSVAR_RTCDATA`: `year` (since 1980), `month` (0 to 11), `day` (1 to
31), `hour`, `minute`, `second`, a byte each.

That is not what the bytes hold: MOS has kept them packed since 1.04.
The type is here for programs that name it. To read the clock, use
`mos_unpackrtc`.

### `vdp_pflag_cursor`, `vdp_pflag_scrchar`, `vdp_pflag_point`, `vdp_pflag_audio`, `vdp_pflag_mode`, `vdp_pflag_rtc`, `vdp_pflag_mouse`

Which of the VDP's answers a bit of `sysvar_vdp_pflags` stands for:

| Bit    | Name                | Bit    | Name                |
|--------|---------------------|--------|-------------------|
| 0x01   | `vdp_pflag_cursor`  | 0x10   | `vdp_pflag_mode`    |
| 0x02   | `vdp_pflag_scrchar` | 0x20   | `vdp_pflag_rtc`     |
| 0x04   | `vdp_pflag_point`   | 0x40   | `vdp_pflag_mouse`   |
| 0x08   | `vdp_pflag_audio`   |        |                     |

`mos_clearvdpflags` and `mos_waitforvdpflags` take them.

### `mos_load`, `mos_save`

    uint8_t mos_load(const char *filename, char *address,
                     uint24_t maxsize);
    uint8_t mos_save(const char *filename, char *address,
                     uint24_t nbytes);

`mos_load` reads the file `filename` into memory at `address`, at
most `maxsize` bytes: call 0x01, `mos_load`. `mos_save` writes
`nbytes` from `address` to the file: call 0x02, `mos_save`. Both
answer a status.

### `mos_cd`, `mos_dir`, `mos_mkdir`

    uint8_t mos_cd(const char *path);
    uint8_t mos_dir(const char *path);
    uint8_t mos_mkdir(const char *path);

Calls 0x03 `mos_cd`, 0x04 `mos_dir` and 0x07 `mos_mkdir`, with `path`
in HL. Each answers a status.

### `mos_del`, `mos_ren`, `mos_copy`

    uint8_t mos_del(const char *filename);
    uint8_t mos_ren(const char *filename, const char *newname);
    uint8_t mos_copy(const char *source, const char *destination);

Delete, rename and copy a file: calls 0x05 `mos_del`, 0x06 `mos_ren`
and 0x11 `mos_copy`, the first name in HL and the second in DE. Each
answers a status.

### `mos_fopen`, `mos_fclose`, `FA_READ`, `FA_WRITE`, `FA_OPEN_EXISTING`, `FA_CREATE_NEW`, `FA_CREATE_ALWAYS`, `FA_OPEN_ALWAYS`, `FA_OPEN_APPEND`

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

### `mos_fgetc`, `mos_fputc`, `mos_feof`

    char    mos_fgetc(uint8_t fh);
    void    mos_fputc(uint8_t fh, char c);
    uint8_t mos_feof(uint8_t fh);

A byte at a time: calls 0x0C `mos_fgetc` (answers the byte), 0x0D
`mos_fputc` (the handle in C, the byte in B) and 0x0E `mos_feof`
(answers whether `fh` is at the end of its file). The handle goes in
C.

### `mos_fread`, `mos_fwrite`

    uint24_t mos_fread(uint8_t fh, char *buffer, uint24_t numbytes);
    uint24_t mos_fwrite(uint8_t fh, char *buffer, uint24_t numbytes);

Read or write up to `numbytes` between the file and `buffer`: calls
0x1A `mos_fread` and 0x1B `mos_fwrite`, the handle in C, `buffer` in
HL and `numbytes` in DE. Each answers how many bytes it moved, which
MOS answers in DE.

### `mos_flseek`, `mos_flseek_p`

    uint8_t mos_flseek(uint8_t fh, uint32_t offset);
    uint8_t mos_flseek_p(uint8_t fh, uint32_t offset);

Move to `offset` bytes from the start of the file, and answer a
status. `mos_flseek` is call 0x1C: the offset's low three bytes in HL
and its top byte in E. `mos_flseek_p` is call 0x24, MOS 3, which takes
HL pointing at the four-byte offset; the function stores `offset` and
passes its address.

### `mos_getfil`

    FIL *mos_getfil(uint8_t fh);

The FatFS `FIL` behind the handle `fh`, for the `ffs_` calls: call
0x19, `mos_getfil`, which answers in HL. Its `fptr` is the place in
the file.

### `mos_getError`

    void mos_getError(uint8_t code, char *buffer,
                      uint24_t bufferlength);

The words for the status `code`, written to `buffer`: call 0x0F,
`mos_geterror`, the code in E, `buffer` in HL and `bufferlength` in BC.
Code 4 is `Could not find file`.

MOS writes no more than `bufferlength - 1` characters, and a message
cut short there gets no terminator: what was in `buffer` after them
stays. Clear the buffer first, or give it room. (libagon's
`mos_getError` restarts the machine; acc's makes the call.)

### `mos_oscli`

    uint8_t mos_oscli(char *command, char **argv, uint24_t argc);

Run a MOS command line, as typed at the prompt: call 0x10,
`mos_oscli`, `command` in HL, `argv` in DE, `argc` in BC. The answer
is a status. `mos_oscli("cd /", NULL, 0)` changes directory.

### `mos_getrtc`, `mos_setrtc`

    uint8_t mos_getrtc(char *buffer);
    void    mos_setrtc(uint8_t *timedata);

`mos_getrtc` writes the clock as a string into `buffer` and answers
the byte MOS leaves in A: call 0x12, `mos_getrtc`. `mos_setrtc` sets
the clock from the bytes at `timedata`: call 0x13, `mos_setrtc`. Both
pass the pointer in HL.

### `mos_unpackrtc`, `vdp_time_t`, `RTC_REFRESH_BEFORE`, `RTC_REFRESH_AFTER`

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

### `mos_setintvector`

    void *mos_setintvector(uint8_t vector, void (*handler)(void));

Put `handler` on the interrupt `vector` and answer the handler that
was there: call 0x14, `mos_setintvector`, the vector in E and the
handler in HL. The handler is an interrupt routine, called as MOS
calls one, not a plain C function. Put the old one back with a second
call.

### `mos_setkbvector`, `mos_getkbmap`

    void     mos_setkbvector(void (*handler)(void),
                             uint8_t addresslength);
    uint8_t *mos_getkbmap(void);

`mos_setkbvector` has MOS call `handler` from its interrupt for each
keyboard packet, with the packet's address in DE: call 0x1D,
`mos_setkbvector`, `handler` in HL. A non-zero `addresslength` (in C)
says `handler` is a 16-bit address.

`mos_getkbmap` answers where MOS's keyboard map is: call 0x1E,
`mos_getkbmap`, which answers in IX.

### `mos_uopen`, `mos_uclose`, `UART`

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

### `mos_ugetc`, `mos_ugetc_nb`, `mos_uputc`

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

### `mos_i2c_open`, `mos_i2c_close`, `I2C_SPEED_57600`, `I2C_SPEED_115200`, `I2C_SPEED_230400`

    void mos_i2c_open(uint8_t frequency);
    void mos_i2c_close(void);

Open the I2C bus at a speed (call 0x1F, `mos_i2c_open`, `frequency`
in C), and close it (call 0x20, `mos_i2c_close`).

| `frequency`        | Value |
|--------------------|-------|
| `I2C_SPEED_57600`  | 0x01  |
| `I2C_SPEED_115200` | 0x02  |
| `I2C_SPEED_230400` | 0x03  |

### `mos_i2c_write`, `mos_i2c_read`, `RET_OK`, `RET_NORESPONSE`, `RET_DATA_NACK`, `RET_ARB_LOST`, `RET_BUS_ERROR`

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

### `mos_pmatch`

    int8_t mos_pmatch(const char *pattern, const char *string,
                      uint8_t flags);

Match `string` against the wildcard `pattern`: call 0x28, `mos_pmatch`,
MOS 3, `pattern` in HL, `string` in DE and `flags` in C. The answer is
MOS's, in A, as a signed byte.

### `mos_getargument`

    void mos_getargument(char **arg, char **argend, const char *source,
                         uint24_t argnumber);

Find the argument numbered `argnumber` in the command line `source`:
call 0x29, `mos_getargument`, MOS 3, `source` in HL and `argnumber` in
BC. Its start goes to `*arg` and its end to `*argend`, from HL and DE;
either pointer may be `NULL`.

### `mos_extractstring`, `mos_extractnumber`

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

### `mos_escapestring`

    uint8_t mos_escapestring(uint24_t *resultlength,
                             const char *source, char *buffer,
                             uint24_t bufferlength);

`source` escaped, into `buffer`: call
0x2C, `mos_escapestring`, MOS 3, `source` in HL, `buffer` in DE and
`bufferlength` in BC. The answer is a status; the length written goes
to `*resultlength`, from BC, unless that is `NULL`.

### `mos_setvarval`, `mos_readvarval`

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

### `mos_gsinit`, `mos_gsread`, `mos_gstrans`

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

### `mos_substituteargs`

    int mos_substituteargs(char *tpl, char *args, char *dest,
                           int length, uint8_t flags);

Fill the template `tpl` (with `%0`, `%1`, `%*1` and the like) from the
arguments `args` into `dest`: call 0x35, `mos_substituteargs`, MOS 3,
`tpl` in HL, `args` in IX, `length` in DE, `dest` in IY and `flags`
in C. The answer is not a status but the length, which MOS answers in
BC.

### `mos_resolvepath`, `mos_getdirforpath`, `mos_getabsolutepath`

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
`resolved`: `path` in HL, `resolved` in IX, `*length` in DE. MOS 3 does
not work the length out yet and hands back what it was given, so
`*length` is left as it was.

### `mos_getleafname`, `mos_isdirectory`

    char   *mos_getleafname(const char *pathname);
    uint8_t mos_isdirectory(const char *pathname);

`mos_getleafname` answers where the last part of `pathname` starts
(`leaf.txt` in `a/b/leaf.txt`): call 0x3A, `mos_getleafname`, MOS 3,
answering in HL. `mos_isdirectory` answers MOS's status for whether
`pathname` is a directory: call 0x3B, `mos_isdirectory`, MOS 3. Both
pass `pathname` in HL.

### `mos_clearvdpflags`, `mos_waitforvdpflags`

    uint8_t mos_clearvdpflags(uint8_t bitmask);
    uint8_t mos_waitforvdpflags(uint8_t bitmask);

Clear the `vdp_pflag_` bits in `bitmask` from `sysvar_vdp_pflags`
(call 0x40, `mos_clearvdpflags`), or wait for them to be set (call
0x41, `mos_waitforvdpflags`). MOS 3. `bitmask` goes in C; each answers
the byte MOS leaves in A.

### `mos_getfunction`, `MOS_FUNC_SD_INIT`, `MOS_FUNC_SD_READBLOCKS`, `MOS_FUNC_SD_WRITEBLOCKS`, `MOS_FUNC_F_PRINTF`, `MOS_FUNC_F_FINDFIRST`, `MOS_FUNC_F_FINDNEXT`, `MOS_FUNC_OPEN_UART1`, `MOS_FUNC_SETVARVAL`, `MOS_FUNC_READVARVAL`, `MOS_FUNC_GSTRANS`, `MOS_FUNC_SUBSTITUTEARGS`, `MOS_FUNC_RESOLVEPATH`, `MOS_FUNC_GETDIRECTORYFORPATH`, `MOS_FUNC_RESOLVERELATIVEPATH`, `MOS_FUNC_GETSYSVARS`, `MOS_FUNC_GETKBMAP`

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

### `sd_getunlockcode`, `sd_init`

    uint24_t sd_getunlockcode(void);
    uint8_t  sd_init(uint24_t unlockcode);

The SD card a block at a time sits behind a code that has to be asked
for first. MOS 3. `sd_getunlockcode` answers it: call 0x70,
`sd_getunlockcode`, which writes it where HL points. MOS makes one
code and keeps it. `sd_init` (call 0x71, `sd_init`, HL pointing at
the code) starts the card and answers a status.

### `sd_readblocks`, `sd_writeblocks`

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

### `FIL`, `DIR`, `FILINFO`, `FFOBJID`, `FATFS`

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

### `AM_RDO`, `AM_HID`, `AM_SYS`, `AM_DIR`, `AM_ARC`

What a directory entry says about itself, in `FILINFO`'s `fattrib`:

| Name     | Value | Name     | Value |
|----------|-------|----------|-------|
| `AM_RDO` | 0x01  | `AM_DIR` | 0x10  |
| `AM_HID` | 0x02  | `AM_ARC` | 0x20  |
| `AM_SYS` | 0x04  |          |       |

### `ffs_fopen`, `ffs_fclose`

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

### `ffs_fread`, `ffs_fwrite`

    uint24_t ffs_fread(FIL *fh, char *buffer, uint24_t numbytes);
    uint24_t ffs_fwrite(FIL *fh, const char *buffer,
                        uint24_t numbytes);

Read or write up to `numbytes`: calls 0x82 `ffs_fread` and 0x83
`ffs_fwrite`, `buffer` in DE and `numbytes` in BC. Each answers how
many bytes it moved, which MOS answers in BC.

### `ffs_flseek`, `ffs_flseek_p`, `ffs_ftell`

    uint8_t ffs_flseek(FIL *fh, uint32_t offset);
    uint8_t ffs_flseek_p(FIL *fh, uint32_t *offset);
    uint8_t ffs_ftell(FIL *fh, uint32_t *result);

`ffs_flseek` moves to `offset` from the start: call 0x84, the
offset's low three bytes in DE and its top byte in C. `ffs_flseek_p`
(call 0xA6, MOS 3) takes the offset where `offset` points, in DE.
`ffs_ftell` (call 0x8D, MOS 3) stores the place at `*result`, in DE.
Each answers an `FRESULT`.

### `ffs_fgets`, `ffs_fputc`, `ffs_fputs`

    uint8_t *ffs_fgets(FIL *fh, char *buffer, uint24_t buffersize);
    uint24_t ffs_fputc(FIL *fh, char c);
    uint24_t ffs_fputs(FIL *fh, const char *string);

MOS 3. `ffs_fgets` reads a line into `buffer` (call 0x89: `buffer` in
DE, `buffersize` in BC) and answers `buffer`, or `NULL`, from DE.
`ffs_fputc` writes `c` (call 0x8A, `c` in C) and `ffs_fputs` writes
`string` (call 0x8B, `string` in DE); both answer what MOS answers in
BC, the count written.

### `ffs_fprintf`

    int ffs_fprintf(FIL *fp, const char *str, ...);

`fprintf` into a `FIL`. MOS 3's call 0x8C is not made: acc formats with
its own `printf`, which knows every conversion (see
[`printf`](2-io.md#printf-fprintf-sprintf-snprintf)), and writes
the text 64 bytes at a time with `ffs_fwrite`. The answer is how many
characters were formatted; a failed write is not seen. MOS's own `f_printf` is
`mos_getfunction(MOS_FUNC_F_PRINTF, 0)`.

### `ffs_ftruncate`, `ffs_fsync`, `ffs_feof`, `ffs_fsize`, `ffs_ferror`

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

### `ffs_dopen`, `ffs_dclose`, `ffs_dread`

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

### `ffs_dfindfirst`, `ffs_dfindnext`

    uint8_t ffs_dfindfirst(DIR *dir_handle, FILINFO *fil_handle,
                           const char *dirpath, const char *pattern);
    uint8_t ffs_dfindnext(DIR *dir_handle, FILINFO *fil_handle);

The entries of `dirpath` that match `pattern` (as `"*.txt"`): call
0x94, `ffs_dfindfirst`, MOS 3, `fil_handle` in DE, `dirpath` in BC
and `pattern` in IX, opens the directory and reads the first; call
0x95, `ffs_dfindnext`, reads the next. Each answers an `FRESULT`;
close with `ffs_dclose`.

### `ffs_stat`

    uint8_t ffs_stat(FILINFO *fil_handle, const char *filename);

Fill the `FILINFO` with what the directory says of `filename`: call
0x96, `ffs_stat`, `fil_handle` in HL and `filename` in DE. The answer
is an `FRESULT`.

### `ffs_unlink`, `ffs_rename`, `ffs_mkdir`, `ffs_chdir`, `ffs_getcwd`

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

### `ffs_mount`, `ffs_getfree`, `ffs_getlabel`, `ffs_setlabel`

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

### `ffs_fforward`, `ffs_expand`, `ffs_chmod`, `ffs_utime`, `ffs_chdrive`, `ffs_mkfs`, `ffs_fdisk`, `ffs_setcp`

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

### `FFCONF_DEF`, `FF_FS_READONLY`, `FF_FS_MINIMIZE`, `FF_USE_FIND`, `FF_USE_MKFS`, `FF_USE_FASTSEEK`, `FF_USE_EXPAND`, `FF_USE_CHMOD`, `FF_USE_LABEL`, `FF_USE_FORWARD`, `FF_USE_STRFUNC`, `FF_CODE_PAGE`, `FF_USE_LFN`, `FF_MAX_LFN`, `FF_LFN_UNICODE`, `FF_LFN_BUF`, `FF_SFN_BUF`, `FF_FS_RPATH`, `FF_VOLUMES`, `FF_MULTI_PARTITION`, `FF_MIN_SS`, `FF_MAX_SS`, `FF_LBA64`, `FF_FS_TINY`, `FF_FS_EXFAT`, `FF_FS_NORTC`, `FF_FS_NOFSINFO`, `FF_FS_LOCK`, `FF_FS_REENTRANT`

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

## `agon/vdp.h`

The VDP, the Agon's video processor, driven by VDU commands: the bytes a
program writes to the screen through MOS. This is the header a program
includes; it brings in one header for each part of the VDP, and gives
the colour names, the background offset and the shapes of VDU commands
that the others share.

| Header                                      | Has                      |
|---------------------------------------------|--------------------------|
| [agon/vdp/screen.h](#agonvdpscreenh)     | text, modes, the cursor  |
| [agon/vdp/graphics.h](#agonvdpgraphicsh) | PLOT and the shapes      |
| [agon/vdp/bitmap.h](#agonvdpbitmaph)     | bitmaps, sprites, fonts  |
| [agon/vdp/buffer.h](#agonvdpbufferh)     | the buffered commands    |
| [agon/vdp/audio.h](#agonvdpaudioh)       | sound                    |

It includes `stdbool.h`, `stdint.h` and [agon/mos.h](#agonmosh) too.

The names, the arguments and the bytes sent are libagon's, so that a
program written against libagon builds against acc and draws the same;
test/agonlib.sh holds each call to the bytes libagon sends. Past
libagon, every command VDP 2.16.0 has is here as well, named the way
libagon names its neighbours, and test/vdpsync.sh plays each into the
VDP firmware to hold it to what the VDP takes.

Most calls send one VDU command; a shape may send a few. Each command
goes with all of its bytes in one call to MOS (one `RST 18h`), as
libagon does. A 16-bit argument goes as two bytes,
low first; the VDU documentation writes one as `x;`, and so do these
pages. Where a call takes an `int` for a byte argument, only its low
byte is sent.

The few calls that read an answer send a request, and the VDP's answer
reaches the program through MOS's system variables: see
[agon/vdp/screen.h](#agonvdpscreenh).

### `BLACK`, `RED`, `GREEN`, `YELLOW`, `BLUE`, `MAGENTA`, `CYAN`, `WHITE`, `BRIGHT_BLACK`, `BRIGHT_RED`, `BRIGHT_GREEN`, `BRIGHT_YELLOW`, `BRIGHT_BLUE`, `BRIGHT_MAGENTA`, `BRIGHT_CYAN`, `BRIGHT_WHITE`

The sixteen colours, as an enum:

| Name             | Value | Name             | Value |
|------------------|-------|------------------|-------|
| `BLACK`          | 0     | `BRIGHT_BLACK`   | 8     |
| `RED`            | 1     | `BRIGHT_RED`     | 9     |
| `GREEN`          | 2     | `BRIGHT_GREEN`   | 10    |
| `YELLOW`         | 3     | `BRIGHT_YELLOW`  | 11    |
| `BLUE`           | 4     | `BRIGHT_BLUE`    | 12    |
| `MAGENTA`        | 5     | `BRIGHT_MAGENTA` | 13    |
| `CYAN`           | 6     | `BRIGHT_CYAN`    | 14    |
| `WHITE`          | 7     | `BRIGHT_WHITE`   | 15    |

### `BACKGROUND_COL_OFFSET`

    #define BACKGROUND_COL_OFFSET 128

A colour with 128 added is the background colour, for VDU 17 (the text
colour) and VDU 18 (the graphics colour).
[`vdp_set_text_bg_colour`](#vdp_set_text_colour-vdp_set_text_bg_colour)
adds it; [`vdp_set_graphics_bg_colour`](#vdp_set_graphics_colour-vdp_set_graphics_fg_colour-vdp_set_graphics_bg_colour-vdp_gcol)
flips the bit instead.

### `VDP_PUTS`, `VDU_A`, `VDU_A_x_y`, `VDU_ADV_CMD`, `VDU_AUDIO_CMD`

    #define VDP_PUTS(S) mos_puts((char *) &(S), sizeof (S), 0)

`VDP_PUTS` sends a structure of VDU bytes as it lies in memory, in one
`mos_puts`. The typedefs are libagon's shapes of VDU commands, for it:
a struct here has no padding, so each lies in memory as the bytes the
VDP reads. A `uint8_t` field is a byte, a `uint16_t` two bytes low
first, and a `uint24_t` three.

    VDU_A_x_y origin = { 29, 640, 512 };    /* VDU 29, 640; 512; */

    VDP_PUTS(origin);

The names spell out the fields: `A`, `B`, `C` and `CMD` are the command
bytes, a lower-case letter or `b0` a byte, `w0` a 16-bit word, `ww0` a
24-bit one, and `ui8x8` eight bytes.

| Family            | Shaped for                                      |
|-------------------|-------------------------------------------------|
| `VDU_A...`        | one command byte, then `CMD` or arguments       |
| `VDU_A_B_CMD...`  | two bytes and a command: VDU 23, 0, &C0, ...    |
| `VDU_ADV_CMD...`  | VDU 23, 0, &A0, a 16-bit buffer id, a command   |
| `VDU_AUDIO_...`   | VDU 23, 0, &85, a channel or sample, a command  |
| `VDU_FONT...`     | VDU 23, 0, &95, a subcommand, a buffer id       |
| `VDU_CONTEXT...`  | VDU 23, 0, &C8, a subcommand                    |
| `VDU_MOUSE...`    | VDU 23, 0, &89, a subcommand                    |

The header has 57 of them; it is the place to read their fields.

## `agon/vdp_vdu.h`

libagon's old name for [agon/vdp.h](#agonvdph), which libagon keeps so
that programs written against it still build. acc keeps it for the same
reason. It includes `agon/vdp.h` and declares nothing of its own, so
everything it gives is on that page and the pages it links to.

### `vdp_vdu.h`

    #include <agon/vdp_vdu.h>

is the same as

    #include <agon/vdp.h>

A new program includes `agon/vdp.h`.

## `agon/vdp/screen.h`

Text, colour, modes, viewports, the cursor, characters, the keyboard and
the VDP's system commands. It is part of [agon/vdp.h](#agonvdph),
which is what a program includes; graphics are in
[agon/vdp/graphics.h](#agonvdpgraphicsh).

Each call sends what libagon sends for the same arguments. The calls
libagon does not have send VDP 2.16.0's commands as its own source
reads them. A byte argument passed as an `int` is sent as its low byte;
`x;` is a 16-bit argument, sent low byte first.

A call that asks the VDP for something, such as the cursor's place, a
pixel or the screen's size, is answered through MOS: the VDP sends a
packet, MOS puts what is in it in the system variables (`sys_vars`, in
[agon/mos.h](#agonmosh)) and sets a bit in `sys_vars->vdp_pflags`. A
`vdp_request_` call given a true `wait` clears all of `vdp_pflags`,
sends the request, and waits until the bit for its answer is set; there
is no time limit. Given false, it sends the request and returns at once,
and the answer lands in the system variables when it comes. A
`vdp_return_` call always waits, and answers what arrived.

### `vdp_vdu_init`

    volatile SYSVAR *vdp_vdu_init(void);

Answers `sys_vars`, the system variables. It is libagon's name for them
from before it had `sys_vars`, and sends nothing.

### `vdp_send_to_printer`, `vdp_enable_printer`, `vdp_disable_printer`

    void vdp_send_to_printer(char ch);
    void vdp_enable_printer(void);
    void vdp_disable_printer(void);

VDU 1, `ch`; VDU 2; VDU 3.

### `vdp_enable_screen`, `vdp_disable_screen`, `vdp_bell`

    void vdp_enable_screen(void);
    void vdp_disable_screen(void);
    void vdp_bell(void);

VDU 6; VDU 21; VDU 7.

### `vdp_write_at_text_cursor`, `vdp_write_at_graphics_cursor`

    void vdp_write_at_text_cursor(void);
    void vdp_write_at_graphics_cursor(void);

VDU 4 and VDU 5: text is written at the text cursor, or at the graphics
cursor.

### `vdp_clear_screen`, `vdp_cls`

    void vdp_clear_screen(void);
    #define vdp_cls() vdp_clear_screen()

VDU 12.

### `vdp_cursor_left`, `vdp_cursor_right`, `vdp_cursor_down`, `vdp_cursor_up`

    void vdp_cursor_left(void);
    void vdp_cursor_right(void);
    void vdp_cursor_down(void);
    void vdp_cursor_up(void);

VDU 8, 9, 10 and 11: the text cursor one place left, right, down or up.

### `vdp_carriage_return`, `vdp_cursor_home`, `vdp_backspace`

    void vdp_carriage_return(void);
    void vdp_cursor_home(void);
    void vdp_backspace(void);

VDU 13; VDU 30; VDU 127.

### `vdp_cursor_tab`

    void vdp_cursor_tab(int xpos, int ypos);

VDU 31, `xpos`, `ypos`: the text cursor to that column and row, each a
byte.

    vdp_cursor_tab(10, 5);
    puts("here");

### `vdp_outchar`, `vdp_send_to_screen`

    void vdp_outchar(uint8_t c);
    void vdp_send_to_screen(char c);

VDU 27, `c`: `c` is drawn as a character, whatever it is, so a control
code shows as its glyph rather than acting. The two are libagon's two
names for the same thing.

### `vdp_page_mode_on`, `vdp_page_mode_off`, `vdp_temp_paged_mode`

    void vdp_page_mode_on(void);
    void vdp_page_mode_off(void);
    void vdp_temp_paged_mode(void);

VDU 14 and VDU 15: paged mode on and off. `vdp_temp_paged_mode` sends
VDU 23, 0, &9A: paged mode until the next time the screen scrolls a
page.

### `vdp_set_text_colour`, `vdp_set_text_bg_colour`

    void vdp_set_text_colour(int colour);
    void vdp_set_text_bg_colour(int colour);

VDU 17, `colour`: the text colour. `vdp_set_text_bg_colour` sends VDU
17, `colour` + 128 (`BACKGROUND_COL_OFFSET`), the text background. The
colour names, `BLACK` to `BRIGHT_WHITE`, are in
[agon/vdp.h](#agonvdph).

    vdp_set_text_colour(BRIGHT_YELLOW);
    vdp_set_text_bg_colour(BLUE);

### `vdp_define_colour`, `vdp_reset_graphics`

    void vdp_define_colour(int logical, int physical, int red,
                           int green, int blue);
    void vdp_reset_graphics(void);

`vdp_define_colour` sends VDU 19, `logical`, `physical`, `red`,
`green`, `blue`, one byte each: the colour a logical colour number
stands for. `vdp_reset_graphics` sends VDU 20.

### `vdp_mode`

    int vdp_mode(int mode);

VDU 22, `mode`: the screen mode. The answer is `mode`. A `mode` of 255
or more is not sent, and the answer is -1; a negative one is sent as its
low byte.

    if (vdp_mode(8) < 0)
        return 1;

### `vdp_reset_viewports`, `vdp_set_text_viewport`

    void vdp_reset_viewports(void);
    void vdp_set_text_viewport(int left, int bottom, int right, int top);

`vdp_reset_viewports` sends VDU 26. `vdp_set_text_viewport` sends VDU
28, `left`, `bottom`, `right`, `top`: the text viewport, its corners
one byte each. The graphics viewport is
[`vdp_set_graphics_viewport`](#vdp_set_graphics_viewport).

### `vdp_set_text_viewport_via_plot`, `vdp_set_graphics_viewport_via_plot`, `vdp_set_graphics_origin_via_plot`, `vdp_move_graphics_origin_and_viewport`

    void vdp_set_text_viewport_via_plot(void);
    void vdp_set_graphics_viewport_via_plot(void);
    void vdp_set_graphics_origin_via_plot(void);
    void vdp_move_graphics_origin_and_viewport(void);

The viewports and the origin from the last two graphics points, as
PLOT left them:

| Call                                    | Sends              |
|-----------------------------------------|--------------------|
| `vdp_set_text_viewport_via_plot`        | VDU 23, 0, &9C     |
| `vdp_set_graphics_viewport_via_plot`    | VDU 23, 0, &9D     |
| `vdp_set_graphics_origin_via_plot`      | VDU 23, 0, &9E     |
| `vdp_move_graphics_origin_and_viewport` | VDU 23, 0, &9F     |

### `vdp_cursor_enable`, `vdp_cursor_behaviour`

    void vdp_cursor_enable(bool flag);
    void vdp_cursor_behaviour(int setting, int mask);

`vdp_cursor_enable` sends VDU 23, 1, `flag`: the text cursor shown
(true) or hidden (false). `vdp_cursor_behaviour` sends VDU 23, 16,
`setting`, `mask`.

### `vdp_set_cursor_start_line`, `vdp_set_cursor_end_line`, `vdp_set_cursor_start_column`, `vdp_set_cursor_end_column`

    void vdp_set_cursor_start_line(int n);
    void vdp_set_cursor_end_line(int n);
    void vdp_set_cursor_start_column(int n);
    void vdp_set_cursor_end_column(int n);

The cursor's shape: the rows and columns of the character cell it
covers. The start row's top bits choose how it blinks.

| Call                          | Sends               |
|-------------------------------|---------------------|
| `vdp_set_cursor_start_line`   | VDU 23, 0, &0A, n   |
| `vdp_set_cursor_end_line`     | VDU 23, 0, &0B, n   |
| `vdp_set_cursor_start_column` | VDU 23, 0, &8A, n   |
| `vdp_set_cursor_end_column`   | VDU 23, 0, &8B, n   |

### `vdp_move_cursor_relative`

    void vdp_move_cursor_relative(int x, int y);

VDU 23, 0, &8C, `x`, `y`: the text cursor moved by `x` and `y` pixels,
each a signed byte, as VDP 2.16.0 reads them. libagon sends two bytes
each, which the VDP reads as `x`, `y` and two more commands; acc sends
one.

### `vdp_scroll_screen`, `vdp_scroll_screen_extent`

    void vdp_scroll_screen(int direction, int speed);
    void vdp_scroll_screen_extent(int extent, int direction, int speed);

VDU 23, 7, `extent`, `direction`, `speed`: scroll in a direction, by
`speed` pixels. `vdp_scroll_screen` is libagon's, and scrolls the
screen: its `extent` is 1.

| `extent` | Scrolls               |
|----------|-----------------------|
| 0        | the text viewport     |
| 1        | the screen            |
| 2        | the graphics viewport |

### `vdp_redefine_character`, `vdp_redefine_character_special`, `vdp_define_character`, `vdp_reset_system_font`

    void vdp_redefine_character(int char_num, uint8_t b0, uint8_t b1,
                                uint8_t b2, uint8_t b3, uint8_t b4,
                                uint8_t b5, uint8_t b6, uint8_t b7);
    void vdp_redefine_character_special(int char_num, uint8_t b0,
                                        uint8_t b1, uint8_t b2,
                                        uint8_t b3, uint8_t b4,
                                        uint8_t b5, uint8_t b6,
                                        uint8_t b7);
    void vdp_define_character(int char_num, uint8_t *data);
    void vdp_reset_system_font(void);

A character's shape, eight bytes, one a row of pixels.

`vdp_redefine_character` sends VDU 23, `char_num`, `b0`, ..., `b7`. It
cannot reach the characters below 32: VDU 23 with those is the other
VDU 23 commands.

`vdp_redefine_character_special` sends VDU 23, 0, &90, `char_num`,
`b0`, ..., `b7`, which reaches any character, 0 to 255.
`vdp_define_character` sends the same with the eight bytes from `data`
(VDU 23, 0, &90, `char_num`, then the bytes as a second run), and
nothing at all when `data` is `NULL`.

`vdp_reset_system_font` sends VDU 23, 0, &91: the system font back.

    static uint8_t arrow[8] = {
        0x18, 0x3c, 0x7e, 0xff, 0x18, 0x18, 0x18, 0x00
    };

    vdp_define_character(128, arrow);
    vdp_outchar(128);

### `vdp_request_text_cursor_position`, `vdp_return_text_cursor_position`

    void vdp_request_text_cursor_position(bool wait);
    void vdp_return_text_cursor_position(uint8_t *return_x,
                                         uint8_t *return_y);

VDU 23, 0, &82. The answer is `sys_vars->cursorX` and `cursorY`, the
column and row; its flag is `vdp_pflag_cursor`.

`vdp_return_text_cursor_position` asks and waits, then stores the column
in `*return_x` and the row in `*return_y`; either pointer may be `NULL`.

    uint8_t col, row;

    vdp_return_text_cursor_position(&col, &row);

### `vdp_request_ascii_code_at_position`, `vdp_return_ascii_code_at_position`, `vdp_request_ascii_code_at_graphics_position`, `vdp_return_ascii_code_at_graphics_position`

    void    vdp_request_ascii_code_at_position(int x, int y, bool wait);
    uint8_t vdp_return_ascii_code_at_position(int x, int y);
    void    vdp_request_ascii_code_at_graphics_position(int x, int y,
                                                        bool wait);
    uint8_t vdp_return_ascii_code_at_graphics_position(int x, int y);

The character on the screen at a place. VDU 23, 0, &83, `x;` `y;` asks
at a text column and row; VDU 23, 0, &93, `x;` `y;` at a graphics
point. The answer is `sys_vars->scrchar`, and its flag is
`vdp_pflag_scrchar`. The `vdp_return_` calls answer it.

### `vdp_request_pixel_colour`, `vdp_return_pixel_colour`

    void     vdp_request_pixel_colour(int x, int y, bool wait);
    uint24_t vdp_return_pixel_colour(int x, int y);

VDU 23, 0, &84, `x;` `y;`: the colour of the pixel at a graphics point.
The answer is `sys_vars->scrpixel`, the colour, and `scrpixelIndex`, its
palette index; its flag is `vdp_pflag_point`. `vdp_return_pixel_colour`
answers `scrpixel`, 0 for black.

Drawing is queued in the VDP: test/vdpreal calls
[`vdp_flush_drawing_commands`](#vdp_legacy_modes-vdp_swap-vdp_flush_drawing_commands)
after drawing and before reading a pixel back.

### `vdp_request_palette_entry`, `vdp_return_palette_entry_colour`, `vdp_return_palette_entry_index`

    void     vdp_request_palette_entry(int n, bool wait);
    uint24_t vdp_return_palette_entry_colour(int n);
    uint8_t  vdp_return_palette_entry_index(int n);

VDU 23, 0, &94, `n`: palette entry `n`. The answer is in the same places
as a pixel's, `sys_vars->scrpixel` and `scrpixelIndex`, with the same
flag, `vdp_pflag_point`. `vdp_return_palette_entry_colour` answers
`scrpixel`, and `vdp_return_palette_entry_index` `scrpixelIndex`; each
asks the VDP once.

### `vdp_get_scr_dims`

    void vdp_get_scr_dims(bool wait);

VDU 23, 0, &86: the screen's size and mode. The answer is in
`sys_vars->scrWidth` and `scrHeight` (pixels), `scrCols` and `scrRows`
(characters), `scrColours` and `scrMode`; its flag is `vdp_pflag_mode`.

    vdp_get_scr_dims(true);
    printf("%d x %d\n", sys_vars->scrWidth, sys_vars->scrHeight);

### `vdp_request_rtc`, `vdp_set_rtc`

    void vdp_request_rtc(bool wait);
    void vdp_set_rtc(int year, int month, int day, int hour, int minute,
                     int second);

`vdp_request_rtc` sends VDU 23, 0, &87, 0: the clock. The answer is
`sys_vars->rtc`, six bytes MOS keeps packed: what they mean is the
`vdp_time_t` that `mos_unpackrtc` in [agon/mos.h](#agonmosh) fills
in. Its flag is `vdp_pflag_rtc`.

`vdp_set_rtc` sends VDU 23, 0, &87, 1, `year` - 1980, `month`, `day`,
`hour`, `minute`, `second`, one byte each: the clock set, with the year
counted from 1980.

### `vdp_general_poll`

    void vdp_general_poll(int n);

VDU 23, 0, &80, `n`: the general poll, which the VDP answers with `n`.
MOS 3 keeps the answer in `sys_vars->gp`. It does not wait, and no flag
marks the answer.

### `vdp_set_keyboard_locale`, `vdp_keyboard_control`

    void vdp_set_keyboard_locale(int locale);
    void vdp_keyboard_control(int delay, int rate, int led);

`vdp_set_keyboard_locale` sends VDU 23, 0, &81, `locale`: the keyboard's
layout. `vdp_keyboard_control` sends VDU 23, 0, &88, `delay;` `rate;`
`led`: the repeat delay and rate, and the lights.

### `vdp_control_keys`, `vdp_check_key`

    void vdp_control_keys(bool on);
    void vdp_check_key(int vkey);

`vdp_control_keys` sends VDU 23, 0, &98, `on`. `vdp_check_key` sends
VDU 23, 0, &99, `vkey`: the VDP sends a new key packet for that virtual
key, as it now stands, and MOS puts it in the system variables
(`vkeycode`, `vkeydown`, and the rest).

### `vdp_getKeyMap`, `vdp_getKeyCode`, `vdp_waitKeyUp`, `vdp_waitKeyDown`

    uint8_t vdp_getKeyMap(uint8_t index);
    uint8_t vdp_getKeyCode(void);
    void    vdp_waitKeyUp(void);
    void    vdp_waitKeyDown(void);

These send nothing; they read what MOS already holds.

`vdp_getKeyMap` answers byte `index` of MOS's keyboard map (from
`mos_getkbmap`): sixteen bytes, eight keys a byte.

`vdp_getKeyCode` answers `sys_vars->keyascii` while a key is down
(`sys_vars->vkeydown` is not 0), and 0 while none is.

`vdp_waitKeyUp` waits until no key is down, and `vdp_waitKeyDown` until
one is, by `vkeydown`.

    vdp_waitKeyDown();
    key = vdp_getKeyCode();
    vdp_waitKeyUp();

### `vdp_logical_scr_dims`, `vdp_set_pixel_coordinates`, `vdp_set_logical_coordinates`

    void vdp_logical_scr_dims(bool flag);
    void vdp_set_pixel_coordinates(void);
    void vdp_set_logical_coordinates(void);

VDU 23, 0, &C0, `n`: graphics coordinates in pixels (`n` 0) or logical
(`n` 1). Logical coordinates are BBC BASIC's 1280 by 1024, whatever the
mode; pixel coordinates are the mode's own.

| Call                          | Sends                    |
|-------------------------------|--------------------------|
| `vdp_logical_scr_dims(flag)`  | VDU 23, 0, &C0, flag & 1 |
| `vdp_set_pixel_coordinates`   | VDU 23, 0, &C0, 0        |
| `vdp_set_logical_coordinates` | VDU 23, 0, &C0, 1        |

### `vdp_legacy_modes`, `vdp_swap`, `vdp_flush_drawing_commands`

    void vdp_legacy_modes(bool on);
    void vdp_swap(void);
    void vdp_flush_drawing_commands(void);

| Call                         | Sends                 |
|------------------------------|-----------------------|
| `vdp_legacy_modes`           | VDU 23, 0, &C1, `on`  |
| `vdp_swap`                   | VDU 23, 0, &C3        |
| `vdp_flush_drawing_commands` | VDU 23, 0, &CA        |

### `vdp_console_mode`, `vdp_terminal_mode`

    void vdp_console_mode(bool on);
    void vdp_terminal_mode(void);

`vdp_console_mode` sends VDU 23, 0, &FE, `on`. `vdp_terminal_mode`
sends VDU 23, 0, &FF: terminal mode, which takes the VDU commands away
until the VDP is reset.

### `vdp_print_buffer`

    void vdp_print_buffer(int bufferId);

VDU 23, 0, &9B, `bufferId;`: the bytes of a buffer drawn as characters,
none of them taken as a command. Buffers are in
[agon/vdp/buffer.h](#agonvdpbufferh).

### `vdp_set_variable`, `vdp_clear_variable`

    void vdp_set_variable(uint16_t variableId, uint16_t value);
    void vdp_clear_variable(uint16_t variableId);

The VDP's variables, which turn features on and hold settings.
`vdp_set_variable` sends VDU 23, 0, &F8, `variableId;` `value;`, and
`vdp_clear_variable` VDU 23, 0, &F9, `variableId;`.

## `agon/vdp/graphics.h`

The graphics colour, viewport and origin, the line style, and PLOT with
every shape it draws. It is part of [agon/vdp.h](#agonvdph), which is
what a program includes; text and the screen mode are in
[agon/vdp/screen.h](#agonvdpscreenh), and bitmaps in
[agon/vdp/bitmap.h](#agonvdpbitmaph).

Coordinates are the VDP's: 16-bit, from the graphics origin, with y up
unless the program has turned logical coordinates off
([`vdp_set_pixel_coordinates`](#vdp_logical_scr_dims-vdp_set_pixel_coordinates-vdp_set_logical_coordinates)).
`x;` is a 16-bit argument, sent low byte first.

Nearly all of it is PLOT, VDU 25, a code, `x;` `y;`. The VDP keeps the
last three points it was sent, and a shape takes its corners from them:
a filled triangle is two moves and a plot, and a circle a move to the
centre and a plot relative to it. Where libagon has a call, acc sends
the same commands in the same order.

    vdp_mode(8);
    vdp_set_pixel_coordinates();
    vdp_clg();
    vdp_gcol(GCOLMODE_COLOUR, BRIGHT_RED);
    vdp_filled_circle(160, 120, 40);

### `vdp_clear_graphics`, `vdp_clg`

    void vdp_clear_graphics(void);
    #define vdp_clg() vdp_clear_graphics()

VDU 16: clear the graphics viewport (CLG).

### `vdp_set_graphics_colour`, `vdp_set_graphics_fg_colour`, `vdp_set_graphics_bg_colour`, `vdp_gcol`

    void vdp_set_graphics_colour(uint8_t mode, uint8_t colour);
    void vdp_set_graphics_fg_colour(uint8_t mode, uint8_t colour);
    void vdp_set_graphics_bg_colour(uint8_t mode, uint8_t colour);
    #define vdp_gcol(M, C) vdp_set_graphics_colour(M, C)

VDU 18, `mode`, `colour`: the graphics colour, as GCOL `mode`, `colour`
sets it, painted in one of the `GCOLMODE_` modes below. A colour of 128
or more is the background colour.

`vdp_set_graphics_fg_colour` is the same as `vdp_set_graphics_colour`.
`vdp_set_graphics_bg_colour` takes a colour from 0 to 127 and sends it
with its top bit flipped (`colour ^ 128`), as libagon does: a colour
that is already 128 or more comes out a foreground one.

### `GCOLMODE_COLOUR`, `GCOLMODE_OR`, `GCOLMODE_AND`, `GCOLMODE_EOR`, `GCOLMODE_INVERT`, `GCOLMODE_NOP`, `GCOLMODE_AND_INV_COLOUR`, `GCOLMODE_OR_INV_COLOUR`

| Name                      | Value | The pixel                       |
|---------------------------|-------|---------------------------------|
| `GCOLMODE_COLOUR`         | 0     | set to the colour               |
| `GCOLMODE_OR`             | 1     | ORed with the colour            |
| `GCOLMODE_AND`            | 2     | ANDed with the colour           |
| `GCOLMODE_EOR`            | 3     | EORed with the colour           |
| `GCOLMODE_INVERT`         | 4     | inverted                        |
| `GCOLMODE_NOP`            | 5     | left alone                      |
| `GCOLMODE_AND_INV_COLOUR` | 6     | ANDed with the colour inverted  |
| `GCOLMODE_OR_INV_COLOUR`  | 7     | ORed with the colour inverted   |

### `vdp_set_graphics_viewport`

    void vdp_set_graphics_viewport(int left, int bottom, int right,
                                   int top);

VDU 24, `left;` `bottom;` `right;` `top;`: the graphics viewport.

### `vdp_graphics_origin`

    void vdp_graphics_origin(int x, int y);

VDU 29, `x;` `y;`: the graphics origin, which every later point counts
from.

### `vdp_set_line_thickness`

    void vdp_set_line_thickness(int pixels);

VDU 23, 23, `pixels`: lines `pixels` thick, a byte. libagon's sends
VDU 23, `pixels`, 0, which the VDP takes for some other VDU 23 command;
acc sends what the VDP reads.

### `vdp_set_dotdash_line_pattern`, `vdp_set_dotdash_pattern_length`

    void vdp_set_dotdash_line_pattern(uint8_t b0, uint8_t b1, uint8_t b2,
                                      uint8_t b3, uint8_t b4, uint8_t b5,
                                      uint8_t b6, uint8_t b7);
    void vdp_set_dotdash_pattern_length(int n);

`vdp_set_dotdash_line_pattern` sends VDU 23, 6, `b0`, ..., `b7`: the
pattern of the dot-dash lines, one bit a pixel.

`vdp_set_dotdash_pattern_length` sends VDU 23, 0, &F2, `n`: how many of
those bits are used, 1 to 64, or 0 for the default pattern back. An `n`
above 64 sends nothing, as in libagon, though the VDP takes up to 255.
A negative `n` is not refused: it is sent as its low byte.

### `vdp_plot`

    void vdp_plot(int plot_mode, int x, int y);

VDU 25, `plot_mode`, `x;` `y;`: PLOT. The code is an operation, a
`PLOT_` constant, ORed with a `PLOTMODE_`: what the point is relative
to, and what colour it is drawn in.

    vdp_plot(PLOT_POINT | PLOTMODE_FG_ABSOLUTE, 100, 100);  /* PLOT 69 */

### `PLOTMODE_MOVE_RELATIVE`, `PLOTMODE_FG_RELATIVE`, `PLOTMODE_INVERT_RELATIVE`, `PLOTMODE_BG_RELATIVE`, `PLOTMODE_MOVE_ABSOLUTE`, `PLOTMODE_FG_ABSOLUTE`, `PLOTMODE_INVERT_ABSOLUTE`, `PLOTMODE_BG_ABSOLUTE`

| Name                       | Value | The point is, and drawn in   |
|----------------------------|-------|------------------------------|
| `PLOTMODE_MOVE_RELATIVE`   | 0     | relative; a move             |
| `PLOTMODE_FG_RELATIVE`     | 1     | relative; the foreground     |
| `PLOTMODE_INVERT_RELATIVE` | 2     | relative; inverted           |
| `PLOTMODE_BG_RELATIVE`     | 3     | relative; the background     |
| `PLOTMODE_MOVE_ABSOLUTE`   | 4     | absolute; a move             |
| `PLOTMODE_FG_ABSOLUTE`     | 5     | absolute; the foreground     |
| `PLOTMODE_INVERT_ABSOLUTE` | 6     | absolute; inverted           |
| `PLOTMODE_BG_ABSOLUTE`     | 7     | absolute; the background     |

A relative point is from the last point plotted; an absolute one from
the graphics origin.

### `PLOT_LINE`, `PLOT_LINE_OMIT_LAST`, `PLOT_DOTDASH`, `PLOT_DOTDASH_OMIT_LAST`, `PLOT_LINE_OMIT_FIRST`, `PLOT_LINE_OMIT_BOTH`, `PLOT_DOTDASH_OMIT_FIRST`, `PLOT_DOTDASH_OMIT_BOTH`, `PLOT_POINT`

The line operations for `vdp_plot`, and the call that draws with each.

| Name                           | Hex | Used by                          |
|--------------------------------|-----|----------------------------------|
| `PLOT_LINE`                    | &00 | vdp_line_to                      |
| `PLOT_LINE_OMIT_LAST`          | &08 | vdp_line_to_omit_last            |
| `PLOT_DOTDASH`                 | &10 | vdp_dotdash_line_to              |
| `PLOT_DOTDASH_OMIT_LAST`       | &18 | vdp_dotdash_line_to_omit_last    |
| `PLOT_LINE_OMIT_FIRST`         | &20 | vdp_line_to_omit_first           |
| `PLOT_LINE_OMIT_BOTH`          | &28 | vdp_line_to_omit_both            |
| `PLOT_DOTDASH_OMIT_FIRST`      | &30 | vdp_dotdash_line_to_omit_first   |
| `PLOT_DOTDASH_OMIT_BOTH`       | &38 | vdp_dotdash_line_to_omit_both    |
| `PLOT_POINT`                   | &40 | vdp_point                        |

The omit-first and omit-both dot-dash lines carry the pattern on from
the line before.

### `PLOT_FILL_LEFTRIGHT_TO_NONBG`, `PLOT_FILL_RIGHT_TO_BG`, `PLOT_FILL_LEFTRIGHT_TO_FG`, `PLOT_FILL_RIGHT_TO_NONFG`, `PLOT_FLOOD_TO_NONBG`, `PLOT_FLOOD_TO_FG`

The fill operations for `vdp_plot`, and the call that draws with each.

| Name                           | Hex | Used by                          |
|--------------------------------|-----|----------------------------------|
| `PLOT_FILL_LEFTRIGHT_TO_NONBG` | &48 | vdp_line_fill_leftright_to_nonbg |
| `PLOT_FILL_RIGHT_TO_BG`        | &58 | vdp_line_fill_right_to_bg        |
| `PLOT_FILL_LEFTRIGHT_TO_FG`    | &68 | vdp_line_fill_leftright_to_fg    |
| `PLOT_FILL_RIGHT_TO_NONFG`     | &78 | vdp_line_fill_right_to_nonfg     |
| `PLOT_FLOOD_TO_NONBG`          | &80 | vdp_flood_fill_to_nonbg          |
| `PLOT_FLOOD_TO_FG`             | &88 | vdp_flood_fill_to_fg             |

### `PLOT_TRIANGLE`, `PLOT_RECTANGLE`, `PLOT_PARALLELOGRAM`, `PLOT_CIRCLE`, `PLOT_FILLED_CIRCLE`, `PLOT_ARC`, `PLOT_SEGMENT`, `PLOT_SECTOR`, `PLOT_COPY_MOVE`, `PLOT_ELLIPSE`, `PLOT_FILLED_ELLIPSE`, `PLOT_PATH`, `PLOT_BITMAP`

The shape operations for `vdp_plot`, and the call that draws with each.

| Name                           | Hex | Used by                          |
|--------------------------------|-----|----------------------------------|
| `PLOT_TRIANGLE`                | &50 | vdp_filled_triangle              |
| `PLOT_RECTANGLE`               | &60 | vdp_filled_rectangle             |
| `PLOT_PARALLELOGRAM`           | &70 | vdp_filled_parallelogram         |
| `PLOT_CIRCLE`                  | &90 | vdp_circle                       |
| `PLOT_FILLED_CIRCLE`           | &98 | vdp_filled_circle                |
| `PLOT_ARC`                     | &A0 | vdp_arc                          |
| `PLOT_SEGMENT`                 | &A8 | vdp_segment                      |
| `PLOT_SECTOR`                  | &B0 | vdp_sector                       |
| `PLOT_COPY_MOVE`               | &B8 | vdp_copy_rectangle               |
| `PLOT_ELLIPSE`                 | &C0 | vdp_ellipse                      |
| `PLOT_FILLED_ELLIPSE`          | &C8 | vdp_filled_ellipse               |
| `PLOT_PATH`                    | &D8 | vdp_path_point                   |
| `PLOT_BITMAP`                  | &E8 | vdp_plot_bitmap                  |

With `PLOT_COPY_MOVE`, modes 1 and 5 move the rectangle and the rest
copy it.

### `vdp_move_to`, `vdp_move_by`, `vdp_line_to`, `vdp_line_by`, `vdp_line`

    void vdp_move_to(int x, int y);
    void vdp_move_by(int dx, int dy);
    void vdp_line_to(int x, int y);
    void vdp_line_by(int dx, int dy);
    void vdp_line(int x1, int y1, int x2, int y2);

The graphics cursor moved, or a line drawn from it in the foreground
colour, to a point or by a distance:

| Call          | Sends                                   |
|---------------|-----------------------------------------|
| `vdp_move_to` | PLOT 4, `x;` `y;`                       |
| `vdp_move_by` | PLOT 0, `dx;` `dy;`                     |
| `vdp_line_to` | PLOT 5, `x;` `y;`                       |
| `vdp_line_by` | PLOT 1, `dx;` `dy;`                     |
| `vdp_line`    | PLOT 4, `x1;` `y1;`, then PLOT 5, `x2;` `y2;` |

`vdp_line` moves to its first point itself; it needs no `vdp_move_to`
first.

    vdp_move_to(0, 0);
    vdp_line_to(319, 239);
    vdp_line_by(-319, 0);

### `vdp_line_to_omit_last`, `vdp_line_to_omit_first`, `vdp_line_to_omit_both`

    void vdp_line_to_omit_last(int x, int y);
    void vdp_line_to_omit_first(int x, int y);
    void vdp_line_to_omit_both(int x, int y);

A line from the graphics cursor to (`x`, `y`) without its last point,
its first, or both: PLOT &0D, &25 or &2D, `x;` `y;`.

### `vdp_dotdash_line_to`, `vdp_dotdash_line`, `vdp_dotdash_line_to_omit_last`, `vdp_dotdash_line_to_omit_first`, `vdp_dotdash_line_to_omit_both`

    void vdp_dotdash_line_to(int x, int y);
    void vdp_dotdash_line(int x1, int y1, int x2, int y2);
    void vdp_dotdash_line_to_omit_last(int x, int y);
    void vdp_dotdash_line_to_omit_first(int x, int y);
    void vdp_dotdash_line_to_omit_both(int x, int y);

The same lines in the dot-dash pattern
([`vdp_set_dotdash_line_pattern`](#vdp_set_dotdash_line_pattern-vdp_set_dotdash_pattern_length)):
PLOT &15, &1D, &35 or &3D, `x;` `y;`. `vdp_dotdash_line` is PLOT 4 to
its first point, then PLOT &15 to its second. The omit-first and
omit-both lines carry the pattern on.

### `vdp_point`

    void vdp_point(int x, int y);

PLOT &45, `x;` `y;`: one pixel in the foreground colour.

### `vdp_line_fill_leftright_to_nonbg`, `vdp_line_fill_leftright_to_fg`, `vdp_line_fill_right_to_bg`, `vdp_line_fill_right_to_nonfg`, `vdp_flood_fill_to_nonbg`, `vdp_flood_fill_to_fg`

    void vdp_line_fill_leftright_to_nonbg(int x, int y);
    void vdp_line_fill_leftright_to_fg(int x, int y);
    void vdp_line_fill_right_to_bg(int x, int y);
    void vdp_line_fill_right_to_nonfg(int x, int y);
    void vdp_flood_fill_to_nonbg(int x, int y);
    void vdp_flood_fill_to_fg(int x, int y);

Fills in the foreground colour, one PLOT each. The line fills fill a
row from (`x`, `y`), left and right or right only, up to a pixel that
is or is not the foreground or the background colour; the flood fills
fill the whole area around (`x`, `y`) the same way.

| Call                               | Sends               |
|------------------------------------|---------------------|
| `vdp_line_fill_leftright_to_nonbg` | PLOT &4D, `x;` `y;` |
| `vdp_line_fill_right_to_bg`        | PLOT &5D, `x;` `y;` |
| `vdp_line_fill_leftright_to_fg`    | PLOT &6D, `x;` `y;` |
| `vdp_line_fill_right_to_nonfg`     | PLOT &7D, `x;` `y;` |
| `vdp_flood_fill_to_nonbg`          | PLOT &85, `x;` `y;` |
| `vdp_flood_fill_to_fg`             | PLOT &8D, `x;` `y;` |

### `vdp_triangle`, `vdp_filled_triangle`

    void vdp_triangle(int x1, int y1, int x2, int y2, int x3, int y3);
    void vdp_filled_triangle(int x1, int y1, int x2, int y2, int x3,
                             int y3);

`vdp_triangle` draws the outline line by line, as libagon does: PLOT 4
to the first corner, then PLOT 5 to the second, the third and the first
again. `vdp_filled_triangle` is PLOT 4 to the first corner and the
second, then PLOT &55 to the third.

### `vdp_rectangle`, `vdp_filled_rectangle`

    void vdp_rectangle(int x1, int y1, int x2, int y2);
    void vdp_filled_rectangle(int x1, int y1, int x2, int y2);

The rectangle between two opposite corners. `vdp_rectangle` is PLOT 4
to (`x1`, `y1`), then four PLOT 5s round to (`x2`, `y1`), (`x2`, `y2`),
(`x1`, `y2`) and back. `vdp_filled_rectangle` is PLOT 4 to (`x1`,
`y1`), then PLOT &65 to (`x2`, `y2`).

### `vdp_parallelogram`, `vdp_filled_parallelogram`

    void vdp_parallelogram(int x1, int y1, int x2, int y2, int x3,
                           int y3);
    void vdp_filled_parallelogram(int x1, int y1, int x2, int y2,
                                  int x3, int y3);

Three corners in order. `vdp_filled_parallelogram` is PLOT 4 to the
first two and PLOT &75 to the third, and the VDP works out the fourth.

`vdp_parallelogram` draws the outline line by line: PLOT 4 to the first
corner, then PLOT 5 to the second, the third, the fourth
(`x1 - x2 + x3`, `y1 - y2 + y3`, the one the VDP would work out) and
the first again.

### `vdp_circle`, `vdp_filled_circle`

    void vdp_circle(int x, int y, int radius);
    void vdp_filled_circle(int x, int y, int radius);

A circle around (`x`, `y`): a move to the centre with the circle's code,
then a plot relative to it, whose distance is the radius.
`vdp_circle` sends PLOT &94, `x;` `y;` and PLOT &91, `radius;` 0;.
`vdp_filled_circle` sends PLOT &9C and PLOT &99 the same way.

### `vdp_arc`, `vdp_segment`, `vdp_sector`

    void vdp_arc(int centre_x, int centre_y, int x1, int y1, int x2,
                 int y2);
    void vdp_segment(int centre_x, int centre_y, int x1, int y1, int x2,
                     int y2);
    void vdp_sector(int centre_x, int centre_y, int x1, int y1, int x2,
                    int y2);

An arc, segment or sector of the circle around the centre, from the
first point on it to the second. Each is PLOT 4 to the centre and to
(`x1`, `y1`), then PLOT &A5 (arc), &AD (segment) or &B5 (sector) to
(`x2`, `y2`).

### `vdp_ellipse`, `vdp_filled_ellipse`

    void vdp_ellipse(int centre_x, int centre_y, int x1, int y1, int x2,
                     int y2);
    void vdp_filled_ellipse(int centre_x, int centre_y, int x1, int y1,
                            int x2, int y2);

An ellipse the way Acorn draws one: around the centre, as wide as `x1`
is from it (`y1` does not matter), with its top at (`x2`, `y2`). A top
not above the centre shears it. Each is PLOT 4 to the centre
and to (`x1`, `y1`), then PLOT &C5 (outline) or &CD (filled) to (`x2`,
`y2`).

### `vdp_copy_rectangle`, `vdp_move_rectangle`

    void vdp_copy_rectangle(int src_x1, int src_y1, int src_x2,
                            int src_y2, int dest_x, int dest_y);
    void vdp_move_rectangle(int src_x1, int src_y1, int src_x2,
                            int src_y2, int dest_x, int dest_y);

The rectangle between two corners, copied or moved so that its bottom
left is at (`dest_x`, `dest_y`); a move clears what it leaves behind.
Each is PLOT 4 to the two corners, then PLOT &BF (copy) or &BD (move)
to the destination.

### `vdp_fill_path`, `vdp_path_point`

    void vdp_fill_path(const int *path, int pathsize);
    void vdp_path_point(int x, int y);

A filled path. `vdp_path_point` sends PLOT &DD, `x;` `y;`: one point
added to the path that the last two points the cursor visited begin.
The VDP fills the path when the next thing it is sent is anything else.

`vdp_fill_path` takes `x`, `y` pairs and their size in bytes,
`sizeof path`. It sends PLOT 4 to the first two points and
`vdp_path_point` for each after. With fewer than three points
(`pathsize` under 18, an `int` being 3 bytes), or an odd number of
`int`s, it sends nothing. The size is counted unsigned, as in libagon,
so a negative one is a very large one.

    static const int star[] = {
        160, 200,  130, 40,  250, 140,  70, 140,  190, 40
    };

    vdp_fill_path(star, sizeof star);

### `vdp_plot_bitmap`

    void vdp_plot_bitmap(int x, int y);

PLOT &ED, `x;` `y;`: the current bitmap, at (`x`, `y`), in its own
colours; mode 7 (PLOT &EF) would draw it in the pen's.
Bitmaps are made and chosen with
[agon/vdp/bitmap.h](#agonvdpbitmaph).

## `agon/vdp/bitmap.h`

Bitmaps and sprites (VDU 23, 27), the mouse (VDU 23, 0, &89), a
character drawn as a bitmap (&92), fonts (&95), the affine transform
(&96), the tile engine (&C2), the copper (&C4) and contexts (&C8). A
program includes [agon/vdp.h](#agonvdph), which includes this.

Each call sends one VDU command (a few send two or three) in one call
into MOS, and none waits for an answer. The bytes are libagon's, name
for name, except where libagon sends what VDP 2.16.0 does not take;
those places are said below. The calls libagon does not have send what
the VDP's own source reads at v2.16.0.

In the sequences, a plain argument is one byte, its low byte when the
argument is wider; `x;` is two bytes, low first; `x(3)` is three.

An 8-bit bitmap number `n` is the bitmap in buffer 64000 + `n`. The
calls named `vdp_adv_` take the 16-bit buffer ID itself; buffers are in
[agon/vdp/buffer.h](#agonvdpbufferh).

### `VDP_VAR_AFFINE_TRANSFORM`, `VDP_VAR_HW_SPRITES`, `VDP_VAR_TILE_ENGINE`, `VDP_VAR_COPPER`

Parts of the VDP that are off until a program sets their VDP variable
(VDU 23, 0, &F8, id; value;, `vdp_set_variable` in
[agon/vdp/screen.h](#agonvdpscreenh)). While one is unset its
commands take no bytes at all: the VDP reads what follows as VDU codes.

| Name                       | ID      | Turns on                     |
|----------------------------|---------|------------------------------|
| `VDP_VAR_AFFINE_TRANSFORM` | &0001   | `vdp_adv_use_affine_matrix`  |
| `VDP_VAR_HW_SPRITES`       | &0002   | `vdp_set_hardware_sprite`    |
| `VDP_VAR_TILE_ENGINE`      | &0300   | the `vdp_tile` calls         |
| `VDP_VAR_COPPER`           | &0310   | the `vdp_copper` calls       |

`vdp_copper_enable` sets the copper's for you.

### `vdp_select_bitmap`

    void vdp_select_bitmap(int n);

VDU 23, 27, 0, n. Select bitmap `n` for the commands that follow.

### `vdp_load_bitmap`, `LOAD_BMAP_BLOCK`

    void vdp_load_bitmap(int width, int height, uint8_t *data);

VDU 23, 27, 1, width; height; and then `width * height * 4` bytes from
`data`: RGBA8888 pixels, into the selected bitmap. The pixels go to MOS
in pieces of at most `LOAD_BMAP_BLOCK` (65535) bytes, and none at all
when there are none.

### `vdp_load_bitmap_file`

    int vdp_load_bitmap_file(const char *fname, int width, int height);

`vdp_load_bitmap` with the pixels read from the file `fname`, 4 KB at a
time. The answer is 0, or -1 when the file cannot be opened or the 4 KB
buffer cannot be had (nothing is sent then), or when the file holds
fewer than `width * height * 4` bytes: the bytes it lacks are sent as
zeros all the same, since the VDP waits for every one.

### `vdp_solid_bitmap`

    void vdp_solid_bitmap(int width, int height, int r, int g, int b,
                          int a);

VDU 23, 27, 2, width; height; r, g, b, a. Make the selected bitmap
`width` by `height` of one colour, the four bytes of an RGBA8888 pixel.

### `vdp_draw_bitmap`

    void vdp_draw_bitmap(int x, int y);

VDU 23, 27, 3, x; y;. Draw the selected bitmap at (`x`, `y`).

### `vdp_capture_bitmap`, `vdp_adv_capture_bitmap`

    void vdp_capture_bitmap(uint16_t top, uint16_t left,
                            uint16_t bottom, uint16_t right,
                            uint8_t bitmapID);
    void vdp_adv_capture_bitmap(uint16_t top, uint16_t left,
                                uint16_t bottom, uint16_t right,
                                int bufferId);

Capture the screen between (`left`, `top`) and (`right`, `bottom`). The
VDP captures the rectangle between the last two graphics positions, so
both send two moves first, VDU 25, 4, left; top; and VDU 25, 4, right;
bottom;, and then:

| Call                     | Capture                           |
|--------------------------|-----------------------------------|
| `vdp_capture_bitmap`     | VDU 23, 27, 1, bitmapID; 0;       |
| `vdp_adv_capture_bitmap` | VDU 23, 27, &21, bufferId; 0;     |

Command 1 with a height of 0 is the capture, into the bitmap its width
names. libagon sends a single byte, &21, in place of that command; acc
sends the command. Command &21 with a height of 0 takes no format byte.

Note the order of the arguments: top before left.

### `vdp_adv_select_bitmap`

    void vdp_adv_select_bitmap(int bufferId);

VDU 23, 27, &20, bufferId;. Select the bitmap in buffer `bufferId`.

### `vdp_adv_bitmap_from_buffer`, `VDP_BITMAP_RGBA8888`, `VDP_BITMAP_RGBA2222`, `VDP_BITMAP_MONO`, `VDP_BITMAP_NATIVE`

    void vdp_adv_bitmap_from_buffer(int width, int height, int format);

VDU 23, 27, &21, width; height; format. Make the selected buffer a
bitmap of `width` by `height` pixels in `format`:

| Name                  | Value | Pixels                              |
|-----------------------|-------|-------------------------------------|
| `VDP_BITMAP_RGBA8888` | 0     | 4 bytes each                        |
| `VDP_BITMAP_RGBA2222` | 1     | a byte each                         |
| `VDP_BITMAP_MONO`     | 2     | 1 bit each, in the graphics colour  |
| `VDP_BITMAP_NATIVE`   | 3     | the VDP's own                       |

A `height` of 0 makes it a capture instead (as
`vdp_adv_capture_bitmap`), which reads no format byte, and then no
format is sent. libagon sends one all the same, and the VDP takes it
for a command of its own.

### `vdp_select_sprite`, `vdp_clear_sprite`, `vdp_add_sprite_bitmap`, `vdp_adv_add_sprite_bitmap`, `vdp_activate_sprites`

    void vdp_select_sprite(int n);
    void vdp_clear_sprite(void);
    void vdp_add_sprite_bitmap(int n);
    void vdp_adv_add_sprite_bitmap(int bitmap_num);
    void vdp_activate_sprites(int n);

| Call                        | Sends                            |
|-----------------------------|----------------------------------|
| `vdp_select_sprite`         | VDU 23, 27, 4, n                 |
| `vdp_clear_sprite`          | VDU 23, 27, 5                    |
| `vdp_add_sprite_bitmap`     | VDU 23, 27, 6, n                 |
| `vdp_adv_add_sprite_bitmap` | VDU 23, 27, &26, bitmap_num;     |
| `vdp_activate_sprites`      | VDU 23, 27, 7, n                 |

Select sprite `n`; clear the selected sprite's frames; add bitmap `n`
(or the bitmap in buffer `bitmap_num`) to it as a frame; and make `n`
sprites active, sprites 0 to `n - 1`.

### `vdp_next_sprite_frame`, `vdp_prev_sprite_frame`, `vdp_nth_sprite_frame`

    void vdp_next_sprite_frame(void);
    void vdp_prev_sprite_frame(void);
    void vdp_nth_sprite_frame(int n);

VDU 23, 27, 8; VDU 23, 27, 9; VDU 23, 27, 10, n. Show the selected
sprite's next frame, its previous one, or frame `n`.

### `vdp_replace_sprite_frame`, `vdp_adv_replace_sprite_frame`

    void vdp_replace_sprite_frame(uint8_t bitmapID);
    void vdp_adv_replace_sprite_frame(int bufferId);

VDU 23, 27, 21, bitmapID and VDU 23, 27, &35, bufferId;. Put a bitmap,
by number or by buffer ID, in place of the selected sprite's frame.

### `vdp_show_sprite`, `vdp_hide_sprite`, `vdp_move_sprite_to`, `vdp_move_sprite_by`

    void vdp_show_sprite(void);
    void vdp_hide_sprite(void);
    void vdp_move_sprite_to(int x, int y);
    void vdp_move_sprite_by(int x, int y);

| Call                 | Sends                      |
|----------------------|----------------------------|
| `vdp_show_sprite`    | VDU 23, 27, 11             |
| `vdp_hide_sprite`    | VDU 23, 27, 12             |
| `vdp_move_sprite_to` | VDU 23, 27, 13, x; y;      |
| `vdp_move_sprite_by` | VDU 23, 27, 14, x; y;      |

Each acts on the selected sprite.

### `vdp_refresh_sprites`, `vdp_reset_sprites`, `vdp_reset_sprites_only`

    void vdp_refresh_sprites(void);
    void vdp_reset_sprites(void);
    void vdp_reset_sprites_only(void);

VDU 23, 27, 15 refreshes the sprites. VDU 23, 27, 16 resets the sprites,
the bitmaps and the screen; VDU 23, 27, 17 the sprites alone.

### `vdp_set_sprite_paint_mode`, `vdp_set_hardware_sprite`, `vdp_set_software_sprite`

    void vdp_set_sprite_paint_mode(int n);
    void vdp_set_hardware_sprite(void);
    void vdp_set_software_sprite(void);

VDU 23, 27, 18, n sets the selected sprite's paint mode, a GCOL mode.
VDU 23, 27, 19 makes it a hardware sprite, which the VDP takes only once
`VDP_VAR_HW_SPRITES` is set; VDU 23, 27, 20 a software one.

### `vdp_load_sprite_bitmaps`, `vdp_adv_load_sprite_bitmaps`

    int vdp_load_sprite_bitmaps(const char *fname_prefix,
                                const char *fname_format,
                                int width, int height, int num,
                                int bitmap_num);
    int vdp_adv_load_sprite_bitmaps(const char *fname_prefix,
                                    const char *fname_format,
                                    int width, int height, int num,
                                    int bitmap_num);

Load `num` RGBA8888 bitmaps of `width` by `height` from files, frame
`i` from the file named by `sprintf(name, fname_format, fname_prefix,
i)` (at most 255 characters), into bitmap `bitmap_num + i`: selected
with `vdp_select_bitmap`, or for the `_adv_` one with
`vdp_adv_select_bitmap` (a buffer ID), and then `vdp_load_bitmap`.

The answer is how many were loaded. It stops at the first file that
cannot be opened or is short, as libagon's does, and is 0 when the
`width * height * 4`-byte buffer cannot be had.

    n = vdp_load_sprite_bitmaps("ship", "%s%d.rgba", 16, 16, 4, 0);

### `vdp_create_sprite`, `vdp_adv_create_sprite`

    void vdp_create_sprite(int sprite, int bitmap_num, int frames);
    void vdp_adv_create_sprite(int sprite, int bitmap_num, int frames);

Select `sprite`, clear it, add `frames` frames from bitmap `bitmap_num`
onwards (`vdp_add_sprite_bitmap`, or `vdp_adv_add_sprite_bitmap` with
buffer IDs), and then `vdp_activate_sprites(sprite)`.

That last is libagon's arithmetic: it activates `sprite` sprites, which
leaves the sprite itself out. Activate `sprite + 1` afterwards to show
it.

    vdp_create_sprite(0, 0, 4);
    vdp_activate_sprites(1);
    vdp_show_sprite();

### `vdp_mouse_enable`, `vdp_mouse_disable`, `vdp_mouse_reset`

    void vdp_mouse_enable(void);
    void vdp_mouse_disable(void);
    void vdp_mouse_reset(void);

VDU 23, 0, &89, 0; VDU 23, 0, &89, 1; VDU 23, 0, &89, 2.

### `vdp_mouse_set_cursor`, `vdp_mouse_set_position`, `vdp_mouse_set_area`, `vdp_mouse_set_bitmap`

    void vdp_mouse_set_cursor(int cursorId);
    void vdp_mouse_set_position(int X, int Y);
    void vdp_mouse_set_area(int x1, int y1, int x2, int y2);
    void vdp_mouse_set_bitmap(int hotx, int hoty);

| Call                     | Sends                                   |
|--------------------------|-----------------------------------------|
| `vdp_mouse_set_cursor`   | VDU 23, 0, &89, 3, cursorId;            |
| `vdp_mouse_set_position` | VDU 23, 0, &89, 4, X; Y;                |
| `vdp_mouse_set_area`     | VDU 23, 0, &89, 5, x1; y1; x2; y2;      |
| `vdp_mouse_set_bitmap`   | VDU 23, 27, &40, hotx, hoty             |

VDP 2.16.0 reads the area's four corners and does nothing with them.
`vdp_mouse_set_bitmap` makes the selected bitmap the cursor, with its
hot spot at (`hotx`, `hoty`).

### `vdp_mouse_sample_rate`, `vdp_mouse_resolution`, `vdp_mouse_scaling`, `vdp_mouse_acceleration`, `vdp_mouse_wheel_accel`

    void vdp_mouse_sample_rate(int sampleRate);
    void vdp_mouse_resolution(int resolution);
    void vdp_mouse_scaling(int scaling);
    void vdp_mouse_acceleration(int acceleration);
    void vdp_mouse_wheel_accel(int wheelAccel);

| Call                     | Sends                               |
|--------------------------|-------------------------------------|
| `vdp_mouse_sample_rate`  | VDU 23, 0, &89, 6, sampleRate       |
| `vdp_mouse_resolution`   | VDU 23, 0, &89, 7, resolution       |
| `vdp_mouse_scaling`      | VDU 23, 0, &89, 8, scaling          |
| `vdp_mouse_acceleration` | VDU 23, 0, &89, 9, acceleration;    |
| `vdp_mouse_wheel_accel`  | VDU 23, 0, &89, 10, wheelAccel(3)   |

### `vdp_map_char_to_bitmap`

    void vdp_map_char_to_bitmap(int char_num, int bitmap_num);

VDU 23, 0, &92, char_num, bitmap_num;. Draw the bitmap in buffer
`bitmap_num` in place of character `char_num`.

### `vdp_font_select`, `VDP_FONT_SELECT_ADJUSTBASE`

    void vdp_font_select(int buffer_id, int flags);

VDU 23, 0, &95, 0, buffer_id; flags. Use the font in buffer
`buffer_id`, the buffer that holds its glyphs; 65535 is the system
font. `VDP_FONT_SELECT_ADJUSTBASE` (1) is the one flag named.

### `vdp_font_create`

    void vdp_font_create(int buffer_id, int width, int height,
                         int ascent, int flags);

VDU 23, 0, &95, 1, buffer_id; width, height, ascent, flags. Make the
glyphs in buffer `buffer_id` a font.

### `vdp_font_adjust`, `vdp_font_set_property`, `VDP_FONT_INFO_WIDTH`

    void vdp_font_adjust(int buffer_id, int field, int value);
    void vdp_font_set_property(int buffer_id, int field, int value);

VDU 23, 0, &95, 2, buffer_id; field, value;. Set one field of a font.
`vdp_font_set_property` is libagon's other name for it, which libagon's
header leaves out.

The value is 16 bits. libagon sends only its low byte, and the VDP then
takes the next command's first byte for the high one; acc sends both.

| Name                        | `field` |
|-----------------------------|---------|
| `VDP_FONT_INFO_WIDTH`       | 0       |
| `VDP_FONT_INFO_HEIGHT`      | 1       |
| `VDP_FONT_INFO_ASCENT`      | 2       |
| `VDP_FONT_INFO_FLAGS`       | 3       |
| `VDP_FONT_INFO_CHARPTRS`    | 4       |
| `VDP_FONT_INFO_POINTSIZE`   | 5       |
| `VDP_FONT_INFO_INLEADING`   | 6       |
| `VDP_FONT_INFO_EXLEADING`   | 7       |
| `VDP_FONT_INFO_WEIGHT`      | 8       |
| `VDP_FONT_INFO_CHARSET`     | 9       |
| `VDP_FONT_INFO_CODEPAGE`    | 10      |

### `vdp_font_delete`, `vdp_font_copy`, `vdp_font_debug`

    void vdp_font_delete(int buffer_id);
    void vdp_font_copy(int buffer_id);
    void vdp_font_debug(int buffer_id);

VDU 23, 0, &95, 4, buffer_id; deletes a font; buffer 65535 clears every
font. VDU 23, 0, &95, 5, buffer_id; is the copy, and VDU 23, 0, &95,
&20, buffer_id; writes the font to the VDP's debug log.

### `vdp_adv_use_affine_matrix`

    void vdp_adv_use_affine_matrix(uint8_t flags, uint16_t affineID);

VDU 23, 0, &96, flags, affineID;. With bit 0 of `flags` set, bitmaps are
drawn through the affine matrix in buffer `affineID`. The VDP takes it
only once `VDP_VAR_AFFINE_TRANSFORM` is set. The matrices are made by
the commands in [agon/vdp/buffer.h](#agonvdpbufferh).

### `vdp_tilebank_init`, `vdp_tilebank_load`, `vdp_tilebank_draw`, `vdp_tilebank_free`

    void vdp_tilebank_init(int bank, int bit_depth);
    void vdp_tilebank_load(int bank, int tile_id,
                           const uint8_t *pixels);
    void vdp_tilebank_draw(int bank, int tile_id, int palette, int x,
                           int y, int x_offset, int y_offset,
                           int attribute);
    void vdp_tilebank_free(int bank);

The tile engine, VDU 23, 0, &C2, command, which the VDP takes only once
`VDP_VAR_TILE_ENGINE` is set. Tiles are 8 by 8, a byte a pixel. Every
argument is a byte; the zeros are the VDP's reserved arguments, which it
reads and ignores.

| Call                | Sends (after VDU 23, 0, &C2)                  |
|---------------------|-----------------------------------------------|
| `vdp_tilebank_init` | &00, bank, bit_depth, 0, 0                    |
| `vdp_tilebank_load` | &01, bank, tile_id, then 64 bytes of pixels   |
| `vdp_tilebank_draw` | &06, bank, tile_id, palette, x, y, x_offset,  |
|                     | y_offset, attribute                           |
| `vdp_tilebank_free` | &07, bank                                     |

Make a bank with `vdp_tilebank_init` before loading tiles into it, or
the VDP takes the pixels for commands.

### `vdp_tilemap_init`, `vdp_tilemap_set_tile`, `vdp_tilemap_free`

    void vdp_tilemap_init(int layer, int size);
    void vdp_tilemap_set_tile(int layer, int x, int y, int tile_id,
                              int attribute);
    void vdp_tilemap_free(int layer);

| Call                   | Sends (after VDU 23, 0, &C2)               |
|------------------------|--------------------------------------------|
| `vdp_tilemap_init`     | &10, layer, size, 0, 0                     |
| `vdp_tilemap_set_tile` | &11, layer, x, y, tile_id, attribute       |
| `vdp_tilemap_free`     | &17, layer                                 |

### `vdp_tilelayer_init`, `vdp_tilelayer_set_scroll`, `vdp_tilelayer_update_layerbuffer`, `vdp_tilelayer_draw_layerbuffer`, `vdp_tilelayer_draw`, `vdp_tilelayer_free`

    void vdp_tilelayer_init(int layer, int layer_size, int tile_size);
    void vdp_tilelayer_set_scroll(int layer, int x, int y, int x_offset,
                                  int y_offset);
    void vdp_tilelayer_update_layerbuffer(int layer);
    void vdp_tilelayer_draw_layerbuffer(int layer);
    void vdp_tilelayer_draw(int layer);
    void vdp_tilelayer_free(int layer);

| Call                         | Sends (after VDU 23, 0, &C2)         |
|------------------------------|--------------------------------------|
| `vdp_tilelayer_init`         | &18, layer, layer_size, tile_size, 0 |
| `vdp_tilelayer_set_scroll`   | &1A, layer, x, y, x_offset, y_offset |
| `..._update_layerbuffer`     | &1C, layer                           |
| `..._draw_layerbuffer`       | &1D, layer                           |
| `vdp_tilelayer_draw`         | &1E, layer                           |
| `vdp_tilelayer_free`         | &1F, layer                           |

### `vdp_copper_enable`, `vdp_copper_disable`

    void vdp_copper_enable(void);
    void vdp_copper_disable(void);

The copper changes palettes as the screen is drawn. `vdp_copper_enable`
sets its VDP variable, VDU 23, 0, &F8, &0310; 0;, and
`vdp_copper_disable` clears it, VDU 23, 0, &F9, &0310;. The VDP asks
only that the variable be there; libagon sets it to 0. The other copper
calls take no bytes until it is enabled.

### `vdp_copper_create_palette`, `vdp_copper_delete_palette`, `vdp_copper_set_palette_entry`, `vdp_copper_set_signal_list`, `vdp_copper_reset_signal_list`

    void vdp_copper_create_palette(uint16_t paletteID);
    void vdp_copper_delete_palette(uint16_t paletteID);
    void vdp_copper_set_palette_entry(uint16_t paletteID, uint8_t indx,
                                      uint8_t red, uint8_t green,
                                      uint8_t blue);
    void vdp_copper_set_signal_list(uint16_t bufferID);
    void vdp_copper_reset_signal_list(void);

| Call                     | Sends (after VDU 23, 0, &C4)            |
|--------------------------|-----------------------------------------|
| `..._create_palette`     | 0, paletteID;                           |
| `..._delete_palette`     | 1, paletteID;                           |
| `..._set_palette_entry`  | 2, paletteID; indx, red, green, blue    |
| `..._set_signal_list`    | 3, bufferID;                            |
| `..._reset_signal_list`  | 4                                       |

### `vdp_context_select`, `vdp_context_delete`, `vdp_context_save`, `vdp_context_restore`, `vdp_context_save_copy`, `vdp_context_restore_all`, `vdp_context_clear_stack`, `vdp_context_debug`

    void vdp_context_select(int context_id);
    void vdp_context_delete(int context_id);
    void vdp_context_save(void);
    void vdp_context_restore(void);
    void vdp_context_save_copy(int context_id);
    void vdp_context_restore_all(void);
    void vdp_context_clear_stack(void);
    void vdp_context_debug(void);

Contexts are stacks of the VDP's drawing state.

| Call                      | Sends (after VDU 23, 0, &C8)  |
|---------------------------|-------------------------------|
| `vdp_context_select`      | 0, context_id                 |
| `vdp_context_delete`      | 1, context_id                 |
| `vdp_context_reset`       | 2, flags                      |
| `vdp_context_save`        | 3                             |
| `vdp_context_restore`     | 4                             |
| `vdp_context_save_copy`   | 5, context_id                 |
| `vdp_context_restore_all` | 6                             |
| `vdp_context_clear_stack` | 7                             |
| `vdp_context_debug`       | &80, to the VDP's debug log   |

### `vdp_context_reset`, `VDP_CONTEXT_RESET_GPAINT`

    void vdp_context_reset(int flags);

VDU 23, 0, &C8, 2, flags. Reset the parts of the context that `flags`
names:

| Name                            | Bit   | Resets                   |
|---------------------------------|-------|--------------------------|
| `VDP_CONTEXT_RESET_GPAINT`      | &01   | graphics paint           |
| `VDP_CONTEXT_RESET_GPOS`        | &02   | graphics position        |
| `VDP_CONTEXT_RESET_TPAINT`      | &04   | text paint               |
| `VDP_CONTEXT_RESET_TCURSOR`     | &08   | text cursor              |
| `VDP_CONTEXT_RESET_TBEHAVIOUR`  | &10   | text behaviour           |
| `VDP_CONTEXT_RESET_FONTS`       | &20   | fonts                    |
| `VDP_CONTEXT_RESET_CHAR2BITMAP` | &40   | characters mapped to     |
|                                 |       | bitmaps                  |

## `agon/vdp/buffer.h`

The buffered commands: VDU 23, 0, &A0, bufferId; command, arguments. A
buffer is a list of blocks of bytes the VDP keeps under a 16-bit ID; it
can hold VDU commands to call later, bitmap or sample data, or a
matrix. A program includes [agon/vdp.h](#agonvdph), which includes
this.

The first part is libagon's, name for name, sending libagon's bytes
except where libagon sends what VDP 2.16.0 does not take; those places
are said below. The rest is every other form VDP 2.16.0 takes. Many
commands lay out their arguments by flags in their first argument
byte: for each of those there is a function taking the flag byte and
the rest as bytes (the `_args` ones) beside typed functions for the
common forms, and `vdp_adv_command` sends any command at all. None
waits for an answer.

In the sequences, a plain argument is one byte, its low byte when the
argument is wider; `x;` is two bytes, low first; `x(3)` is three. Every
sequence starts VDU 23, 0, &A0, bufferId;, written `A0 id;` below. A
command whose length depends on its arguments goes out in more than one
piece; the VDP reads a stream and cannot tell.

An advanced offset, which the jump and call offsets always are and the
others are when their `ADVANCED_OFFSETS` flag is set, is 24 bits. With
its top bit set (`VDP_OFFSET_BLOCK`, &800000) a 16-bit block number
follows it, and the offset counts from the start of that block.
Otherwise offsets and counts are 16 bits.

A list of buffers ends with 65535.

### `VDP_BUFFERED_WRITE`, `VDP_BUFFERED_CALL`, `VDP_OFFSET_BLOCK`

The command byte, after bufferId;.

| Name                                | Value | Function                  |
|-------------------------------------|-------|---------------------------|
| `VDP_BUFFERED_WRITE`                | &00   | `vdp_adv_write_block`     |
| `VDP_BUFFERED_CALL`                 | &01   | `vdp_adv_call_buffer`     |
| `VDP_BUFFERED_CLEAR`                | &02   | `vdp_adv_clear_buffer`    |
| `VDP_BUFFERED_CREATE`               | &03   | `vdp_adv_create`          |
| `VDP_BUFFERED_SET_OUTPUT`           | &04   | `vdp_adv_stream`          |
| `VDP_BUFFERED_ADJUST`               | &05   | `vdp_adv_adjust`          |
| `VDP_BUFFERED_COND_CALL`            | &06   | `vdp_adv_call_if`         |
| `VDP_BUFFERED_JUMP`                 | &07   | `vdp_adv_jump_buffer`     |
| `VDP_BUFFERED_COND_JUMP`            | &08   | `vdp_adv_jump_if`         |
| `VDP_BUFFERED_OFFSET_JUMP`          | &09   | `vdp_adv_jump_offset`     |
| `VDP_BUFFERED_OFFSET_COND_JUMP`     | &0A   | `vdp_adv_jump_offset_if`  |
| `VDP_BUFFERED_OFFSET_CALL`          | &0B   | `vdp_adv_call_offset`     |
| `VDP_BUFFERED_OFFSET_COND_CALL`     | &0C   | `vdp_adv_call_offset_if`  |
| `VDP_BUFFERED_COPY`                 | &0D   | `vdp_adv_copy_multiple`   |
| `VDP_BUFFERED_CONSOLIDATE`          | &0E   | `vdp_adv_consolidate`     |
| `VDP_BUFFERED_SPLIT`                | &0F   | `vdp_adv_split`           |
| `VDP_BUFFERED_SPLIT_INTO`           | &10   | `vdp_adv_split_multiple`  |
| `VDP_BUFFERED_SPLIT_FROM`           | &11   | `..._split_multiple_from` |
| `VDP_BUFFERED_SPLIT_BY`             | &12   | `vdp_adv_split_by_width`  |
| `VDP_BUFFERED_SPLIT_BY_INTO`        | &13   | `..._by_width_multiple`   |
| `VDP_BUFFERED_SPLIT_BY_FROM`        | &14   | `..._multiple_from`       |
| `VDP_BUFFERED_SPREAD_INTO`          | &15   | `vdp_adv_spread_multiple` |
| `VDP_BUFFERED_SPREAD_FROM`          | &16   | `..._spread_multiple_from`|
| `VDP_BUFFERED_REVERSE_BLOCKS`       | &17   | `..._reverse_block_order` |
| `VDP_BUFFERED_REVERSE`              | &18   | `..._reverse_block_data`  |
| `VDP_BUFFERED_COPY_REF`             | &19   | `..._by_reference`        |
| `VDP_BUFFERED_COPY_AND_CONSOLIDATE` | &1A   | `..._multiple_consolidate`|
| `VDP_BUFFERED_AFFINE_TRANSFORM`     | &20   | `vdp_adv_affine_float`    |
| `VDP_BUFFERED_AFFINE_TRANSFORM_3D`  | &21   | `vdp_adv_affine_3d_float` |
| `VDP_BUFFERED_MATRIX`               | &22   | `vdp_adv_matrix_float`    |
| `VDP_BUFFERED_TRANSFORM_BITMAP`     | &28   | `..._transform_bitmap`    |
| `VDP_BUFFERED_TRANSFORM_DATA`       | &29   | `vdp_adv_transform_data`  |
| `VDP_BUFFERED_READ_VARIABLE`        | &30   | `vdp_adv_read_variable`   |
| `VDP_BUFFERED_COMPRESS`             | &40   | `..._compress_buffer`     |
| `VDP_BUFFERED_DECOMPRESS`           | &41   | `..._decompress_buffer`   |
| `VDP_BUFFERED_EXPAND_BITMAP`        | &48   | `vdp_adv_expand_bitmap`   |
| `VDP_BUFFERED_ADD_CALLBACK`         | &50   | `vdp_adv_add_callback`    |
| `VDP_BUFFERED_REMOVE_CALLBACK`      | &51   | `vdp_adv_remove_callback` |
| `VDP_BUFFERED_DEBUG_INFO`           | &80   | `vdp_adv_debug_info`      |

The VDP takes the commands from &20 to &29 only once
`VDP_VAR_AFFINE_TRANSFORM` is set (VDU 23, 0, &F8, 1; 1;, see
[agon/vdp/bitmap.h](#agonvdpbitmaph)); until then it reads the rest
of the command as VDU codes.

### `vdp_adv_command`

    void vdp_adv_command(int bufferId, int command, const void *args,
                         int length);

A0 id; command, then the `length` bytes at `args` (none when `length`
is 0 or less). Any command at all.

### `vdp_adv_write_block`, `vdp_adv_write_block_data`

    void vdp_adv_write_block(int bufferID, int length);
    void vdp_adv_write_block_data(int bufferID, int length, char *data);

A0 id; 0, length;. Add a block of `length` bytes to the buffer.
`vdp_adv_write_block_data` sends the `length` bytes at `data` after it;
`vdp_adv_write_block` announces the block and leaves its bytes to the
caller, which must send exactly that many next.

    static const char beep[] = { 7 };

    vdp_adv_clear_buffer(1);
    vdp_adv_write_block_data(1, sizeof beep, (char *) beep);
    vdp_adv_call_buffer(1);

### `vdp_adv_call_buffer`, `vdp_adv_jump_buffer`

    void vdp_adv_call_buffer(int bufferID);
    void vdp_adv_jump_buffer(int bufferID);

A0 id; 1 and A0 id; 7. Run the VDU commands in the buffer. A jump from
the top level, where a program's commands run, is a call; a jump to
65535 goes to the end of the buffer running.

### `vdp_adv_clear_buffer`

    void vdp_adv_clear_buffer(int bufferID);

A0 id; 2. Clear the buffer. 65535 clears every buffer, and every
bitmap, font and sample made from one.

### `vdp_adv_create`, `vdp_adv_stream`

    void vdp_adv_create(int bufferID, int length);
    void vdp_adv_stream(int bufferID);

`vdp_adv_create` sends A0 id; 3, length;: make a buffer of `length`
bytes. `vdp_adv_stream` sends A0 id; 4: where the VDP's output
goes, a buffer `vdp_adv_create` made, 0 for back to MOS, or 65535 for
nowhere.

### `vdp_adv_adjust`, `vdp_adv_adjust_args`, `VDP_ADJUST_SET`

    void vdp_adv_adjust(int bufferID, int operation, int offset);
    void vdp_adv_adjust_args(int bufferId, int operation,
                             const void *args, int length);

Command 5 changes bytes in the buffer. The operation is in the low four
bits, flags above:

| Name                          | Value | Meaning                       |
|-------------------------------|-------|-------------------------------|
| `VDP_ADJUST_NOT`              | &00   | not; no operand               |
| `VDP_ADJUST_NEG`              | &01   | negate; no operand            |
| `VDP_ADJUST_SET`              | &02   | set                           |
| `VDP_ADJUST_ADD`              | &03   | add                           |
| `VDP_ADJUST_ADD_CARRY`        | &04   | add with carry                |
| `VDP_ADJUST_AND`              | &05   | and                           |
| `VDP_ADJUST_OR`               | &06   | or                            |
| `VDP_ADJUST_XOR`              | &07   | exclusive or                  |
| `VDP_ADJUST_OP_MASK`          | &0F   | the operation's bits          |
| `VDP_ADJUST_ADVANCED_OFFSETS` | &10   | 24-bit offset and count       |
| `VDP_ADJUST_BUFFER_VALUE`     | &20   | the operand from a buffer     |
| `VDP_ADJUST_MULTI_TARGET`     | &40   | a count of target bytes       |
| `VDP_ADJUST_MULTI_OPERAND`    | &80   | a count of operand bytes      |

The VDP reads, in order: the offset; a count when either MULTI flag is
set; an operand buffer and an offset into it with BUFFER_VALUE; then,
when the operands are not from a buffer, the operand bytes, one or
(with MULTI_OPERAND) count of them.

`vdp_adv_adjust` is libagon's: A0 id; 5, operation, offset;, and no
more. An operation that takes an operand (SET and above) leaves it to
the caller. `vdp_adv_adjust_args` sends A0 id; 5, operation and the
`length` bytes at `args`.

### `vdp_adv_adjust_value`, `vdp_adv_adjust_multi`, `vdp_adv_adjust_multi_data`, `vdp_adv_adjust_buffer`

    void vdp_adv_adjust_value(int bufferId, int operation, int offset,
                              int value);
    void vdp_adv_adjust_multi(int bufferId, int operation, int offset,
                              int count, int value);
    void vdp_adv_adjust_multi_data(int bufferId, int operation,
                                   int offset, int count,
                                   const void *operands);
    void vdp_adv_adjust_buffer(int bufferId, int operation, int offset,
                               int operandBufferId, int operandOffset);

Whole adjust commands. Each sends A0 id; 5, operation, then the offset,
and the count where there is one, as 16 bits, or 24 with
`VDP_ADJUST_ADVANCED_OFFSETS` in `operation`. An operand goes only with
the operations that take one (SET and above).

| Call                   | Flag added   | Then                         |
|------------------------|--------------|------------------------------|
| `..._adjust_value`     | none         | value                        |
| `..._adjust_multi`     | MULTI_TARGET | count, value                 |
| `..._adjust_multi_data`| MULTI_OPERAND| count, `count` operand bytes |
| `..._adjust_buffer`    | BUFFER_VALUE | operandBufferId;,            |
|                        |              | operandOffset                |

    /* add 1 to byte 4 of buffer 10 */
    vdp_adv_adjust_value(10, VDP_ADJUST_ADD, 4, 1);

### `vdp_adv_call_conditional`, `vdp_adv_jump_conditional`, `VDP_COND_EQUAL`

    void vdp_adv_call_conditional(int bufferId, int operation,
                                  int checkBufferId, int checkOffset);
    void vdp_adv_jump_conditional(int bufferId, int operation,
                                  int checkBufferId, int checkOffset);

libagon's: A0 id; 6 (call) or 8 (jump), operation, checkBufferId;,
checkOffset;. That is the whole command for EXISTS and NOT_EXISTS; the
other conditions take an operand the caller sends after. (libagon's
header declares `vdp_adv_call_conditional` twice and
`vdp_adv_jump_conditional` not at all; its library has both.)

The condition byte of commands 6, 8, &0A and &0C:

| Name                        | Value | Meaning                   |
|-----------------------------|-------|---------------------------|
| `VDP_COND_EXISTS`           | &00   | exists; no operand        |
| `VDP_COND_NOT_EXISTS`       | &01   | does not; no operand      |
| `VDP_COND_EQUAL`            | &02   | =                         |
| `VDP_COND_NOT_EQUAL`        | &03   | !=                        |
| `VDP_COND_LESS`             | &04   | <                         |
| `VDP_COND_GREATER`          | &05   | >                         |
| `VDP_COND_LESS_EQUAL`       | &06   | <=                        |
| `VDP_COND_GREATER_EQUAL`    | &07   | >=                        |
| `VDP_COND_AND`              | &08   | and                       |
| `VDP_COND_OR`               | &09   | or                        |
| `VDP_COND_OP_MASK`          | &0F   | the condition's bits      |
| `VDP_COND_ADVANCED_OFFSETS` | &10   | 24-bit check offset       |
| `VDP_COND_BUFFER_VALUE`     | &20   | the operand from a buffer |
| `VDP_COND_VAR_VALUE`        | &40   | check a VDP variable      |
| `VDP_COND_16BIT`            | &80   | a 16-bit operand          |

### `vdp_adv_call_if`, `vdp_adv_jump_if`, `vdp_adv_call_if_args`, `vdp_adv_jump_if_args`

    void vdp_adv_call_if(int bufferId, int operation, int checkBufferId,
                         int checkOffset, int operand);
    void vdp_adv_jump_if(int bufferId, int operation, int checkBufferId,
                         int checkOffset, int operand);
    void vdp_adv_call_if_args(int bufferId, int operation,
                              const void *args, int length);
    void vdp_adv_jump_if_args(int bufferId, int operation,
                              const void *args, int length);

A whole conditional call (command 6) or jump (8). `vdp_adv_call_if` and
`vdp_adv_jump_if` send A0 id; 6 or 8, operation, checkBufferId; (a
variable's ID with `VDP_COND_VAR_VALUE`), then the check offset, 16 or
24 bits by `VDP_COND_ADVANCED_OFFSETS` and none for a variable, then
the operand for the conditions that take one (EQUAL and above): a byte,
or `operand;` with `VDP_COND_16BIT`.

An operand from a buffer, or an offset with a block number, goes
through the `_args` forms, which send A0 id; 6 or 8, operation, and the
`length` bytes at `args`.

    /* call buffer 2 when byte 0 of buffer 3 is 5 */
    vdp_adv_call_if(2, VDP_COND_EQUAL, 3, 0, 5);

### `vdp_adv_jump_offset`, `vdp_adv_call_offset`, `vdp_adv_jump_offset_block`, `vdp_adv_call_offset_block`

    void vdp_adv_jump_offset(int bufferId, int offset);
    void vdp_adv_call_offset(int bufferId, int offset);
    void vdp_adv_jump_offset_block(int bufferId, int offset, int block);
    void vdp_adv_call_offset_block(int bufferId, int offset, int block);

Jump (command 9) or call (&0B) into a buffer at an offset, which is
always an advanced one.

`vdp_adv_jump_offset` and `vdp_adv_call_offset` send A0 id; 9 or &0B,
offset(3). With `VDP_OFFSET_BLOCK` set in `offset` they send nothing at
all, since the VDP would wait for a block number.

The `_block` forms send A0 id; 9 or &0B, (offset | `VDP_OFFSET_BLOCK`)(3),
block;. acc sets the bit for the caller; libagon's send nothing unless
it is set, so block 2 at offset 0 does nothing there. libagon's block
calls also all send command 9, whatever they are; here each sends its
own.

### `vdp_adv_jump_offset_conditional`, `vdp_adv_call_offset_conditional`, `vdp_adv_jump_offset_block_conditional`, `vdp_adv_call_offset_block_conditional`

    void vdp_adv_jump_offset_conditional(int bufferId, int offset);
    void vdp_adv_call_offset_conditional(int bufferId, int offset);
    void vdp_adv_jump_offset_block_conditional(int bufferId,
                                               int offset, int block);
    void vdp_adv_call_offset_block_conditional(int bufferId,
                                               int offset, int block);

As the four above with commands &0A (jump) and &0C (call): they send the
offset, and the block, and leave the condition to the caller, as
libagon does. `vdp_adv_jump_offset_if` and `vdp_adv_call_offset_if`
send it too.

### `vdp_adv_jump_offset_if`, `vdp_adv_call_offset_if`

    void vdp_adv_jump_offset_if(int bufferId, int offset,
                                int operation, int checkBufferId,
                                int checkOffset, int operand);
    void vdp_adv_call_offset_if(int bufferId, int offset,
                                int operation, int checkBufferId,
                                int checkOffset, int operand);

A0 id; &0A (jump) or &0C (call), offset(3), then the condition as
`vdp_adv_call_if` sends it. No block number is sent, so `offset` must
not have `VDP_OFFSET_BLOCK` set; for that, use `vdp_adv_command`.

### `vdp_adv_copy_multiple`, `vdp_adv_copy_multiple_by_reference`, `vdp_adv_copy_multiple_consolidate`

    void vdp_adv_copy_multiple(int bufferId, int num_buffers, ...);
    void vdp_adv_copy_multiple_by_reference(int bufferId,
                                            int num_buffers, ...);
    void vdp_adv_copy_multiple_consolidate(int bufferId,
                                           int num_buffers, ...);

A0 id; &0D, &19 or &1A, then the `num_buffers` IDs that follow, each
`int` sent as 16 bits, and 65535;. Copy the blocks of those buffers
into `bufferId`; &19 copies by reference, &1A consolidates the copy.

    vdp_adv_copy_multiple(20, 3, 1, 2, 3);

### `vdp_adv_consolidate`, `vdp_adv_split`, `vdp_adv_split_multiple`, `vdp_adv_split_multiple_from`

    void vdp_adv_consolidate(int bufferID);
    void vdp_adv_split(int bufferID, int blockSize);
    void vdp_adv_split_multiple(int bufferId, int blockSize,
                                int num_buffers, ...);
    void vdp_adv_split_multiple_from(int bufferID, int blockSize,
                                     int targetBufferID);

| Call                     | Sends (after A0 id;)                     |
|--------------------------|------------------------------------------|
| `vdp_adv_consolidate`    | &0E                                      |
| `vdp_adv_split`          | &0F, blockSize;                          |
| `..._split_multiple`     | &10, blockSize;, the IDs;, 65535;        |
| `..._split_multiple_from`| &11, blockSize;, targetBufferID;         |

Split the buffer into blocks of `blockSize`: in place, into the
buffers listed, or from `targetBufferID`.

### `vdp_adv_split_by_width`, `vdp_adv_split_by_width_multiple`, `vdp_adv_split_by_width_multiple_from`

    void vdp_adv_split_by_width(int bufferID, int width,
                                int blockCount);
    void vdp_adv_split_by_width_multiple(int bufferId, int width,
                                         int num_buffers, ...);
    void vdp_adv_split_by_width_multiple_from(int bufferID, int width,
                                              int blockCount,
                                              int targetBufferID);

| Call                     | Sends (after A0 id;)                     |
|--------------------------|------------------------------------------|
| `..._split_by_width`     | &12, width;, blockCount;                 |
| `..._by_width_multiple`  | &13, width;, the IDs;, 65535;            |
| `..._multiple_from`      | &14, width;, blockCount;, targetBufferID;|

### `vdp_adv_spread_multiple`, `vdp_adv_spread_multiple_from`

    void vdp_adv_spread_multiple(int bufferId, int num_buffers, ...);
    void vdp_adv_spread_multiple_from(int bufferID, int targetBufferID);

A0 id; &15, the IDs;, 65535; and A0 id; &16, targetBufferID;. Spread the
buffer's blocks into the buffers listed, or from `targetBufferID`.

### `vdp_adv_reverse_block_order`, `vdp_adv_reverse_block_data`, `VDP_REVERSE_16BIT`

    void vdp_adv_reverse_block_order(int bufferID);
    void vdp_adv_reverse_block_data(int bufferID, int options,
                                    int valueSize, int chunkSize);

`vdp_adv_reverse_block_order` sends A0 id; &17: the blocks in reverse
order. `vdp_adv_reverse_block_data` sends A0 id; &18, options, then
valueSize; only when both size bits are set, and chunkSize; only with
`VDP_REVERSE_CHUNKED`.

| Name                      | Value | Option                          |
|---------------------------|-------|---------------------------------|
| `VDP_REVERSE_16BIT`       | &01   | 16-bit values                   |
| `VDP_REVERSE_32BIT`       | &02   | 32-bit values                   |
| `VDP_REVERSE_SIZE`        | &03   | both: a value size follows      |
| `VDP_REVERSE_CHUNKED`     | &04   | a chunk size follows            |
| `VDP_REVERSE_BLOCK`       | &08   | block                           |
| `VDP_REVERSE_UNUSED_BITS` | &F0   | unused                          |

libagon decides otherwise: it sends a value size for options 1 and 2 as
well, both sizes for options 0, and neither for any options above 4,
chunked or not, each of which puts the VDP out of step.

### `vdp_adv_compress_buffer`, `vdp_adv_decompress_buffer`

    void vdp_adv_compress_buffer(int targetBufferID,
                                 int sourceBufferId);
    void vdp_adv_decompress_buffer(int targetBufferID,
                                   int sourceBufferId);

A0 targetBufferID; &40 or &41, sourceBufferId;.

### `vdp_adv_create_rotated_buffer`, `vdp_adv_create_scaled_buffer`, `vdp_adv_create_translated_buffer`, `vdp_adv_create_skewed_buffer`

    void vdp_adv_create_rotated_buffer(uint16_t transformID,
                                       uint16_t angle);
    void vdp_adv_create_scaled_buffer(uint16_t transformID,
                                      float scaleX, float scaleY);
    void vdp_adv_create_translated_buffer(uint16_t transformID,
                                          int16_t x, int16_t y);
    void vdp_adv_create_skewed_buffer(uint16_t transformID,
                                      float scaleX, float scaleY);

libagon's: each makes a 2d affine transform in buffer `transformID`
with one operation, its arguments 16-bit fixed point. Each sends A0
transformID; &20, then:

| Call            | Then                                         |
|-----------------|----------------------------------------------|
| `..._rotated`   | 2 (ROTATE), &C0, angle; (degrees)            |
| `..._scaled`    | 5 (SCALE), &C8, scaleX;, scaleY;             |
| `..._translated`| 6 (TRANSLATE), &C0, x;, y;                   |
| `..._skewed`    | 8 (SHEAR), &C8, scaleX;, scaleY;             |

Format &C0 is whole numbers, &C8 is 8 bits of fraction: a scale or a
shear is rounded to the nearest 1/256 and cut to 16 bits. libagon sends
x's high byte where y's should be in the translation; acc sends y.
"Skewed" is a shear, as libagon has it, not the VDP's SKEW, which takes
angles. The matrix is used to draw by `vdp_adv_use_affine_matrix` in
[agon/vdp/bitmap.h](#agonvdpbitmaph).

### `vdp_adv_apply_transformation_with_stride`

    void vdp_adv_apply_transformation_with_stride(uint16_t sourceID,
                                                  uint16_t destID,
                                                  uint16_t transformID,
                                                  uint16_t offset,
                                                  uint16_t stride);

A0 destID; &29, 6 (HAS_OFFSET and HAS_STRIDE), &C0, transformID;,
sourceID;, offset;, stride;. Transform data, on whole 16-bit numbers.

### `vdp_adv_affine_identity`, `vdp_adv_affine_invert`, `vdp_adv_affine_float`, `vdp_adv_affine_args`, `VDP_AFFINE_ROTATE`

    void vdp_adv_affine_identity(int bufferId);
    void vdp_adv_affine_invert(int bufferId);
    void vdp_adv_affine_float(int bufferId, int operation, int count,
                              const float *values);
    void vdp_adv_affine_args(int bufferId, int operation,
                             const void *args, int length);

2d affine transforms, command &20: A0 id; &20, operation, then the
arguments. IDENTITY and INVERT take none, so no format byte either.
`vdp_adv_affine_float` sends a format byte 0 (a 32-bit float) and then
the `count` floats of `values`, 4 bytes each; with
`VDP_AFFINE_OP_MULTI_FORMAT` every value has a format byte of its own.
`vdp_adv_affine_args` sends the `length` bytes at `args`, formats and
values as they are to go.

| Name                             | Value | Operation               |
|----------------------------------|-------|-------------------------|
| `VDP_AFFINE_IDENTITY`            | 0     | identity                |
| `VDP_AFFINE_INVERT`              | 1     | invert                  |
| `VDP_AFFINE_ROTATE`              | 2     | rotate, degrees         |
| `VDP_AFFINE_ROTATE_RAD`          | 3     | rotate, radians         |
| `VDP_AFFINE_MULTIPLY`            | 4     | multiply                |
| `VDP_AFFINE_SCALE`               | 5     | scale                   |
| `VDP_AFFINE_TRANSLATE`           | 6     | translate               |
| `VDP_AFFINE_TRANSLATE_OS_COORDS` | 7     | translate, OS coords    |
| `VDP_AFFINE_SHEAR`               | 8     | shear                   |
| `VDP_AFFINE_SKEW`                | 9     | skew, degrees           |
| `VDP_AFFINE_SKEW_RAD`            | 10    | skew, radians           |
| `VDP_AFFINE_TRANSFORM`           | 11    | transform               |
| `VDP_AFFINE_TRANSLATE_BITMAP`    | 12    | translate bitmap        |
| `VDP_AFFINE_OP_MASK`             | &0F   | the operation's bits    |
| `VDP_AFFINE_OP_ADVANCED_OFFSETS` | &10   | advanced offsets        |
| `VDP_AFFINE_OP_BUFFER_VALUE`     | &20   | values from a buffer    |
| `VDP_AFFINE_OP_MULTI_FORMAT`     | &40   | a format for each value |

    float scale[2] = { 2.0f, 2.0f };

    vdp_adv_affine_float(5, VDP_AFFINE_SCALE, 2, scale);

### `vdp_adv_affine_3d_identity`, `vdp_adv_affine_3d_invert`, `vdp_adv_affine_3d_float`, `vdp_adv_affine_3d_args`

    void vdp_adv_affine_3d_identity(int bufferId);
    void vdp_adv_affine_3d_invert(int bufferId);
    void vdp_adv_affine_3d_float(int bufferId, int operation, int count,
                                 const float *values);
    void vdp_adv_affine_3d_args(int bufferId, int operation,
                                const void *args, int length);

The same for 3d transforms, command &21.

### `VDP_FLOAT_FORMAT_FIXED`, `VDP_FLOAT_FORMAT_16BIT`

The format byte before a run of number arguments. Zero is a 32-bit
float; otherwise 16-bit and/or fixed point, the low five bits a signed
shift of the binary point.

| Name                            | Value | Meaning                   |
|---------------------------------|-------|---------------------------|
| `VDP_FLOAT_FORMAT_SHIFT_MASK`   | &1F   | the shift                 |
| `VDP_FLOAT_FORMAT_SHIFT_TOPBIT` | &10   | the shift's sign          |
| `VDP_FLOAT_FORMAT_FLAGS`        | &E0   | the flags                 |
| `VDP_FLOAT_FORMAT_FIXED`        | &40   | fixed point               |
| `VDP_FLOAT_FORMAT_16BIT`        | &80   | 16 bits                   |

### `vdp_adv_matrix_float`, `vdp_adv_matrix_combine`, `vdp_adv_matrix_args`, `VDP_MATRIX_SET`

    void vdp_adv_matrix_float(int bufferId, int operation, int rows,
                              int columns, int count,
                              const float *values);
    void vdp_adv_matrix_combine(int bufferId, int operation, int rows,
                                int columns, int sourceId1,
                                int sourceId2);
    void vdp_adv_matrix_args(int bufferId, int operation, int rows,
                             int columns, const void *args, int length);

Matrices of any size, command &22: A0 id; &22, operation, rows,
columns, then the arguments. `vdp_adv_matrix_float` sends one format
byte 0 and the `count` floats, all sharing it.
`vdp_adv_matrix_combine` sends sourceId1;, sourceId2;, for ADD,
SUBTRACT and MULTIPLY. `vdp_adv_matrix_args` sends the `length` bytes
at `args`.

| Name                             | Value | Operation               |
|----------------------------------|-------|-------------------------|
| `VDP_MATRIX_SET`                 | 0     | set                     |
| `VDP_MATRIX_SET_VALUE`           | 1     | set a value             |
| `VDP_MATRIX_FILL`                | 2     | fill                    |
| `VDP_MATRIX_DIAGONAL`            | 3     | diagonal                |
| `VDP_MATRIX_ADD`                 | 4     | add two                 |
| `VDP_MATRIX_SUBTRACT`            | 5     | subtract two            |
| `VDP_MATRIX_MULTIPLY`            | 6     | multiply two            |
| `VDP_MATRIX_SCALAR_MULTIPLY`     | 7     | multiply by a number    |
| `VDP_MATRIX_SUBMATRIX`           | 8     | submatrix               |
| `VDP_MATRIX_INSERT_ROW`          | 9     | insert a row            |
| `VDP_MATRIX_INSERT_COLUMN`       | 10    | insert a column         |
| `VDP_MATRIX_DELETE_ROW`          | 11    | delete a row            |
| `VDP_MATRIX_DELETE_COLUMN`       | 12    | delete a column         |
| `VDP_MATRIX_OP_MASK`             | &0F   | the operation's bits    |
| `VDP_MATRIX_OP_ADVANCED_OFFSETS` | &10   | advanced offsets        |
| `VDP_MATRIX_OP_BUFFER_VALUE`     | &20   | values from a buffer    |

### `vdp_adv_transform_bitmap`, `VDP_TRANSFORM_BITMAP_RESIZE`

    void vdp_adv_transform_bitmap(int bufferId, int options,
                                  int transformBufferId, int bitmapId,
                                  int width, int height);

A0 id; &28, options, transformBufferId;, bitmapId;, and width;
height; only with `VDP_TRANSFORM_BITMAP_EXPLICIT_SIZE`.

| Name                                 | Value |
|--------------------------------------|-------|
| `VDP_TRANSFORM_BITMAP_RESIZE`        | &01   |
| `VDP_TRANSFORM_BITMAP_EXPLICIT_SIZE` | &02   |
| `VDP_TRANSFORM_BITMAP_TRANSLATE`     | &04   |

### `vdp_adv_transform_data`, `vdp_adv_transform_data_args`, `VDP_TRANSFORM_DATA_HAS_SIZE`

    void vdp_adv_transform_data(int bufferId, int options, int format,
                                int transformBufferId,
                                int sourceBufferId, int size,
                                int offset, int stride, int limit);
    void vdp_adv_transform_data_args(int bufferId, int options,
                                     int format, int transformBufferId,
                                     int sourceBufferId,
                                     const void *args, int length);

Both send A0 id; &29, options, format, transformBufferId;,
sourceBufferId;. Then `vdp_adv_transform_data` sends each of these
only when its option is set: size (a byte), offset (16 bits, or 24
with `VDP_TRANSFORM_DATA_ADVANCED`), stride; and limit; (16 bits
either way). With `VDP_TRANSFORM_DATA_BUFFER_ARGS` each is a buffer
and offset instead, which is what the `_args` form, sending the
`length` bytes at `args`, is for.

| Name                             | Value | Option                     |
|----------------------------------|-------|----------------------------|
| `VDP_TRANSFORM_DATA_HAS_SIZE`    | &01   | a size                     |
| `VDP_TRANSFORM_DATA_HAS_OFFSET`  | &02   | an offset                  |
| `VDP_TRANSFORM_DATA_HAS_STRIDE`  | &04   | a stride                   |
| `VDP_TRANSFORM_DATA_HAS_LIMIT`   | &08   | a limit                    |
| `VDP_TRANSFORM_DATA_ADVANCED`    | &10   | a 24-bit offset            |
| `VDP_TRANSFORM_DATA_BUFFER_ARGS` | &20   | arguments from buffers     |
| `VDP_TRANSFORM_DATA_PER_BLOCK`   | &40   | per block                  |

### `vdp_adv_read_variable`, `VDP_READ_VAR_16BIT`

    void vdp_adv_read_variable(int bufferId, int options, int offset,
                               int variableId, int defaultValue);

A0 id; &30, options, offset (16 bits, or 24 with
`VDP_READ_VAR_ADVANCED_OFFSETS`), variableId;, and with
`VDP_READ_VAR_USE_DEFAULT` the default, as wide as the value: a byte,
or defaultValue; with `VDP_READ_VAR_16BIT`. Read a VDP variable into
the buffer at `offset`.

| Name                            | Value | Option                    |
|---------------------------------|-------|---------------------------|
| `VDP_READ_VAR_BIG_ENDIAN`       | &01   | high byte first           |
| `VDP_READ_VAR_ADVANCED_OFFSETS` | &10   | a 24-bit offset           |
| `VDP_READ_VAR_USE_DEFAULT`      | &40   | a default follows         |
| `VDP_READ_VAR_16BIT`            | &80   | a 16-bit value            |

### `vdp_adv_expand_bitmap`, `vdp_adv_expand_bitmap_mapped`, `VDP_EXPAND_BITMAP_SIZE`

    void vdp_adv_expand_bitmap(int bufferId, int options,
                               int sourceBufferId, int width,
                               const void *map);
    void vdp_adv_expand_bitmap_mapped(int bufferId, int options,
                                      int sourceBufferId, int width,
                                      int mapBufferId);

A0 id; &48, options, sourceBufferId;, and width; only with
`VDP_EXPAND_BITMAP_ALIGNED`, then the map. `vdp_adv_expand_bitmap`
takes `VDP_EXPAND_BITMAP_USEBUFFER` out of the options and sends the
map, a byte for each value a pixel can have: 2 to the bits a pixel,
which are 8 (256 bytes) when the options say 0.
`vdp_adv_expand_bitmap_mapped` puts USEBUFFER in and sends
mapBufferId;.

| Name                          | Value | Option                      |
|-------------------------------|-------|-----------------------------|
| `VDP_EXPAND_BITMAP_SIZE`      | &07   | bits a pixel, 0 for 8       |
| `VDP_EXPAND_BITMAP_ALIGNED`   | &08   | a width follows             |
| `VDP_EXPAND_BITMAP_USEBUFFER` | &10   | the map is in a buffer      |

### `vdp_adv_add_callback`, `vdp_adv_remove_callback`, `VDP_CALLBACK_VSYNC`

    void vdp_adv_add_callback(int bufferId, int type);
    void vdp_adv_remove_callback(int bufferId, int type);

A0 id; &50, type; and A0 id; &51, type;. Have the VDP call the buffer
on an event, or stop. In a remove, 65535 as the buffer removes every
buffer from that type; as the type, the buffer from every type.

| Name                       | Value  | Called on                   |
|----------------------------|--------|-----------------------------|
| `VDP_CALLBACK_VSYNC`       | 0      | vsync                       |
| `VDP_CALLBACK_MODE_CHANGE` | 1      | a mode change               |
| `VDP_CALLBACK_KEYBOARD`    | 2      | the keyboard                |
| `VDP_CALLBACK_MOUSE`       | 3      | the mouse                   |
| `VDP_CALLBACK_PALETTE`     | 4      | the palette                 |
| `VDP_CALLBACK_READPIXEL`   | 5      | a pixel read                |
| `VDP_CALLBACK_SENDING_VDPP`| &0100  | sending VDPP                |
| `VDP_CALLBACK_SENT_VDPP`   | &0180  | VDPP sent                   |

### `vdp_adv_debug_info`

    void vdp_adv_debug_info(int bufferId);

A0 id; &80. The VDP writes what it knows of the buffer to its own
debug output only.

## `agon/vdp/audio.h`

Sound: VDU 23, 0, &85, channel, command, arguments. A program includes
[agon/vdp.h](#agonvdph), which includes this.

The byte after &85 is a channel, 0 to 31, or, for the commands on a
sample, a negative sample number: -1 is the sample kept in buffer
&FB00, -2 the one in &FB01, and so on. Channel 255 is the sound system
as a whole.

The VDP answers every command but the sample debug dump with a status
packet, which MOS keeps in the audio system variables; nothing here
waits for it.

The names down to `vdp_audio_set_waveform_parameter` are libagon's,
sending libagon's bytes except where libagon sends what VDP 2.16.0 does
not take; those places are said below. The ones after are the rest of
what VDP 2.16.0 takes.

In the sequences, a plain argument is one byte, its low byte when the
argument is wider; `x;` is two bytes, low first; `x(3)` is three. Every
sequence starts VDU 23, 0, &85, written `85` below.

### `VDP_AUDIO_CHANNELS`, `VDP_AUDIO_MAX_CHANNELS`, `VDP_AUDIO_DEFAULT_SAMPLE_RATE`, `VDP_AUDIO_DEFAULT_FREQUENCY`, `VDP_AUDIO_BUFFERED_SAMPLE_BASEID`

The VDP's own names, each with `VDP_` in front.

| Name                               | Value | Meaning                 |
|------------------------------------|-------|-------------------------|
| `VDP_AUDIO_CHANNELS`               | 3     | enabled at boot         |
| `VDP_AUDIO_MAX_CHANNELS`           | 32    | channels in all         |
| `VDP_AUDIO_DEFAULT_SAMPLE_RATE`    | 16384 | the default rate        |
| `VDP_AUDIO_DEFAULT_FREQUENCY`      | 523   | a tuneable sample's, C5 |
| `VDP_AUDIO_BUFFERED_SAMPLE_BASEID` | &FB00 | sample -1's buffer      |

### `VDP_AUDIO_CMD_PLAY`

The command byte, after the channel.

| Name                        | Value | Function                         |
|-----------------------------|-------|----------------------------------|
| `VDP_AUDIO_CMD_PLAY`        | 0     | `vdp_audio_play_note`            |
| `VDP_AUDIO_CMD_STATUS`      | 1     | `vdp_audio_status`               |
| `VDP_AUDIO_CMD_VOLUME`      | 2     | `vdp_audio_set_volume`           |
| `VDP_AUDIO_CMD_FREQUENCY`   | 3     | `vdp_audio_set_frequency`        |
| `VDP_AUDIO_CMD_WAVEFORM`    | 4     | `vdp_audio_set_waveform`         |
| `VDP_AUDIO_CMD_SAMPLE`      | 5     | the sample calls                 |
| `VDP_AUDIO_CMD_ENV_VOLUME`  | 6     | the volume envelopes             |
| `VDP_AUDIO_CMD_ENV_FREQUENCY` | 7   | the frequency envelopes          |
| `VDP_AUDIO_CMD_ENABLE`      | 8     | `vdp_audio_enable_channel`       |
| `VDP_AUDIO_CMD_DISABLE`     | 9     | `vdp_audio_disable_channel`      |
| `VDP_AUDIO_CMD_RESET`       | 10    | `vdp_audio_reset_channel`        |
| `VDP_AUDIO_CMD_SEEK`        | 11    | `vdp_audio_sample_seek`          |
| `VDP_AUDIO_CMD_DURATION`    | 12    | `vdp_audio_sample_duration`      |
| `VDP_AUDIO_CMD_SAMPLERATE`  | 13    | `vdp_audio_sample_rate`          |
| `VDP_AUDIO_CMD_SET_PARAM`   | 14    | `..._set_waveform_parameter`     |

### `vdp_audio_play_note`, `vdp_audio_play_sample`

    void vdp_audio_play_note(int channel, int volume, int frequency,
                             int duration);
    void vdp_audio_play_sample(int channel, int volume);

85 channel, 0, volume, frequency;, duration;. Play a note on the
channel. `vdp_audio_play_sample` is a note of frequency 0 and duration
0: a channel set to a sample plays the whole of it.

    vdp_audio_play_note(0, 100, 440, 500);

### `vdp_audio_status`, `VDP_AUDIO_STATUS_ACTIVE`

    void vdp_audio_status(int channel);

85 channel, 1. Ask for the channel's status. The VDP answers with a
status packet, which this does not wait for. Its bits:

| Name                                      | Value |
|-------------------------------------------|-------|
| `VDP_AUDIO_STATUS_ACTIVE`                 | &01   |
| `VDP_AUDIO_STATUS_PLAYING`                | &02   |
| `VDP_AUDIO_STATUS_INDEFINITE`             | &04   |
| `VDP_AUDIO_STATUS_HAS_VOLUME_ENVELOPE`    | &08   |
| `VDP_AUDIO_STATUS_HAS_FREQUENCY_ENVELOPE` | &10   |

### `vdp_audio_set_volume`, `vdp_audio_set_frequency`

    void vdp_audio_set_volume(int channel, int volume);
    void vdp_audio_set_frequency(int channel, int frequency);

85 channel, 2, volume and 85 channel, 3, frequency;.

### `vdp_audio_set_waveform`, `vdp_audio_set_sample`, `VDP_AUDIO_WAVE_SQUARE`, `VDP_AUDIO_WAVEFORM_SQUARE`

    void vdp_audio_set_waveform(int channel, int waveform);
    void vdp_audio_set_sample(int channel, int bufferID);

`vdp_audio_set_waveform` sends 85 channel, 4, waveform. A negative
waveform is a sample number. Waveform 8 takes a buffer ID after it,
which only `vdp_audio_set_sample` sends: 85 channel, 4, 8, bufferID;.
As libagon does, `vdp_audio_set_waveform` sends the one byte whatever
the waveform is.

| VDP's name                 | libagon's name                | Value |
|----------------------------|-------------------------------|-------|
| `VDP_AUDIO_WAVE_SQUARE`    | `VDP_AUDIO_WAVEFORM_SQUARE`   | 0     |
| `VDP_AUDIO_WAVE_TRIANGLE`  | `VDP_AUDIO_WAVEFORM_TRIANGLE` | 1     |
| `VDP_AUDIO_WAVE_SAWTOOTH`  | `VDP_AUDIO_WAVEFORM_SAWTOOTH` | 2     |
| `VDP_AUDIO_WAVE_SINE`      | `VDP_AUDIO_WAVEFORM_SINEWAVE` | 3     |
| `VDP_AUDIO_WAVE_NOISE`     | `VDP_AUDIO_WAVEFORM_NOISE`    | 4     |
| `VDP_AUDIO_WAVE_VICNOISE`  | `VDP_AUDIO_WAVEFORM_VICNOISE` | 5     |
| `VDP_AUDIO_WAVE_SAMPLE`    |                               | 8     |

`VDP_AUDIO_WAVE_DEFAULT` is 0, the square wave.

### `vdp_audio_load_sample`, `vdp_audio_load_sample_header`

    void vdp_audio_load_sample(int sample, int length, uint8_t *data);
    void vdp_audio_load_sample_header(int sample, int length);

85 sample, 5, 0, length(3), and then that many bytes of data.
`vdp_audio_load_sample` sends the `length` bytes at `data`, in pieces
of at most 65535 and none at all when there are none.
`vdp_audio_load_sample_header` sends the command without the data: the
caller sends the `length` bytes itself, straight after, in as many
pieces as it likes.

libagon ends with a call to MOS for whatever is left after its
65535-byte pieces, even when that is nothing, and a count of 0 has MOS
send on up to a zero byte, past the end of the data, which the VDP then
reads as commands.

    vdp_audio_load_sample(-1, len, pcm);
    vdp_audio_set_waveform(0, -1);
    vdp_audio_play_sample(0, 127);

### `vdp_audio_clear_sample`

    void vdp_audio_clear_sample(int sample);

85 sample, 5, 1. libagon sends 8 bytes for this 6-byte command: the
two after it are whatever lies past its template in memory.

### `vdp_audio_create_sample_from_buffer`, `vdp_audio_create_sample_from_buffer_rate`, `VDP_AUDIO_FORMAT_8BIT_SIGNED`

    void vdp_audio_create_sample_from_buffer(int channel, int bufferID,
                                             int format);
    void vdp_audio_create_sample_from_buffer_rate(int channel,
                                                  int bufferID,
                                                  int format,
                                                  int rate);

85 channel, 5, 2, bufferID;, format. Make the buffer a sample.
`vdp_audio_create_sample_from_buffer_rate` sets
`VDP_AUDIO_FORMAT_WITH_RATE` in the format and sends rate; after it.
With that bit in the format, the plain call sends no rate, as libagon's
does not, and the VDP waits for one.

A format is the data in the low three bits, and two flags:

| Name                             | Value | Meaning                   |
|----------------------------------|-------|---------------------------|
| `VDP_AUDIO_FORMAT_8BIT_SIGNED`   | 0     | 8-bit signed              |
| `VDP_AUDIO_FORMAT_8BIT_UNSIGNED` | 1     | 8-bit unsigned            |
| `VDP_AUDIO_FORMAT_DATA_MASK`     | 7     | the data's bits           |
| `VDP_AUDIO_FORMAT_WITH_RATE`     | 8     | a 16-bit rate follows     |
| `VDP_AUDIO_FORMAT_TUNEABLE`      | 16    | tuneable                  |

### `VDP_AUDIO_SAMPLE_FORMAT_8BIT_SIGNED`, `VDP_AUDIO_SAMPLE_FORMAT_SAMPLE_TUNEABLE`

libagon's names for the same formats:

| Name                                          | Value |
|-----------------------------------------------|-------|
| `VDP_AUDIO_SAMPLE_FORMAT_8BIT_SIGNED`         | 0     |
| `VDP_AUDIO_SAMPLE_FORMAT_8BIT_UNSIGNED`       | 1     |
| `VDP_AUDIO_SAMPLE_FORMAT_SAMPLE_RATE_FOLLOWS` | 8     |
| `VDP_AUDIO_SAMPLE_FORMAT_SAMPLE_TUNEABLE`     | 16    |

`VDP_AUDIO_SAMPLE_FORMAT_SAMPLE_TUNEABLE` is 16, as the VDP reads it.
libagon has 8, which is the rate-follows bit, and a sample made with it
has the VDP wait for a rate libagon never sends.

### `vdp_audio_set_sample_frequency`, `vdp_audio_set_buffer_frequency`

    void vdp_audio_set_sample_frequency(int sample, int frequency);
    void vdp_audio_set_buffer_frequency(int channel, int bufferID,
                                        int frequency);

85 sample, 5, 3, frequency; and 85 channel, 5, 4, bufferID;,
frequency;. A sample's frequency, by sample number or by buffer ID.

### `vdp_audio_set_sample_repeat_start`, `vdp_audio_set_buffer_repeat_start`, `vdp_audio_set_sample_repeat_length`, `vdp_audio_set_buffer_repeat_length`

    void vdp_audio_set_sample_repeat_start(int sample, int repeatStart);
    void vdp_audio_set_buffer_repeat_start(int channel, int bufferID,
                                           int repeatStart);
    void vdp_audio_set_sample_repeat_length(int sample,
                                            int repeatLength);
    void vdp_audio_set_buffer_repeat_length(int channel, int bufferID,
                                            int repeatLength);

| Call                          | Sends (after 85)                      |
|-------------------------------|---------------------------------------|
| `..._sample_repeat_start`     | sample, 5, 5, repeatStart(3)          |
| `..._buffer_repeat_start`     | channel, 5, 6, bufferID;,             |
|                               | repeatStart(3)                        |
| `..._sample_repeat_length`    | sample, 5, 7, repeatLength(3)         |
| `..._buffer_repeat_length`    | channel, 5, 8, bufferID;,             |
|                               | repeatLength(3)                       |

A length of &FFFFFF (-1) repeats to the end of the sample.

### `vdp_audio_sample_debug_info`, `VDP_AUDIO_SAMPLE_LOAD`

    void vdp_audio_sample_debug_info(int channel, int bufferID);

85 channel, 5, &10, bufferID;. The VDP writes what it knows of the
sample to its debug log. The channel is not used, and there is no
answer, not even a status packet.

Command 5's sub-commands:

| Name                                        | Value |
|---------------------------------------------|-------|
| `VDP_AUDIO_SAMPLE_LOAD`                     | 0     |
| `VDP_AUDIO_SAMPLE_CLEAR`                    | 1     |
| `VDP_AUDIO_SAMPLE_FROM_BUFFER`              | 2     |
| `VDP_AUDIO_SAMPLE_SET_FREQUENCY`            | 3     |
| `VDP_AUDIO_SAMPLE_BUFFER_SET_FREQUENCY`     | 4     |
| `VDP_AUDIO_SAMPLE_SET_REPEAT_START`         | 5     |
| `VDP_AUDIO_SAMPLE_BUFFER_SET_REPEAT_START`  | 6     |
| `VDP_AUDIO_SAMPLE_SET_REPEAT_LENGTH`        | 7     |
| `VDP_AUDIO_SAMPLE_BUFFER_SET_REPEAT_LENGTH` | 8     |
| `VDP_AUDIO_SAMPLE_DEBUG_INFO`               | &10   |

### `vdp_audio_volume_envelope_disable`, `vdp_audio_volume_envelope_ADSR`, `VDP_AUDIO_ENVELOPE_ADSR`

    void vdp_audio_volume_envelope_disable(int channel);
    void vdp_audio_volume_envelope_ADSR(int channel, int attack,
                                        int decay, int sustain,
                                        int release);

85 channel, 6, 0 takes the channel's volume envelope off. 85 channel,
6, 1, attack;, decay;, sustain, release; gives it an ADSR one.

| Name                               | Value |
|------------------------------------|-------|
| `VDP_AUDIO_ENVELOPE_NONE`          | 0     |
| `VDP_AUDIO_ENVELOPE_ADSR`          | 1     |
| `VDP_AUDIO_ENVELOPE_MULTIPHASE_ADSR` | 2   |

The VDP reads an envelope's arguments only for a channel that is
enabled: sent to any other, they are taken for VDU commands. This holds
for the frequency envelopes too.

### `vdp_audio_volume_envelope_multiphase_ADSR`

    void vdp_audio_volume_envelope_multiphase_ADSR(uint8_t channel,
                                                   uint8_t numAttack,
                                                   uint8_t numSustain,
                                                   uint8_t numRelease,
                                                   int16_t *envelopeData);

85 channel, 6, 2, then the attack, sustain and release phases in turn,
each part as a count byte and then, for each phase, level, duration;.
`envelopeData` is `[level, duration]` pairs, attack then sustain then
release; the level sent is the low byte of its entry.

libagon counts its way through `envelopeData` in a byte, so with more
than 127 phases in all it wraps back to the start of it.

    int16_t env[] = { 127, 50,   100, 200,   0, 100 };

    vdp_audio_volume_envelope_multiphase_ADSR(0, 1, 1, 1, env);

### `vdp_audio_frequency_envelope_disable`, `vdp_audio_frequency_envelope_stepped`, `VDP_AUDIO_FREQUENCY_REPEATS`, `VDP_AUDIO_FREQ_ENVELOPE_CONTROL_REPEATS`

    void vdp_audio_frequency_envelope_disable(int channel);
    void vdp_audio_frequency_envelope_stepped(int channel,
                                              int phaseCount,
                                              int controlByte,
                                              int stepLength,
                                              int16_t *envelopeData);

85 channel, 7, 0 takes the frequency envelope off. The stepped one
sends 85 channel, 7, 1, phaseCount, controlByte, stepLength;, then
`phaseCount * 4` bytes of `envelopeData`: `[adjustment, steps]` pairs,
two 16-bit words a phase, which lie in memory as the VDP reads them.

The phase count is a byte. libagon counts the words in a byte as well,
and so never finishes with 128 phases or more.

| Name                                   | Value |
|----------------------------------------|-------|
| `VDP_AUDIO_FREQUENCY_ENVELOPE_STEPPED` | 1     |
| `VDP_AUDIO_FREQUENCY_REPEATS`          | &01   |
| `VDP_AUDIO_FREQUENCY_CUMULATIVE`       | &02   |
| `VDP_AUDIO_FREQUENCY_RESTRICT`         | &04   |

libagon's names for the three control bits, with the same values, are
`VDP_AUDIO_FREQ_ENVELOPE_CONTROL_REPEATS`,
`VDP_AUDIO_FREQ_ENVELOPE_CONTROL_CUMULATIVE` and
`VDP_AUDIO_FREQ_ENVELOPE_CONTROL_RESTRICT`.

### `vdp_audio_enable_channel`, `vdp_audio_disable_channel`, `vdp_audio_reset_channel`

    void vdp_audio_enable_channel(int channel);
    void vdp_audio_disable_channel(int channel);
    void vdp_audio_reset_channel(int channel);

85 channel, 8; 85 channel, 9; 85 channel, 10. Disabling idles the
channel rather than removing it: it stays enabled.

### `vdp_audio_sample_seek`, `vdp_audio_sample_duration`, `vdp_audio_sample_rate`

    void vdp_audio_sample_seek(int channel, int position);
    void vdp_audio_sample_duration(int channel, int duration);
    void vdp_audio_sample_rate(int channel, int rate);

85 channel, 11, position(3); 85 channel, 12, duration(3); 85 channel,
13, rate;.

### `vdp_audio_set_waveform_parameter`, `VDP_AUDIO_PARAM_16BIT`

    void vdp_audio_set_waveform_parameter(int channel, int parameter,
                                          int value);

85 channel, 14, parameter, value. With `VDP_AUDIO_PARAM_16BIT` in the
parameter the value is a word, value;, otherwise a byte; a byte
frequency changes only the low byte.

| Name                         | Value |
|------------------------------|-------|
| `VDP_AUDIO_PARAM_DUTY_CYCLE` | 0     |
| `VDP_AUDIO_PARAM_VOLUME`     | 2     |
| `VDP_AUDIO_PARAM_FREQUENCY`  | 3     |
| `VDP_AUDIO_PARAM_16BIT`      | &80   |
| `VDP_AUDIO_PARAM_MASK`       | &0F   |

### `vdp_audio_set_system_volume`, `vdp_audio_system_sample_rate`

    void vdp_audio_set_system_volume(int volume);
    void vdp_audio_system_sample_rate(int rate);

`vdp_audio_set_volume` and `vdp_audio_sample_rate` on channel 255, the
sound system as a whole: 85 255, 2, volume and 85 255, 13, rate;. The
volume is 0 to 127 (255 only asks for it); a rate of 65535 is the
default.

## `agon/keyboard.h`

The keyboard, as libagon names it: a buffer of key events, filled as MOS
gets each key, so that a program busy for a while between polls still
gets every key it was sent, in order.

MOS sends a packet for every key going down and every key coming up.
`kbuf_init` has MOS call a handler of acc's runtime on each one (with
`mos_setkbvector`, MOS call 0x1D; see [agon/mos.h](#agonmosh)), and
the handler copies the packet's four bytes into a ring on the heap. The
program takes them out with `kbuf_poll_event`. The header includes
`stdint.h` and `stdbool.h`.

    #include <agon/keyboard.h>

    struct keyboard_event_t e;

    kbuf_init(16);
    for (;;) {
        if (!kbuf_poll_event(&e) || !e.isdown)
            continue;
        if (e.ascii == 27)
            break;
        handle(e.vkey);
    }
    kbuf_deinit();

### `keyboard_event_t`

    struct keyboard_event_t {
        uint8_t ascii;
        uint8_t kmod;
        uint8_t vkey;
        uint8_t isdown;
    };

One key going down or coming up, as MOS's packet has it: the character
it means, which modifiers were held, which physical key it was (FabGL's
virtual key code, as MOS's `sysvar_vkeycode` holds it), and which way it
went (`isdown` is nonzero for down). Every field is a byte, so how a
struct is laid out does not come into it.

### `kbuf_init`

    void kbuf_init(uint8_t buf_len);

Start the buffer, with room for `buf_len` events, and have MOS call the
handler on every key packet from now on. The ring is
`(buf_len + 1) * 4` bytes from `malloc`. When it is full, the newest
events are dropped: the ones already in it are kept.

A buffer already started is ended first, with `kbuf_deinit`, and the
events in it are lost. When `malloc` fails there is no buffer and no
handler, and `kbuf_poll_event` answers `false`. A `buf_len` of 0 makes
a buffer that holds nothing.

### `kbuf_poll_event`

    bool kbuf_poll_event(struct keyboard_event_t *e);

Take the oldest event out of the buffer into `*e`, and answer `true`; or
answer `false`, with `*e` untouched, when the buffer is empty or was
never started. It does not wait.

### `kbuf_clear`

    void kbuf_clear(void);

Drop every event the buffer holds. The handler stays, and the next key
packet goes in as usual.

### `kbuf_deinit`

    void kbuf_deinit(void);

End the buffer: tell MOS to call no handler (`mos_setkbvector` with
`NULL`), and free the ring. It does nothing when no buffer was started.

The end of the program ends a buffer still running, however the
program ends: returning from `main`, `exit`, `_Exit` or `abort`. MOS
would otherwise go on calling the handler, which was in the program,
after the program had gone. A program needs `kbuf_deinit` only to stop
taking keys before it ends.

## `agon/joystick.h`

The two joysticks on the Console8's GPIO pins, as libagon names them:
one call reads them, and a macro for each switch tests what it read.

A pin reads 0 while its switch is closed, so each macro is a NOT of its
bit: it is true while that direction is pushed or that button is held.
The header includes `stdint.h`.

    #include <agon/joystick.h>

    uint16_t b;

    resetJoysticks();
    b = getJoystickButtons();
    if (joystick1_UP(b))
        y--;
    if (joystick1_BUTTON1(b))
        fire();

### `getJoystickButtons`

    uint16_t getJoystickButtons(void);

Read both joysticks: port C's data register (`PC_DR`, I/O port 0x9E) in
the low byte, the directions of the two joysticks interleaved, and port
D's (`PD_DR`, 0xA2) in the high byte, whose top four bits are the
buttons. Pass the answer to the macros below.

### `resetJoysticks`

    void resetJoysticks(void);

Make every pin of port C, and the top four of port D, inputs, as
`inputModePinID` does (see [agon/gpio.h](#agongpioh)): their `ALT2`
and `ALT1` bits cleared, and their direction bits set: the pins
`getJoystickButtons` reads, set up to be read.

### `joystick1_UP`, `joystick1_DOWN`, `joystick1_LEFT`, `joystick1_RIGHT`, `joystick2_UP`, `joystick2_DOWN`, `joystick2_LEFT`, `joystick2_RIGHT`

    #define joystick1_UP(buttons) (!((buttons) & (1U << (1))))

True while the direction is pushed. `buttons` is what
`getJoystickButtons` answered.

| Bit | Pin | Joystick 1        | Joystick 2        |
|-----|-----|-------------------|-------------------|
| 0   | PC0 |                   | `joystick2_UP`    |
| 1   | PC1 | `joystick1_UP`    |                   |
| 2   | PC2 |                   | `joystick2_DOWN`  |
| 3   | PC3 | `joystick1_DOWN`  |                   |
| 4   | PC4 |                   | `joystick2_LEFT`  |
| 5   | PC5 | `joystick1_LEFT`  |                   |
| 6   | PC6 |                   | `joystick2_RIGHT` |
| 7   | PC7 | `joystick1_RIGHT` |                   |

### `joystick1_BUTTON1`, `joystick1_BUTTON2`, `joystick2_BUTTON1`, `joystick2_BUTTON2`

    #define joystick1_BUTTON1(buttons) (!((buttons) & (1U << (13))))

True while the button is held.

| Bit | Pin | Joystick 1          | Joystick 2          |
|-----|-----|---------------------|---------------------|
| 12  | PD4 |                     | `joystick2_BUTTON1` |
| 13  | PD5 | `joystick1_BUTTON1` |                     |
| 14  | PD6 |                     | `joystick2_BUTTON2` |
| 15  | PD7 | `joystick1_BUTTON2` |                     |

## `agon/gpio.h`

The eZ80's GPIO pins, as libagon names them: ports B, C and D, eight
pins each, set up, written and read through the I/O space.

A port has four registers, at consecutive I/O ports: its data register
(the port's own number), then the data direction (a 1 bit makes that pin
an input), then `ALT1` and `ALT2`, which choose an alternate function and
are both 0 for a plain pin. The header includes `ez80f92.h`, which names
all four of each port (see [ez80f92.h](#ez80f92h)), and `stdbool.h` and
`stdint.h`.

Every function comes in two forms. One takes a port and a pin number,
0 to 7 (only the low three bits of it count). The other, ending in
`PinID`, takes a pin ID: a port in the high byte and a mask of its pins
in the low, so that one call does several pins of a port at once. Every
change is a read of the register, the bits changed, and a write back, so
the port's other pins keep what they had.

    #include <agon/gpio.h>

    outputMode(PORTC, 3);
    outputHigh(PORTC, 3);

    outputModePinID(PC0 | PC1 | PC7);
    outputLowPinID(PC0 | PC7);

### `PORTB`, `PORTC`, `PORTD`

| Name    | Value   | I/O port |
|---------|---------|----------|
| `PORTB` | `PB_DR` | 0x9A     |
| `PORTC` | `PC_DR` | 0x9E     |
| `PORTD` | `PD_DR` | 0xA2     |

The port numbers the functions with a `portID` take: each is the port's
data register. Its direction, `ALT1` and `ALT2` registers are the next
three ports up.

### `PB0`, `PB1`, `PB2`, `PB3`, `PB4`, `PB5`, `PB6`, `PB7`, `PC0`, `PC1`, `PC2`, `PC3`, `PC4`, `PC5`, `PC6`, `PC7`, `PD0`, `PD1`, `PD2`, `PD3`, `PD4`, `PD5`, `PD6`, `PD7`

    #define PB0 ((PB_DR << 8) | 0x01)
    ...
    #define PD7 ((PD_DR << 8) | 0x80)

The pin IDs: `PB0` is port B's data register in the high byte and pin
0's bit, 0x01, in the low; `PB7`'s bit is 0x80. IDs of one port combine
with `|` into an ID for several of its pins. IDs of two ports do not:
their port numbers would be or-ed together too.

### `outputMode`, `outputModePinID`

    void outputMode(uint8_t portID, uint8_t pinnumber);
    void outputModePinID(uint16_t pinID);

Make the pins outputs: clear their bits in `ALT2`, then `ALT1`, then the
direction register. The alternate bits go first, so that the pin passes
through no other function on the way.

### `inputMode`, `inputModePinID`

    void inputMode(uint8_t portID, uint8_t pinnumber);
    void inputModePinID(uint16_t pinID);

Make the pins inputs: clear their bits in `ALT2`, then `ALT1`, then set
them in the direction register.

### `outputHigh`, `outputHighPinID`, `outputLow`, `outputLowPinID`

    void outputHigh(uint8_t portID, uint8_t pinnumber);
    void outputHighPinID(uint16_t pinID);
    void outputLow(uint8_t portID, uint8_t pinnumber);
    void outputLowPinID(uint16_t pinID);

`outputHigh` sets the pins' bits in the port's data register, and
`outputLow` clears them. Neither changes a pin's direction: make it an
output with `outputMode` first.

### `input`, `inputPinID`, `inputPort`

    bool    input(uint8_t portID, uint8_t pinnumber);
    bool    inputPinID(uint16_t pinID);
    uint8_t inputPort(uint8_t port);

`input` reads the port's data register and answers whether the pin's bit
is 1. `inputPinID` answers whether any of the pins in the ID is 1.

`inputPort` answers the whole data register, a bit a pin. It is
`io_in(port)`, so it reads any I/O port it is given.

## `agon/timer.h`

A delay, as libagon names it, counted by the eZ80's timer 0.

### `delay`

    void delay(unsigned int ms);

Wait `ms` milliseconds. A `ms` of 0 returns at once.

`delay` busy-waits on timer 0 of the eZ80 (see
[ez80f92.h](#ez80f92h)): it stops the timer (`TMR0_CTL`, I/O port
0x80), loads 72 into its reload registers (`TMR0_RR_L` and `TMR0_RR_H`),
reads `TMR0_CTL` once to clear any flag left over, and starts it running
continuously on the 18.432 MHz clock divided by 256. That is 72,000
counts a second, so the timer comes round once a millisecond and sets
the top bit of `TMR0_CTL`, which reading clears. `delay` waits for that
bit once for every millisecond asked for, then stops the timer again.

MOS keeps timer 0 for itself but does nothing with it, which is why
libagon uses it too. A program that runs timer 0 itself loses what it
set up there at every `delay`.

    #include <agon/timer.h>

    delay(500);         /* half a second */

## `ez80f92.h`

The eZ80F92's on-chip peripherals, by their I/O port numbers: where each
timer, GPIO port, UART, SPI and I2C register and the real-time clock sit
in the I/O space, the interrupt vectors, and the bits of the UARTs' line
status. The names are Zilog's, as agondev's header has them. The header
includes `stdint.h`.

The I/O space is not memory, and C has no way into it: a pointer cannot
reach a register. agondev reaches it with `IO(addr)`, a clang address
space; acc has no address spaces, so `IO` is not defined, and `io_in`
and `io_out` are the way in.

    #include <ez80f92.h>

    io_out(TMR1_CTL, 0);                /* agondev: IO(TMR1_CTL) = 0; */
    if (io_in(TMR1_CTL) & 0x80)         /* agondev: IO(TMR1_CTL) & 0x80 */
        wraps++;

Where two names share a number below, both reach the same I/O port; the
chip decides which register a read or a write of it is.

### `io_in`, `io_out`

    uint8_t io_in(int addr);
    void    io_out(int addr, uint8_t value);

`io_in` reads the I/O port `addr` and answers its byte; `io_out` writes
`value` to it. Each is a call, over two instructions of acc's runtime:
`addr` goes in BC, the whole of it, as `in a, (c)` and `out (c), a`
take it.

### Timers: `TMR0_CTL`, `TMR0_RR_L`, `TMR0_RR_H`, `TMR0_DR_L`, `TMR0_DR_H`, `TMR1_CTL`, `TMR1_RR_L`, `TMR1_RR_H`, `TMR_ISS`

Six timers, each a control register and two ports, low byte and high,
of a 16-bit value: `RR`, the reload value the count starts again from,
and `DR`, which shares the two ports.

| Timer  | `_CTL` | `_RR_L`, `_DR_L` | `_RR_H`, `_DR_H` |
|--------|--------|------------------|------------------|
| `TMR0` | 0x80   | 0x81             | 0x82             |
| `TMR1` | 0x83   | 0x84             | 0x85             |
| `TMR2` | 0x86   | 0x87             | 0x88             |
| `TMR3` | 0x89   | 0x8A             | 0x8B             |
| `TMR4` | 0x8C   | 0x8D             | 0x8E             |
| `TMR5` | 0x8F   | 0x90             | 0x91             |

`TMR_ISS` is 0x92.

The `_CTL` bits acc's library and acc itself use:

| Bit  | Meaning                                              |
|------|------------------------------------------------------|
| 0x80 | the count came round; reading `_CTL` clears it       |
| 0x10 | continuous: start again from the reload value        |
| 0x0C | count the clock divided by 256                       |
| 0x02 | reload the count now                                 |
| 0x01 | run                                                  |

`delay` uses timer 0, which MOS keeps for itself but does nothing with
(see [agon/timer.h](#agontimerh)); acc's own cycle count uses timer 1,
which MOS leaves alone.

### Watchdog, clocks and infrared: `WDT_CTL`, `WDT_RR`, `CLK_PPD1`, `CLK_PPD2`, `IR_CTL`

| Name       | Port |
|------------|------|
| `WDT_CTL`  | 0x93 |
| `WDT_RR`   | 0x94 |
| `IR_CTL`   | 0xBF |
| `CLK_PPD1` | 0xDB |
| `CLK_PPD2` | 0xDC |

### GPIO ports: `PB_DR`, `PB_DDR`, `PB_ALT1`, `PB_ALT2`, `PC_DR`, `PC_DDR`, `PC_ALT1`, `PC_ALT2`, `PD_DR`, `PD_DDR`, `PD_ALT1`, `PD_ALT2`

| Port | `_DR` | `_DDR` | `_ALT1` | `_ALT2` |
|------|-------|--------|---------|---------|
| `PB` | 0x9A  | 0x9B   | 0x9C    | 0x9D    |
| `PC` | 0x9E  | 0x9F   | 0xA0    | 0xA1    |
| `PD` | 0xA2  | 0xA3   | 0xA4    | 0xA5    |

A pin is one bit of each of its port's four registers: data (`DR`),
direction (`DDR`, 1 for an input), and two that choose an alternate
function, both 0 for a plain pin. [agon/gpio.h](#agongpioh) has
functions over them.

### GPIO values: `PORTPIN_ZERO`, `PORTPIN_SEVEN`, `PORTPIN_ALL`, `PORTB_DRVAL_DEF`, `PORTB_DDRVAL_DEF`

The pin masks, one bit each:

| Name            | Value | Name              | Value |
|-----------------|-------|-------------------|-------|
| `PORTPIN_ZERO`  | 0x01  | `PORTPIN_FOUR`    | 0x10  |
| `PORTPIN_ONE`   | 0x02  | `PORTPIN_FIVE`    | 0x20  |
| `PORTPIN_TWO`   | 0x04  | `PORTPIN_SIX`     | 0x40  |
| `PORTPIN_THREE` | 0x08  | `PORTPIN_SEVEN`   | 0x80  |

and several together: `PORTPIN_FOURBITS_L` 0x0F, `PORTPIN_FOURBITS_U`
0xF0, `PORTPIN_PATTERN_AA` 0xAA, `PORTPIN_PATTERN_55` 0x55 and
`PORTPIN_ALL` 0xFF.

The registers' default values, the same for ports B, C and D (`PORTC_`
and `PORTD_` in place of `PORTB_`):

| Name                | Value |
|---------------------|-------|
| `PORTB_DRVAL_DEF`   | 0xFF  |
| `PORTB_DDRVAL_DEF`  | 0xFF  |
| `PORTB_ALT0VAL_DEF` | 0xFF  |
| `PORTB_ALT1VAL_DEF` | 0x00  |
| `PORTB_ALT2VAL_DEF` | 0x00  |

### UARTs: `UART0_RBR`, `UART0_THR`, `UART0_LSR`, `UART0_LCTL`, `UART0_BRG_L`, `UART1_RBR`, `UART1_THR`, `UART1_LSR`, `UART1_LCTL`, `UART1_BRG_L`

| Register  | `UART0_` | `UART1_` |
|-----------|----------|----------|
| `RBR`     | 0xC0     | 0xD0     |
| `THR`     | 0xC0     | 0xD0     |
| `BRG_L`   | 0xC0     | 0xD0     |
| `IER`     | 0xC1     | 0xD1     |
| `BRG_H`   | 0xC1     | 0xD1     |
| `IIR`     | 0xC2     | 0xD2     |
| `FCTL`    | 0xC2     | 0xD2     |
| `LCTL`    | 0xC3     | 0xD3     |
| `MCTL`    | 0xC4     | 0xD4     |
| `LSR`     | 0xC5     | 0xD5     |
| `MSR`     | 0xC6     | 0xD6     |
| `SPR`     | 0xC7     | 0xD7     |

UART1 is the one MOS opens for a program (`mos_uopen`; see
[agon/mos.h](#agonmosh)). `mos_ugetc_nb` reads it directly: a byte is
waiting in `UART1_RBR` when bit 0 of `UART1_LSR` is set.

### UART line status: `UART0_LSR_DR`, `UART0_LSR_THRE`, `UART0_LSR_TEMT`, `UART0_LSR_ERR`, `UART1_LSR_DR`, `UART1_LSR_THRE`, `UART1_LSR_TEMT`, `UART1_LSR_ERR`

The bits of `UART0_LSR` and `UART1_LSR`, the same for both (`UART1_` in
place of `UART0_`):

| Name             | Bit  |
|------------------|------|
| `UART0_LSR_ERR`  | 0x80 |
| `UART0_LSR_TEMT` | 0x40 |
| `UART0_LSR_THRE` | 0x20 |
| `UART0_LSR_BI`   | 0x10 |
| `UART0_LSR_FE`   | 0x08 |
| `UART0_LSR_PE`   | 0x04 |
| `UART0_LSR_OE`   | 0x02 |
| `UART0_LSR_DR`   | 0x01 |

### SPI: `SPI_CTL`, `SPI_SR`, `SPI_TSR`, `SPI_RBR`, `SPI_BRG_L`, `SPI_BRG_H`

| Name        | Port |
|-------------|------|
| `SPI_BRG_L` | 0xB8 |
| `SPI_BRG_H` | 0xB9 |
| `SPI_CTL`   | 0xBA |
| `SPI_SR`    | 0xBB |
| `SPI_TSR`   | 0xBC |
| `SPI_RBR`   | 0xBC |

### I2C: `I2C_DR`, `I2C_CTL`, `I2C_SR`, `I2C_CCR`, `I2C_SAR`, `I2C_XSAR`, `I2C_SRR`

| Name       | Port |
|------------|------|
| `I2C_SAR`  | 0xC8 |
| `I2C_XSAR` | 0xC9 |
| `I2C_DR`   | 0xCA |
| `I2C_CTL`  | 0xCB |
| `I2C_SR`   | 0xCC |
| `I2C_CCR`  | 0xCC |
| `I2C_SRR`  | 0xCD |

MOS has calls for I2C too: `mos_i2c_open` and the others in
[agon/mos.h](#agonmosh).

### Real-time clock: `RTC_SEC`, `RTC_MIN`, `RTC_HRS`, `RTC_DOW`, `RTC_DOM`, `RTC_MON`, `RTC_YR`, `RTC_CEN`, `RTC_CTRL`, `RTC_ACTRL`

| Name       | Port | Name        | Port |
|------------|------|-------------|------|
| `RTC_SEC`  | 0xE0 | `RTC_ASEC`  | 0xE8 |
| `RTC_MIN`  | 0xE1 | `RTC_AMIN`  | 0xE9 |
| `RTC_HRS`  | 0xE2 | `RTC_AHRS`  | 0xEA |
| `RTC_DOW`  | 0xE3 | `RTC_ADOW`  | 0xEB |
| `RTC_DOM`  | 0xE4 | `RTC_ACTRL` | 0xEC |
| `RTC_MON`  | 0xE5 | `RTC_CTRL`  | 0xED |
| `RTC_YR`   | 0xE6 |             |      |
| `RTC_CEN`  | 0xE7 |             |      |

### Chip selects and memory: `CS0_CTL`, `CS0_LBR`, `CS0_UBR`, `CS0_BMC`, `RAM_CTL`, `RAM_ADDR_U`

| Chip select | `_LBR` | `_UBR` | `_CTL` | `_BMC` |
|-------------|--------|--------|--------|--------|
| `CS0`       | 0xA8   | 0xA9   | 0xAA   | 0xF0   |
| `CS1`       | 0xAB   | 0xAC   | 0xAD   | 0xF1   |
| `CS2`       | 0xAE   | 0xAF   | 0xB0   | 0xF2   |
| `CS3`       | 0xB1   | 0xB2   | 0xB3   | 0xF3   |

`RAM_CTL` is 0xB4 and `RAM_ADDR_U` 0xB5.

### Flash: `FLASH_CTRL`, `FLASH_KEY`, `FLASH_DATA`, `FLASH_ADDR_U`, `FLASH_PAGE`, `FLASH_PROT`

| Name           | Port | Name          | Port |
|----------------|------|---------------|------|
| `FLASH_KEY`    | 0xF5 | `FLASH_IRQ`   | 0xFB |
| `FLASH_DATA`   | 0xF6 | `FLASH_PAGE`  | 0xFC |
| `FLASH_ADDR_U` | 0xF7 | `FLASH_ROW`   | 0xFD |
| `FLASH_CTRL`   | 0xF8 | `FLASH_COL`   | 0xFE |
| `FLASH_FDIV`   | 0xF9 | `FLASH_PGCTL` | 0xFF |
| `FLASH_PROT`   | 0xFA |               |      |

### Interrupt vectors: `PRT0_IVECT`, `UART0_IVECT`, `UART1_IVECT`, `I2C_IVECT`, `SPI_IVECT`, `RTC_IVECT`, `PORTB0_IVECT`

Not ports: the interrupt vectors, such as `mos_setintvector` takes (see
[agon/mos.h](#agonmosh)).

| Name          | Vector | Name          | Vector |
|---------------|--------|---------------|--------|
| `FLASH_IVECT` | 0x08   | `RTC_IVECT`   | 0x16   |
| `PRT0_IVECT`  | 0x0A   | `UART0_IVECT` | 0x18   |
| `PRT1_IVECT`  | 0x0C   | `UART1_IVECT` | 0x1A   |
| `PRT2_IVECT`  | 0x0E   | `I2C_IVECT`   | 0x1C   |
| `PRT3_IVECT`  | 0x10   | `SPI_IVECT`   | 0x1E   |
| `PRT4_IVECT`  | 0x12   |               |        |
| `PRT5_IVECT`  | 0x14   |               |        |

The GPIO pins have one each, two apart: `PORTB0_IVECT` 0x30 to
`PORTB7_IVECT` 0x3E, `PORTC0_IVECT` 0x40 to `PORTC7_IVECT` 0x4E, and
`PORTD0_IVECT` 0x50 to `PORTD7_IVECT` 0x5E.
