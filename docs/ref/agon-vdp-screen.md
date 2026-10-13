# `agon/vdp/screen.h`

Text, colour, modes, viewports, the cursor, characters, the keyboard and
the VDP's system commands. It is part of [agon/vdp.h](agon-vdp.md),
which is what a program includes; graphics are in
[agon/vdp/graphics.h](agon-vdp-graphics.md).

Each call sends what libagon sends for the same arguments. The calls
libagon does not have send VDP 2.16.0's commands as its own source
reads them. A byte argument passed as an `int` is sent as its low byte;
`x;` is a 16-bit argument, sent low byte first.

A call that asks the VDP for something, such as the cursor's place, a
pixel or the screen's size, is answered through MOS: the VDP sends a
packet, MOS puts what is in it in the system variables (`sys_vars`, in
[agon/mos.h](agon-mos.md)) and sets a bit in `sys_vars->vdp_pflags`. A
`vdp_request_` call given a true `wait` clears all of `vdp_pflags`,
sends the request, and waits until the bit for its answer is set; there
is no time limit. Given false, it sends the request and returns at once,
and the answer lands in the system variables when it comes. A
`vdp_return_` call always waits, and answers what arrived.

## `vdp_vdu_init`

    volatile SYSVAR *vdp_vdu_init(void);

Answers `sys_vars`, the system variables. It is libagon's name for them
from before it had `sys_vars`, and sends nothing.

## `vdp_send_to_printer`, `vdp_enable_printer`, `vdp_disable_printer`

    void vdp_send_to_printer(char ch);
    void vdp_enable_printer(void);
    void vdp_disable_printer(void);

VDU 1, `ch`; VDU 2; VDU 3.

## `vdp_enable_screen`, `vdp_disable_screen`, `vdp_bell`

    void vdp_enable_screen(void);
    void vdp_disable_screen(void);
    void vdp_bell(void);

VDU 6; VDU 21; VDU 7.

## `vdp_write_at_text_cursor`, `vdp_write_at_graphics_cursor`

    void vdp_write_at_text_cursor(void);
    void vdp_write_at_graphics_cursor(void);

VDU 4 and VDU 5: text is written at the text cursor, or at the graphics
cursor.

## `vdp_clear_screen`, `vdp_cls`

    void vdp_clear_screen(void);
    #define vdp_cls() vdp_clear_screen()

VDU 12.

## `vdp_cursor_left`, `vdp_cursor_right`, `vdp_cursor_down`, `vdp_cursor_up`

    void vdp_cursor_left(void);
    void vdp_cursor_right(void);
    void vdp_cursor_down(void);
    void vdp_cursor_up(void);

VDU 8, 9, 10 and 11: the text cursor one place left, right, down or up.

## `vdp_carriage_return`, `vdp_cursor_home`, `vdp_backspace`

    void vdp_carriage_return(void);
    void vdp_cursor_home(void);
    void vdp_backspace(void);

VDU 13; VDU 30; VDU 127.

## `vdp_cursor_tab`

    void vdp_cursor_tab(int xpos, int ypos);

VDU 31, `xpos`, `ypos`: the text cursor to that column and row, each a
byte.

    vdp_cursor_tab(10, 5);
    puts("here");

## `vdp_outchar`, `vdp_send_to_screen`

    void vdp_outchar(uint8_t c);
    void vdp_send_to_screen(char c);

VDU 27, `c`: `c` is drawn as a character, whatever it is, so a control
code shows as its glyph rather than acting. The two are libagon's two
names for the same thing.

## `vdp_page_mode_on`, `vdp_page_mode_off`, `vdp_temp_paged_mode`

    void vdp_page_mode_on(void);
    void vdp_page_mode_off(void);
    void vdp_temp_paged_mode(void);

VDU 14 and VDU 15: paged mode on and off. `vdp_temp_paged_mode` sends
VDU 23, 0, &9A: paged mode until the next time the screen scrolls a
page.

## `vdp_set_text_colour`, `vdp_set_text_bg_colour`

    void vdp_set_text_colour(int colour);
    void vdp_set_text_bg_colour(int colour);

VDU 17, `colour`: the text colour. `vdp_set_text_bg_colour` sends VDU
17, `colour` + 128 (`BACKGROUND_COL_OFFSET`), the text background. The
colour names, `BLACK` to `BRIGHT_WHITE`, are in
[agon/vdp.h](agon-vdp.md).

    vdp_set_text_colour(BRIGHT_YELLOW);
    vdp_set_text_bg_colour(BLUE);

## `vdp_define_colour`, `vdp_reset_graphics`

    void vdp_define_colour(int logical, int physical, int red,
                           int green, int blue);
    void vdp_reset_graphics(void);

