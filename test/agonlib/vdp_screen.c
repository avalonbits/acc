/* <agon/vdp/screen.h> against libagon: every call's bytes, with arguments
 * at the edges libagon's code tests -- a mode of 255, a null pointer --
 * and past a byte where an argument is two. See capture.h. One of
 * libagon's is wrong, and is held to the VDP instead, below. The calls that
 * wait for the VDP's answer are asked without waiting, since the bytes
 * never reach one here. */
#include "capture.h"
#include <agon/vdp.h>

static int printf_mode_result;
static uint8_t glyph[8] = { 0x18, 0x3c, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x00 };

int main(void)
{
    CALL(vdp_send_to_printer('Q'));
    CALL(vdp_enable_printer());
    CALL(vdp_disable_printer());
    CALL(vdp_write_at_graphics_cursor());
    CALL(vdp_write_at_text_cursor());
    CALL(vdp_bell());
    CALL(vdp_cursor_left());
    CALL(vdp_cursor_right());
    CALL(vdp_cursor_down());
    CALL(vdp_cursor_up());
    CALL(vdp_clear_screen());
    CALL(vdp_carriage_return());
    CALL(vdp_page_mode_on());
    CALL(vdp_page_mode_off());
    CALL(vdp_set_text_colour(3));
    CALL(vdp_set_text_colour(BRIGHT_WHITE));
    CALL(vdp_set_text_bg_colour(BLUE));
    CALL(vdp_define_colour(1, 255, 10, 200, 30));
    CALL(vdp_reset_graphics());
    /* VDU 21 stops the VDU until VDU 6: sent together, so the VDP is back
     * in step when the replay asks. */
    CALL((vdp_disable_screen(), vdp_enable_screen()));
    CALL(printf_mode_result = vdp_mode(8));
    CALL(printf_mode_result = vdp_mode(255));
    CALL(vdp_reset_viewports());
    CALL(vdp_set_text_viewport(2, 20, 30, 3));
    CALL(vdp_cursor_home());
    CALL(vdp_cursor_tab(5, 7));
    CALL(vdp_outchar(12));
    CALL(vdp_send_to_screen('A'));
    CALL(vdp_cursor_enable(true));
    CALL(vdp_cursor_enable(false));
    CALL(vdp_scroll_screen(1, 8));
    CALL(vdp_scroll_screen_extent(2, 3, 4));
    CALL(vdp_cursor_behaviour(0x10, 0xef));
    CALL(vdp_redefine_character(200, 1, 2, 3, 4, 5, 6, 7, 8));
    CALL(vdp_redefine_character_special(5, 8, 7, 6, 5, 4, 3, 2, 1));
    CALL(vdp_define_character(65, glyph));
    CALL(vdp_define_character(66, 0));
    CALL(vdp_reset_system_font());
    CALL(vdp_set_cursor_start_line(3));
    CALL(vdp_set_cursor_end_line(7));
    CALL(vdp_set_cursor_start_column(1));
    CALL(vdp_set_cursor_end_column(6));
    /* libagon sends x and y as two bytes each, and VDP 2.16.0 reads each
     * as one signed byte (vdu_sys.h, VDP_CURSOR_MOVE): its -2, 3 moves by
     * -2, -1 and leaves 03 00 to be read as two commands of their own. */
#ifndef AGONDEV
    CALL(vdp_move_cursor_relative(-2, 3));
#else
    expect("vdp_move_cursor_relative(-2, 3)", "17 00 8c fe 03");
#endif
    CALL(vdp_set_text_viewport_via_plot());
    CALL(vdp_set_graphics_viewport_via_plot());
    CALL(vdp_set_graphics_origin_via_plot());
    CALL(vdp_move_graphics_origin_and_viewport());
    CALL(vdp_request_text_cursor_position(false));
    CALL(vdp_request_ascii_code_at_position(300, 5, false));
    CALL(vdp_request_ascii_code_at_graphics_position(-5, 1000, false));
    CALL(vdp_request_pixel_colour(640, 480, false));
    CALL(vdp_request_palette_entry(9, false));
    CALL(vdp_get_scr_dims(false));
    CALL(vdp_request_rtc(false));
    CALL(vdp_set_keyboard_locale(1));
    CALL(vdp_keyboard_control(500, 33, 2));
    CALL(vdp_control_keys(false));
    CALL(vdp_control_keys(true));
    CALL(vdp_logical_scr_dims(true));
    CALL(vdp_logical_scr_dims(false));
    CALL(vdp_set_logical_coordinates());
    CALL(vdp_set_pixel_coordinates());
    CALL(vdp_legacy_modes(true));
    CALL(vdp_legacy_modes(false));
    CALL(vdp_swap());
    CALL(vdp_flush_drawing_commands());
    CALL(vdp_console_mode(true));
    CALL(vdp_console_mode(false));
    CALL(vdp_set_variable(0x1234, 500));
    CALL(vdp_clear_variable(0x1234));
    printf_mode_result = vdp_vdu_init() == sys_vars;
    CALL(vdp_vdu_init());

    NEW(vdp_backspace(), "7f");
    NEW(vdp_general_poll(7), "17 00 80 07");
    NEW(vdp_set_rtc(2026, 8, 24, 13, 5, 9), "17 00 87 01 2e 08 18 0d 05 09");
    NEW(vdp_check_key(0x45), "17 00 99 45");
    NEW(vdp_temp_paged_mode(), "17 00 9a");
    NEW(vdp_print_buffer(0x1234), "17 00 9b 34 12");

    /* Terminal mode takes the VDU commands away until the VDP is reset. */
    CALL_UNREPLAYED(vdp_terminal_mode());

    done();
    return printf_mode_result ? 0 : 1;
}
