/*
 * <agon/vdp/screen.h>'s calls: text, colour, modes, viewports, the cursor,
 * characters, the keyboard, and the VDP's system commands.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Each sends what libagon sends for the same arguments, read off its
 * compiled members, and test/agonlib/vdp_screen.c holds it to that. The
 * calls libagon does not have are VDP 2.16.0's commands as its source,
 * vdu.h and vdu_sys.h, reads them.
 *
 * A call that asks the VDP for something -- the cursor's place, a pixel,
 * the screen's size -- is answered through MOS: the VDP sends a packet,
 * MOS puts what is in it in the system variables and sets a bit in
 * sysvar_vdp_pflags. Asked to wait, these do what libagon does: clear the
 * flags, send the request, and wait until the bit for that answer is set.
 */
#include <agon/vdp.h>

#include "vdp_emit.h"

/* ------------------------------------------------------------------ */
/* the single bytes                                                    */

void vdp_send_to_printer(char ch)       { SEND(1, ch); }
void vdp_enable_printer(void)           { SEND(2); }
void vdp_disable_printer(void)          { SEND(3); }
void vdp_write_at_text_cursor(void)     { SEND(4); }
void vdp_write_at_graphics_cursor(void) { SEND(5); }
void vdp_enable_screen(void)            { SEND(6); }
void vdp_bell(void)                     { SEND(7); }
void vdp_cursor_left(void)              { SEND(8); }
void vdp_cursor_right(void)             { SEND(9); }
void vdp_cursor_down(void)              { SEND(10); }
void vdp_cursor_up(void)                { SEND(11); }
void vdp_clear_screen(void)             { SEND(12); }
void vdp_carriage_return(void)          { SEND(13); }
void vdp_page_mode_on(void)             { SEND(14); }
void vdp_page_mode_off(void)            { SEND(15); }
void vdp_reset_graphics(void)           { SEND(20); }
void vdp_disable_screen(void)           { SEND(21); }
void vdp_reset_viewports(void)          { SEND(26); }
void vdp_cursor_home(void)              { SEND(30); }
void vdp_backspace(void)                { SEND(127); }

/* VDU 27: the next byte is a character to draw, whatever it is. libagon
 * has the same thing under two names. */
void vdp_outchar(uint8_t c)             { SEND(27, c); }
void vdp_send_to_screen(char c)         { SEND(27, c); }

/* ------------------------------------------------------------------ */
/* colour, mode, viewports, the cursor                                 */

void vdp_set_text_colour(int colour)    { SEND(17, colour); }

/* A background colour is a colour with 128 added. */
void vdp_set_text_bg_colour(int colour) { SEND(17, colour + BACKGROUND_COL_OFFSET); }

void vdp_define_colour(int logical, int physical, int red, int green, int blue)
{
    SEND(19, logical, physical, red, green, blue);
}

/* A mode past 254 is not sent, and answers -1; any other is sent, and is
 * the answer. */
int vdp_mode(int mode)
{
    if (mode >= 255)
        return -1;
    SEND(22, mode);

    return mode;
}

/* VDU 28 takes its corners as bytes, left, bottom, right, top. */
void vdp_set_text_viewport(int left, int bottom, int right, int top)
{
    SEND(28, left, bottom, right, top);
}

void vdp_cursor_tab(int xpos, int ypos) { SEND(31, xpos, ypos); }

void vdp_cursor_enable(bool flag)       { SEND(23, 1, flag); }

/* VDU 23,7: scroll the text viewport (extent 0), the screen (1) or the
 * graphics viewport (2) in a direction, by a number of pixels. libagon's
 * vdp_scroll_screen is the screen. */
void vdp_scroll_screen(int direction, int speed)
{
    SEND(23, 7, 1, direction, speed);
}

void vdp_scroll_screen_extent(int extent, int direction, int speed)
{
    SEND(23, 7, extent, direction, speed);
}

void vdp_cursor_behaviour(int setting, int mask) { SEND(23, 16, setting, mask); }

/* The cursor's shape: the rows and columns of the character cell it
 * covers. The start row's top bits choose how it blinks. */
void vdp_set_cursor_start_line(int n)   { SEND(23, 0, 0x0a, n); }
void vdp_set_cursor_end_line(int n)     { SEND(23, 0, 0x0b, n); }
void vdp_set_cursor_start_column(int n) { SEND(23, 0, 0x8a, n); }
void vdp_set_cursor_end_column(int n)   { SEND(23, 0, 0x8b, n); }

