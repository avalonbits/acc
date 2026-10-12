# acc

The reference to acc's C library, a header at a time. Look a name up
with `ref acc <name>`: a function, a macro, a type or a header.

## Input and output

- [`stdio.h`](stdio.md): files, the console, `printf` and `scanf`
- [`wchar.h`](wchar.md): wide characters, strings and streams

## Strings and characters

- [`string.h`](string.md): strings and blocks of memory
- [`ctype.h`](ctype.md): classifying characters
- [`wctype.h`](wctype.md): classifying wide characters
- [`locale.h`](locale.md): the C locale

## Numbers

- [`math.h`](math.md): the mathematical functions
- [`tgmath.h`](tgmath.md): type-generic mathematics
- [`fenv.h`](fenv.md): the floating-point environment
- [`float.h`](float.md): the floating types' limits
- [`limits.h`](limits.md): the integer types' limits
- [`stdint.h`](stdint.md): integer types of given widths
- [`inttypes.h`](inttypes.md): their `printf` and `scanf` formats

## The program

- [`stdlib.h`](stdlib.md): memory, numbers from strings, sorting, exit
- [`errno.h`](errno.md): error numbers
- [`assert.h`](assert.md): checking what a program takes for granted
- [`signal.h`](signal.md): signals
- [`setjmp.h`](setjmp.md): jumps between functions
- [`stdarg.h`](stdarg.md): variable arguments
- [`time.h`](time.md): the date and the clock

## The language

- [`stddef.h`](stddef.md): `size_t`, `NULL`, `offsetof`
- [`stdbool.h`](stdbool.md): `bool`, `true`, `false`
- [`iso646.h`](iso646.md): operators spelled as words

## The Agon

- [`agon/mos.h`](agon-mos.md): MOS's calls, files, system variables
- [`agon/vdp.h`](agon-vdp.md): the VDP, as libagon names it
- [`agon/vdp_vdu.h`](agon-vdp_vdu.md): libagon's old name for `agon/vdp.h`
- [`agon/vdp/screen.h`](agon-vdp-screen.md): text, modes and the screen
- [`agon/vdp/graphics.h`](agon-vdp-graphics.md): lines, shapes, colours
- [`agon/vdp/bitmap.h`](agon-vdp-bitmap.md): bitmaps, sprites, fonts
- [`agon/vdp/buffer.h`](agon-vdp-buffer.md): the VDP's buffers
- [`agon/vdp/audio.h`](agon-vdp-audio.md): sound
- [`agon/keyboard.h`](agon-keyboard.md): key events as they happen
- [`agon/joystick.h`](agon-joystick.md): the joystick ports
- [`agon/gpio.h`](agon-gpio.md): the GPIO pins
- [`agon/timer.h`](agon-timer.md): delays
- [`ez80f92.h`](ez80f92.md): the eZ80F92's registers
