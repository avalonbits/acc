/* <agon/vdp/screen.h>'s calls that wait for the VDP, against the VDP: each
 * asks after something the program has just set up, so the answer is
 * known -- the mode it chose, where it put the cursor, a character it
 * drew, a colour it defined and plotted. */
#include "result.h"
#include <agon/vdp.h>

static void plot_point(int x, int y)
{
    static unsigned char b[6] = { 25, 69 };        /* PLOT 69: a point */

    b[2] = (unsigned char) x;
    b[3] = (unsigned char) (x >> 8);
    b[4] = (unsigned char) y;
    b[5] = (unsigned char) (y >> 8);
    mos_puts((char *) b, 6, 0);
}

int main(void)
{
    uint8_t x = 99, y = 99;

    say("mode %d\n", vdp_mode(8));
    vdp_get_scr_dims(true);
    say("dims %d %d %d %d colours %d mode %d\n", sys_vars->scrCols, sys_vars->scrRows,
        sys_vars->scrWidth, sys_vars->scrHeight, sys_vars->scrColours, sys_vars->scrMode);

    vdp_cursor_tab(10, 5);
    vdp_return_text_cursor_position(&x, &y);
    say("cursor %d %d\n", x, y);
    vdp_move_cursor_relative(8, 8);
    vdp_request_text_cursor_position(true);
    say("moved %d %d\n", sys_vars->cursorX, sys_vars->cursorY);

    vdp_cursor_tab(3, 2);
    vdp_send_to_screen('X');
    say("char %c\n", vdp_return_ascii_code_at_position(3, 2));
    say("blank %d\n", vdp_return_ascii_code_at_position(4, 2));
    vdp_request_ascii_code_at_position(3, 2, true);
    say("requested %c\n", sys_vars->scrchar);

    vdp_set_pixel_coordinates();
    say("char at graphics %c\n", vdp_return_ascii_code_at_graphics_position(24, 16));

    vdp_define_colour(5, 255, 0x40, 0x80, 0xc0);
    say("palette 5 %06x index %d\n", (int) vdp_return_palette_entry_colour(5),
        vdp_return_palette_entry_index(5));
    vdp_request_palette_entry(5, true);
    say("requested palette %06x\n", (int) sys_vars->scrpixel);
    mos_puts("\x12\x00\x05", 3, 0);             /* GCOL 0, 5 */
    plot_point(100, 100);
    vdp_flush_drawing_commands();
    say("pixel %06x index %d\n", (int) vdp_return_pixel_colour(100, 100),
        sys_vars->scrpixelIndex);
    say("background %06x\n", (int) vdp_return_pixel_colour(200, 200));

    vdp_request_rtc(true);
    say("rtc answered %d\n", (sys_vars->vdp_pflags & vdp_pflag_rtc) != 0);
    say("keycode %d keymap %d\n", vdp_getKeyCode(), vdp_getKeyMap(0));
    vdp_waitKeyUp();
    say("key up\n");

    return finish();
}
