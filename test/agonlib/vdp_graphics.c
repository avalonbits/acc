/* <agon/vdp/graphics.h> against libagon: see capture.h. Coordinates go
 * negative and past 255 so that both bytes of each are seen, and the
 * calls that check their arguments are called on each side of the check. */
#include "capture.h"
#include <agon/vdp.h>

#ifdef AGONDEV
#define NAMED(x, hex) expect(#x, hex)
#else
#define NAMED(x, hex) do { ncaught = 0; x; sent(#x); } while (0)
#endif

static const int path3[] = { 10, 20, 300, -40, 150, 600 };
static const int path4[] = { -1, -2, 1000, 20, 900, 700, 5, 400 };
static const int path_odd[] = { 1, 2, 3, 4, 5, 6, 7 };
static const int path2[] = { 1, 2, 3, 4 };

int main(void)
{
    CALL(vdp_clear_graphics());
    CALL(vdp_clg());
    CALL(vdp_set_graphics_colour(GCOLMODE_EOR, 5));
    CALL(vdp_set_graphics_colour(0, 200));
    CALL(vdp_gcol(GCOLMODE_OR_INV_COLOUR, 130));
    CALL(vdp_set_graphics_fg_colour(1, 9));
    CALL(vdp_set_graphics_fg_colour(2, 250));
    CALL(vdp_set_graphics_bg_colour(3, 4));
    CALL(vdp_set_graphics_bg_colour(0, 133));

    CALL(vdp_set_graphics_viewport(100, 50, 1279, 1023));
    CALL(vdp_set_graphics_viewport(-5, -300, 70000, 256));
    CALL(vdp_graphics_origin(640, 512));
    CALL(vdp_graphics_origin(-1, 255));

    /* libagon's puts the thickness over the second 23 of VDU 23, 23, n
     * and sends 23, n, 0 -- which the VDP reads as VDU 23, n, the start of
     * some other command altogether (for 3, redefining character 3, which
     * eats the next eight bytes sent). So it is held to what the VDP's
     * vdu_sys reads instead. */
#ifndef AGONDEV
    CALL(vdp_set_line_thickness(3));
    CALL(vdp_set_line_thickness(300));
#else
    expect("vdp_set_line_thickness(3)", "17 17 03");
    expect("vdp_set_line_thickness(300)", "17 17 2c");
#endif
    CALL(vdp_set_dotdash_line_pattern(0xaa, 0x55, 0xff, 0x00, 0x0f, 0xf0, 0x81, 0x7e));
    CALL(vdp_set_dotdash_pattern_length(0));
    CALL(vdp_set_dotdash_pattern_length(64));
    CALL(vdp_set_dotdash_pattern_length(65));
    CALL(vdp_set_dotdash_pattern_length(1000));
    CALL(vdp_set_dotdash_pattern_length(-1));

    CALL(vdp_plot(0x45, 256, -256));
    CALL(vdp_plot(0x1ff, -32768, 32767));
    CALL(vdp_move_to(0, 0));
    CALL(vdp_move_to(-640, 1023));
    CALL(vdp_line_to(1279, -1));
    CALL(vdp_line(1, 2, 300, 400));
    CALL(vdp_dotdash_line_to(-20, 700));
    CALL(vdp_dotdash_line(640, 0, 0, 512));
    CALL(vdp_point(320, -240));

    CALL(vdp_line_fill_leftright_to_nonbg(100, 200));
    CALL(vdp_line_fill_leftright_to_fg(-100, 300));
    CALL(vdp_line_fill_right_to_bg(256, 257));
    CALL(vdp_line_fill_right_to_nonfg(0, -1));

    CALL(vdp_triangle(10, 20, 300, -40, 150, 600));
    CALL(vdp_filled_triangle(-10, 20, 30, 400, 1000, 6));
    CALL(vdp_rectangle(10, 20, 300, 400));
    CALL(vdp_filled_rectangle(-300, -400, 256, 255));
    CALL(vdp_parallelogram(0, 0, 100, 50, 300, 50));
    CALL(vdp_parallelogram(-10, 700, 300, -5, 1000, 20));
    CALL(vdp_filled_parallelogram(0, 0, 100, 50, 300, 50));
    CALL(vdp_circle(640, 512, 100));
    CALL(vdp_circle(-5, 300, 1000));
    CALL(vdp_filled_circle(320, 256, 256));

    CALL(vdp_arc(640, 512, 740, 512, 640, 612));
    CALL(vdp_segment(-640, 512, 740, -512, 0, 300));
    CALL(vdp_sector(100, 100, 200, 300, 400, 500));

    CALL(vdp_copy_rectangle(0, 0, 100, 300, 640, 512));
    CALL(vdp_move_rectangle(-10, 256, 700, 20, -1, 1000));

    CALL(vdp_fill_path(path3, sizeof path3));
    CALL(vdp_fill_path(path4, sizeof path4));
    CALL(vdp_fill_path(path4, sizeof path4 - 2));
    CALL(vdp_fill_path(path_odd, sizeof path_odd));
    CALL(vdp_fill_path(path2, sizeof path2));
    CALL(vdp_fill_path(path3, 17));
    CALL(vdp_fill_path(path3, -1));

    CALL(vdp_plot_bitmap(300, -300));

    /* What libagon does not have. */
    NEW(vdp_move_by(-10, 300), "19 00 f6 ff 2c 01");
    NEW(vdp_line_by(5, -1), "19 01 05 00 ff ff");
    NEW(vdp_line_to_omit_last(100, 200), "19 0d 64 00 c8 00");
    NEW(vdp_line_to_omit_first(100, 200), "19 25 64 00 c8 00");
    NEW(vdp_line_to_omit_both(-1, -2), "19 2d ff ff fe ff");
    NEW(vdp_dotdash_line_to_omit_last(640, 480), "19 1d 80 02 e0 01");
    NEW(vdp_dotdash_line_to_omit_first(1279, 1023), "19 35 ff 04 ff 03");
    NEW(vdp_dotdash_line_to_omit_both(0, 0), "19 3d 00 00 00 00");
    NEW(vdp_flood_fill_to_nonbg(320, 256), "19 85 40 01 00 01");
    NEW(vdp_flood_fill_to_fg(-320, -256), "19 8d c0 fe 00 ff");
    NEW(vdp_ellipse(640, 512, 840, 512, 700, 700),
        "19 04 80 02 00 02 19 04 48 03 00 02 19 c5 bc 02 bc 02");
    NEW(vdp_filled_ellipse(0, 0, -100, 5, 20, -50),
        "19 04 00 00 00 00 19 04 9c ff 05 00 19 cd 14 00 ce ff");
    NEW(vdp_path_point(300, -300), "19 dd 2c 01 d4 fe");

    /* And the PLOT codes, put together from their names. NEW would have
     * acc's build write the names out as the numbers they stand for, so
     * these are caught the way CALL catches them but under the names. */
    NAMED(vdp_plot(PLOT_LINE | PLOTMODE_MOVE_RELATIVE, 1, 2), "19 00 01 00 02 00");
    NAMED(vdp_plot(PLOT_LINE_OMIT_LAST | PLOTMODE_FG_RELATIVE, 1, 2), "19 09 01 00 02 00");
    NAMED(vdp_plot(PLOT_DOTDASH | PLOTMODE_INVERT_RELATIVE, 1, 2), "19 12 01 00 02 00");
    NAMED(vdp_plot(PLOT_DOTDASH_OMIT_LAST | PLOTMODE_BG_RELATIVE, 1, 2), "19 1b 01 00 02 00");
    NAMED(vdp_plot(PLOT_LINE_OMIT_FIRST | PLOTMODE_MOVE_ABSOLUTE, 1, 2), "19 24 01 00 02 00");
    NAMED(vdp_plot(PLOT_LINE_OMIT_BOTH | PLOTMODE_FG_ABSOLUTE, 1, 2), "19 2d 01 00 02 00");
    NAMED(vdp_plot(PLOT_DOTDASH_OMIT_FIRST | PLOTMODE_INVERT_ABSOLUTE, 1, 2), "19 36 01 00 02 00");
    NAMED(vdp_plot(PLOT_DOTDASH_OMIT_BOTH | PLOTMODE_BG_ABSOLUTE, 1, 2), "19 3f 01 00 02 00");
    NAMED(vdp_plot(PLOT_POINT | PLOTMODE_BG_ABSOLUTE, 1, 2), "19 47 01 00 02 00");
    NAMED(vdp_plot(PLOT_FILL_LEFTRIGHT_TO_NONBG | PLOTMODE_FG_ABSOLUTE, 1, 2), "19 4d 01 00 02 00");
    NAMED(vdp_plot(PLOT_TRIANGLE | PLOTMODE_FG_RELATIVE, 1, 2), "19 51 01 00 02 00");
    NAMED(vdp_plot(PLOT_FILL_RIGHT_TO_BG | PLOTMODE_FG_ABSOLUTE, 1, 2), "19 5d 01 00 02 00");
    NAMED(vdp_plot(PLOT_RECTANGLE | PLOTMODE_BG_ABSOLUTE, 1, 2), "19 67 01 00 02 00");
    NAMED(vdp_plot(PLOT_FILL_LEFTRIGHT_TO_FG | PLOTMODE_FG_ABSOLUTE, 1, 2), "19 6d 01 00 02 00");
    NAMED(vdp_plot(PLOT_PARALLELOGRAM | PLOTMODE_FG_ABSOLUTE, 1, 2), "19 75 01 00 02 00");
    NAMED(vdp_plot(PLOT_FILL_RIGHT_TO_NONFG | PLOTMODE_FG_ABSOLUTE, 1, 2), "19 7d 01 00 02 00");
    NAMED(vdp_plot(PLOT_FLOOD_TO_NONBG | PLOTMODE_BG_ABSOLUTE, 1, 2), "19 87 01 00 02 00");
    NAMED(vdp_plot(PLOT_FLOOD_TO_FG | PLOTMODE_FG_RELATIVE, 1, 2), "19 89 01 00 02 00");
    NAMED(vdp_plot(PLOT_CIRCLE | PLOTMODE_MOVE_ABSOLUTE, 1, 2), "19 94 01 00 02 00");
    NAMED(vdp_plot(PLOT_FILLED_CIRCLE | PLOTMODE_FG_RELATIVE, 1, 2), "19 99 01 00 02 00");
    NAMED(vdp_plot(PLOT_ARC | PLOTMODE_FG_ABSOLUTE, 1, 2), "19 a5 01 00 02 00");
    NAMED(vdp_plot(PLOT_SEGMENT | PLOTMODE_FG_ABSOLUTE, 1, 2), "19 ad 01 00 02 00");
    NAMED(vdp_plot(PLOT_SECTOR | PLOTMODE_FG_ABSOLUTE, 1, 2), "19 b5 01 00 02 00");
    NAMED(vdp_plot(PLOT_COPY_MOVE | PLOTMODE_BG_ABSOLUTE, 1, 2), "19 bf 01 00 02 00");
    NAMED(vdp_plot(PLOT_ELLIPSE | PLOTMODE_FG_ABSOLUTE, 1, 2), "19 c5 01 00 02 00");
    NAMED(vdp_plot(PLOT_FILLED_ELLIPSE | PLOTMODE_FG_ABSOLUTE, 1, 2), "19 cd 01 00 02 00");
    NAMED(vdp_plot(PLOT_PATH | PLOTMODE_FG_ABSOLUTE, 1, 2), "19 dd 01 00 02 00");
    NAMED(vdp_plot(PLOT_BITMAP | PLOTMODE_BG_ABSOLUTE, 1, 2), "19 ef 01 00 02 00");

    done();
    return 0;
}