/* By pixels, each a signed byte, as VDP 2.16.0 reads them. libagon sends
 * two bytes each, which the VDP reads as x, y and two commands. */
void vdp_move_cursor_relative(int x, int y) { SEND(23, 0, 0x8c, x, y); }

/* The viewports and origin from the last two graphics points. */
void vdp_set_text_viewport_via_plot(void)     { SEND(23, 0, 0x9c); }
void vdp_set_graphics_viewport_via_plot(void) { SEND(23, 0, 0x9d); }
void vdp_set_graphics_origin_via_plot(void)   { SEND(23, 0, 0x9e); }
void vdp_move_graphics_origin_and_viewport(void) { SEND(23, 0, 0x9f); }

/* ------------------------------------------------------------------ */
/* characters                                                          */

void vdp_redefine_character(int char_num, uint8_t b0, uint8_t b1, uint8_t b2,
                            uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6,
                            uint8_t b7)
{
    SEND(23, char_num, b0, b1, b2, b3, b4, b5, b6, b7);
}

/* VDU 23,0,&90: the same for any character, 0 to 255 -- VDU 23,c cannot
 * reach the ones below 32, which are the other VDU 23 commands. */
void vdp_redefine_character_special(int char_num, uint8_t b0, uint8_t b1,
                                    uint8_t b2, uint8_t b3, uint8_t b4,
                                    uint8_t b5, uint8_t b6, uint8_t b7)
{
    SEND(23, 0, 0x90, char_num, b0, b1, b2, b3, b4, b5, b6, b7);
}

/* From eight bytes somewhere, which libagon sends as a second run; nothing
 * at all for a null pointer. */
void vdp_define_character(int char_num, uint8_t *data)
{
    if (!data)
        return;
    SEND(23, 0, 0x90, char_num);
    SEND_BYTES(data, 8);
}

void vdp_reset_system_font(void)        { SEND(23, 0, 0x91); }

/* ------------------------------------------------------------------ */
/* asking the VDP                                                      */

/* A request, sent, and waited for when asked: the flags cleared first, so
 * that the bit for this answer is known to be new. */
static void request(const unsigned char *bytes, int n, int wait, uint8_t flag)
{
    volatile SYSVAR *v = sys_vars;

    if (wait)
        v->vdp_pflags = 0;
    SEND_BYTES(bytes, n);
    if (wait)
        while (!(v->vdp_pflags & flag))
            ;
}

void vdp_request_text_cursor_position(bool wait)
{
    static const unsigned char b[] = { 23, 0, 0x82 };

    request(b, 3, wait, vdp_pflag_cursor);
}

void vdp_return_text_cursor_position(uint8_t *return_x, uint8_t *return_y)
{
    vdp_request_text_cursor_position(true);
    if (return_x)
        *return_x = sys_vars->cursorX;
    if (return_y)
        *return_y = sys_vars->cursorY;
}

void vdp_request_ascii_code_at_position(int x, int y, bool wait)
{
    unsigned char b[] = { 23, 0, 0x83, W(x), W(y) };

    request(b, 7, wait, vdp_pflag_scrchar);
}

uint8_t vdp_return_ascii_code_at_position(int x, int y)
{
    vdp_request_ascii_code_at_position(x, y, true);

    return sys_vars->scrchar;
}

void vdp_request_ascii_code_at_graphics_position(int x, int y, bool wait)
{
    unsigned char b[] = { 23, 0, 0x93, W(x), W(y) };

    request(b, 7, wait, vdp_pflag_scrchar);
}

uint8_t vdp_return_ascii_code_at_graphics_position(int x, int y)
{
    vdp_request_ascii_code_at_graphics_position(x, y, true);

    return sys_vars->scrchar;
}

void vdp_request_pixel_colour(int x, int y, bool wait)
{
    unsigned char b[] = { 23, 0, 0x84, W(x), W(y) };

    request(b, 7, wait, vdp_pflag_point);
}

uint24_t vdp_return_pixel_colour(int x, int y)
{
    vdp_request_pixel_colour(x, y, true);

    return sys_vars->scrpixel;
}

