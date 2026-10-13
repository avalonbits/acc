# `agon/vdp.h`

The VDP, the Agon's video processor, driven by VDU commands: the bytes a
program writes to the screen through MOS. This is the header a program
includes; it brings in one header for each part of the VDP, and gives
the colour names, the background offset and the shapes of VDU commands
that the others share.

| Header                                      | Has                      |
|---------------------------------------------|--------------------------|
| [agon/vdp/screen.h](30-agon-vdp-screen.md)     | text, modes, the cursor  |
| [agon/vdp/graphics.h](31-agon-vdp-graphics.md) | PLOT and the shapes      |
| [agon/vdp/bitmap.h](32-agon-vdp-bitmap.md)     | bitmaps, sprites, fonts  |
| [agon/vdp/buffer.h](33-agon-vdp-buffer.md)     | the buffered commands    |
| [agon/vdp/audio.h](34-agon-vdp-audio.md)       | sound                    |

It includes `stdbool.h`, `stdint.h` and [agon/mos.h](27-agon-mos.md) too.

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
[agon/vdp/screen.h](30-agon-vdp-screen.md).

## `BLACK`, `RED`, `GREEN`, `YELLOW`, `BLUE`, `MAGENTA`, `CYAN`, `WHITE`, `BRIGHT_BLACK`, `BRIGHT_RED`, `BRIGHT_GREEN`, `BRIGHT_YELLOW`, `BRIGHT_BLUE`, `BRIGHT_MAGENTA`, `BRIGHT_CYAN`, `BRIGHT_WHITE`

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

## `BACKGROUND_COL_OFFSET`

    #define BACKGROUND_COL_OFFSET 128

A colour with 128 added is the background colour, for VDU 17 (the text
colour) and VDU 18 (the graphics colour).
[`vdp_set_text_bg_colour`](30-agon-vdp-screen.md#vdp_set_text_colour-vdp_set_text_bg_colour)
adds it; [`vdp_set_graphics_bg_colour`](31-agon-vdp-graphics.md#vdp_set_graphics_colour-vdp_set_graphics_fg_colour-vdp_set_graphics_bg_colour-vdp_gcol)
flips the bit instead.

## `VDP_PUTS`, `VDU_A`, `VDU_A_x_y`, `VDU_ADV_CMD`, `VDU_AUDIO_CMD`

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