`vdp_define_colour` sends VDU 19, `logical`, `physical`, `red`,
`green`, `blue`, one byte each: the colour a logical colour number
stands for. `vdp_reset_graphics` sends VDU 20.

## `vdp_mode`

    int vdp_mode(int mode);

VDU 22, `mode`: the screen mode. The answer is `mode`. A `mode` of 255
or more is not sent, and the answer is -1; a negative one is sent as its
low byte.

    if (vdp_mode(8) < 0)
        return 1;

## `vdp_reset_viewports`, `vdp_set_text_viewport`

    void vdp_reset_viewports(void);
    void vdp_set_text_viewport(int left, int bottom, int right, int top);

`vdp_reset_viewports` sends VDU 26. `vdp_set_text_viewport` sends VDU
28, `left`, `bottom`, `right`, `top`: the text viewport, its corners
one byte each. The graphics viewport is
[`vdp_set_graphics_viewport`](agon-vdp-graphics.md#vdp_set_graphics_viewport).

## `vdp_set_text_viewport_via_plot`, `vdp_set_graphics_viewport_via_plot`, `vdp_set_graphics_origin_via_plot`, `vdp_move_graphics_origin_and_viewport`

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

## `vdp_cursor_enable`, `vdp_cursor_behaviour`

    void vdp_cursor_enable(bool flag);
    void vdp_cursor_behaviour(int setting, int mask);

`vdp_cursor_enable` sends VDU 23, 1, `flag`: the text cursor shown
(true) or hidden (false). `vdp_cursor_behaviour` sends VDU 23, 16,
`setting`, `mask`.

## `vdp_set_cursor_start_line`, `vdp_set_cursor_end_line`, `vdp_set_cursor_start_column`, `vdp_set_cursor_end_column`

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

## `vdp_move_cursor_relative`

    void vdp_move_cursor_relative(int x, int y);

VDU 23, 0, &8C, `x`, `y`: the text cursor moved by `x` and `y` pixels,
each a signed byte, as VDP 2.16.0 reads them. libagon sends two bytes
each, which the VDP reads as `x`, `y` and two more commands; acc sends
one.

## `vdp_scroll_screen`, `vdp_scroll_screen_extent`

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

## `vdp_redefine_character`, `vdp_redefine_character_special`, `vdp_define_character`, `vdp_reset_system_font`

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

## `vdp_request_text_cursor_position`, `vdp_return_text_cursor_position`

    void vdp_request_text_cursor_position(bool wait);
    void vdp_return_text_cursor_position(uint8_t *return_x,
                                         uint8_t *return_y);

VDU 23, 0, &82. The answer is `sys_vars->cursorX` and `cursorY`, the
column and row; its flag is `vdp_pflag_cursor`.

`vdp_return_text_cursor_position` asks and waits, then stores the column
in `*return_x` and the row in `*return_y`; either pointer may be `NULL`.

    uint8_t col, row;

    vdp_return_text_cursor_position(&col, &row);

## `vdp_request_ascii_code_at_position`, `vdp_return_ascii_code_at_position`, `vdp_request_ascii_code_at_graphics_position`, `vdp_return_ascii_code_at_graphics_position`

    void    vdp_request_ascii_code_at_position(int x, int y, bool wait);
    uint8_t vdp_return_ascii_code_at_position(int x, int y);
    void    vdp_request_ascii_code_at_graphics_position(int x, int y,
                                                        bool wait);
    uint8_t vdp_return_ascii_code_at_graphics_position(int x, int y);

The character on the screen at a place. VDU 23, 0, &83, `x;` `y;` asks
at a text column and row; VDU 23, 0, &93, `x;` `y;` at a graphics
point. The answer is `sys_vars->scrchar`, and its flag is
`vdp_pflag_scrchar`. The `vdp_return_` calls answer it.

## `vdp_request_pixel_colour`, `vdp_return_pixel_colour`

    void     vdp_request_pixel_colour(int x, int y, bool wait);
    uint24_t vdp_return_pixel_colour(int x, int y);

VDU 23, 0, &84, `x;` `y;`: the colour of the pixel at a graphics point.
The answer is `sys_vars->scrpixel`, the colour, and `scrpixelIndex`, its
palette index; its flag is `vdp_pflag_point`. `vdp_return_pixel_colour`
answers `scrpixel`, 0 for black.

Drawing is queued in the VDP: test/vdpreal calls
[`vdp_flush_drawing_commands`](#vdp_legacy_modes-vdp_swap-vdp_flush_drawing_commands)
after drawing and before reading a pixel back.

## `vdp_request_palette_entry`, `vdp_return_palette_entry_colour`, `vdp_return_palette_entry_index`

    void     vdp_request_palette_entry(int n, bool wait);
    uint24_t vdp_return_palette_entry_colour(int n);
    uint8_t  vdp_return_palette_entry_index(int n);

VDU 23, 0, &94, `n`: palette entry `n`. The answer is in the same places
as a pixel's, `sys_vars->scrpixel` and `scrpixelIndex`, with the same
flag, `vdp_pflag_point`. `vdp_return_palette_entry_colour` answers
`scrpixel`, and `vdp_return_palette_entry_index` `scrpixelIndex`; each
asks the VDP once.

## `vdp_get_scr_dims`

    void vdp_get_scr_dims(bool wait);

VDU 23, 0, &86: the screen's size and mode. The answer is in
`sys_vars->scrWidth` and `scrHeight` (pixels), `scrCols` and `scrRows`
(characters), `scrColours` and `scrMode`; its flag is `vdp_pflag_mode`.

    vdp_get_scr_dims(true);
    printf("%d x %d\n", sys_vars->scrWidth, sys_vars->scrHeight);

## `vdp_request_rtc`, `vdp_set_rtc`

    void vdp_request_rtc(bool wait);
    void vdp_set_rtc(int year, int month, int day, int hour, int minute,
                     int second);

`vdp_request_rtc` sends VDU 23, 0, &87, 0: the clock. The answer is
`sys_vars->rtc`, six bytes MOS keeps packed: what they mean is the
`vdp_time_t` that `mos_unpackrtc` in [agon/mos.h](agon-mos.md) fills
in. Its flag is `vdp_pflag_rtc`.

`vdp_set_rtc` sends VDU 23, 0, &87, 1, `year` - 1980, `month`, `day`,
`hour`, `minute`, `second`, one byte each: the clock set, with the year
counted from 1980.

## `vdp_general_poll`

    void vdp_general_poll(int n);

VDU 23, 0, &80, `n`: the general poll, which the VDP answers with `n`.
MOS 3 keeps the answer in `sys_vars->gp`. It does not wait, and no flag
marks the answer.

## `vdp_set_keyboard_locale`, `vdp_keyboard_control`

    void vdp_set_keyboard_locale(int locale);
    void vdp_keyboard_control(int delay, int rate, int led);

`vdp_set_keyboard_locale` sends VDU 23, 0, &81, `locale`: the keyboard's
layout. `vdp_keyboard_control` sends VDU 23, 0, &88, `delay;` `rate;`
`led`: the repeat delay and rate, and the lights.

## `vdp_control_keys`, `vdp_check_key`

    void vdp_control_keys(bool on);
    void vdp_check_key(int vkey);

`vdp_control_keys` sends VDU 23, 0, &98, `on`. `vdp_check_key` sends
VDU 23, 0, &99, `vkey`: the VDP sends a new key packet for that virtual
key, as it now stands, and MOS puts it in the system variables
(`vkeycode`, `vkeydown`, and the rest).

## `vdp_getKeyMap`, `vdp_getKeyCode`, `vdp_waitKeyUp`, `vdp_waitKeyDown`

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

## `vdp_logical_scr_dims`, `vdp_set_pixel_coordinates`, `vdp_set_logical_coordinates`

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

## `vdp_legacy_modes`, `vdp_swap`, `vdp_flush_drawing_commands`

    void vdp_legacy_modes(bool on);
    void vdp_swap(void);
    void vdp_flush_drawing_commands(void);

| Call                         | Sends                 |
|------------------------------|-----------------------|
| `vdp_legacy_modes`           | VDU 23, 0, &C1, `on`  |
| `vdp_swap`                   | VDU 23, 0, &C3        |
| `vdp_flush_drawing_commands` | VDU 23, 0, &CA        |

## `vdp_console_mode`, `vdp_terminal_mode`

    void vdp_console_mode(bool on);
    void vdp_terminal_mode(void);

`vdp_console_mode` sends VDU 23, 0, &FE, `on`. `vdp_terminal_mode`
sends VDU 23, 0, &FF: terminal mode, which takes the VDU commands away
until the VDP is reset.

## `vdp_print_buffer`

    void vdp_print_buffer(int bufferId);

VDU 23, 0, &9B, `bufferId;`: the bytes of a buffer drawn as characters,
none of them taken as a command. Buffers are in
[agon/vdp/buffer.h](agon-vdp-buffer.md).

## `vdp_set_variable`, `vdp_clear_variable`

    void vdp_set_variable(uint16_t variableId, uint16_t value);
    void vdp_clear_variable(uint16_t variableId);

The VDP's variables, which turn features on and hold settings.
`vdp_set_variable` sends VDU 23, 0, &F8, `variableId;` `value;`, and
`vdp_clear_variable` VDU 23, 0, &F9, `variableId;`.