void vdp_request_palette_entry(int n, bool wait)
{
    unsigned char b[] = { 23, 0, 0x94, n };

    request(b, 4, wait, vdp_pflag_point);
}

uint24_t vdp_return_palette_entry_colour(int n)
{
    vdp_request_palette_entry(n, true);

    return sys_vars->scrpixel;
}

uint8_t vdp_return_palette_entry_index(int n)
{
    vdp_request_palette_entry(n, true);

    return sys_vars->scrpixelIndex;
}

/* VDU 23,0,&86: the screen's size and mode, into the system variables. */
void vdp_get_scr_dims(bool wait)
{
    static const unsigned char b[] = { 23, 0, 0x86 };

    request(b, 3, wait, vdp_pflag_mode);
}

/* VDU 23,0,&87,0: the clock, into the system variables. */
void vdp_request_rtc(bool wait)
{
    static const unsigned char b[] = { 23, 0, 0x87, 0 };

    request(b, 4, wait, vdp_pflag_rtc);
}

/* VDU 23,0,&87,1: the clock set, the year counted from 1980. */
void vdp_set_rtc(int year, int month, int day, int hour, int minute, int second)
{
    SEND(23, 0, 0x87, 1, year - 1980, month, day, hour, minute, second);
}

/* VDU 23,0,&80: the general poll, which the VDP answers with n. */
void vdp_general_poll(int n)            { SEND(23, 0, 0x80, n); }

/* libagon's pointer to the system variables, from before it had
 * sys_vars. */
volatile SYSVAR *vdp_vdu_init(void)
{
    return sys_vars;
}

/* ------------------------------------------------------------------ */
/* the keyboard                                                        */

void vdp_set_keyboard_locale(int locale) { SEND(23, 0, 0x81, locale); }

void vdp_keyboard_control(int delay, int rate, int led)
{
    SEND(23, 0, 0x88, W(delay), W(rate), led);
}

void vdp_control_keys(bool on)          { SEND(23, 0, 0x98, on); }

/* VDU 23,0,&99: a new key packet for this virtual key, as it now stands. */
void vdp_check_key(int vkey)            { SEND(23, 0, 0x99, vkey); }

/* A bit of MOS's keyboard map: one byte of sixteen, eight keys a byte. */
uint8_t vdp_getKeyMap(uint8_t index)
{
    return mos_getkbmap()[index];
}

/* The key held, from the system variables; 0 while none is. */
uint8_t vdp_getKeyCode(void)
{
    return sys_vars->vkeydown ? sys_vars->keyascii : 0;
}

void vdp_waitKeyUp(void)
{
    while (sys_vars->vkeydown)
        ;
}

void vdp_waitKeyDown(void)
{
    while (!sys_vars->vkeydown)
        ;
}

/* ------------------------------------------------------------------ */
/* the rest of the system commands                                     */

/* Logical coordinates are BBC BASIC's 1280 by 1024 whatever the mode;
 * pixel coordinates are the mode's own. */
void vdp_logical_scr_dims(bool flag)    { SEND(23, 0, 0xc0, flag & 1); }
void vdp_set_pixel_coordinates(void)    { SEND(23, 0, 0xc0, 0); }
void vdp_set_logical_coordinates(void)  { SEND(23, 0, 0xc0, 1); }

void vdp_legacy_modes(bool on)          { SEND(23, 0, 0xc1, on); }
void vdp_swap(void)                     { SEND(23, 0, 0xc3); }
void vdp_flush_drawing_commands(void)   { SEND(23, 0, 0xca); }
void vdp_console_mode(bool on)          { SEND(23, 0, 0xfe, on); }
void vdp_terminal_mode(void)            { SEND(23, 0, 0xff); }

/* Paged mode until the next time the screen scrolls a page. */
void vdp_temp_paged_mode(void)          { SEND(23, 0, 0x9a); }

/* A buffer's bytes drawn as characters, none of them taken as a command. */
void vdp_print_buffer(int bufferId)     { SEND(23, 0, 0x9b, W(bufferId)); }

/* The VDP's variables, which turn features on and hold settings. */
void vdp_set_variable(uint16_t variableId, uint16_t value)
{
    SEND(23, 0, 0xf8, W(variableId), W(value));
}

void vdp_clear_variable(uint16_t variableId) { SEND(23, 0, 0xf9, W(variableId)); }
