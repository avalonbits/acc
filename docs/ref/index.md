# Agon C Compiler API reference

acc, the C compiler for the Agon, and everything it supports: how to
run it and its options, the language it compiles, and every header of
its library, C99's and the Agon's. Look a name up with
`ref acc <name>`: a function, a macro, a type, a header or an option.

## acc

- [Using acc](01-using.md): compiling, linking, libraries, errors
- [acc's options](02-options.md): every option, one by one
- [The language](03-language.md): types, what is left out, limits

## Input and output

- [`stdio.h`](04-stdio.md): files, the console, `printf` and `scanf`
- [`wchar.h`](05-wchar.md): wide characters, strings and streams

## Strings and characters

- [`string.h`](06-string.md): strings and blocks of memory
- [`ctype.h`](07-ctype.md): classifying characters
- [`wctype.h`](08-wctype.md): classifying wide characters
- [`locale.h`](09-locale.md): the C locale

## Numbers

- [`math.h`](10-math.md): the mathematical functions
- [`tgmath.h`](11-tgmath.md): type-generic mathematics
- [`fenv.h`](12-fenv.md): the floating-point environment
- [`float.h`](13-float.md): the floating types' limits
- [`limits.h`](14-limits.md): the integer types' limits
- [`stdint.h`](15-stdint.md): integer types of given widths
- [`inttypes.h`](16-inttypes.md): their `printf` and `scanf` formats

## The program

- [`stdlib.h`](17-stdlib.md): memory, numbers from strings, sorting, exit
- [`errno.h`](18-errno.md): error numbers
- [`assert.h`](19-assert.md): checking what a program takes for granted
- [`signal.h`](20-signal.md): signals
- [`setjmp.h`](21-setjmp.md): jumps between functions
- [`stdarg.h`](22-stdarg.md): variable arguments
- [`time.h`](23-time.md): the date and the clock

## The language

- [`stddef.h`](24-stddef.md): `size_t`, `NULL`, `offsetof`
- [`stdbool.h`](25-stdbool.md): `bool`, `true`, `false`
- [`iso646.h`](26-iso646.md): operators spelled as words

## The Agon

- [`agon/mos.h`](27-agon-mos.md): MOS's calls, files, system variables
- [`agon/vdp.h`](28-agon-vdp.md): the VDP, as libagon names it
- [`agon/vdp_vdu.h`](29-agon-vdp_vdu.md): libagon's old name for `agon/vdp.h`
- [`agon/vdp/screen.h`](30-agon-vdp-screen.md): text, modes and the screen
- [`agon/vdp/graphics.h`](31-agon-vdp-graphics.md): lines, shapes, colours
- [`agon/vdp/bitmap.h`](32-agon-vdp-bitmap.md): bitmaps, sprites, fonts
- [`agon/vdp/buffer.h`](33-agon-vdp-buffer.md): the VDP's buffers
- [`agon/vdp/audio.h`](34-agon-vdp-audio.md): sound
- [`agon/keyboard.h`](35-agon-keyboard.md): key events as they happen
- [`agon/joystick.h`](36-agon-joystick.md): the joystick ports
- [`agon/gpio.h`](37-agon-gpio.md): the GPIO pins
- [`agon/timer.h`](38-agon-timer.md): delays
- [`ez80f92.h`](39-ez80f92.md): the eZ80F92's registers
