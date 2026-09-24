/* <agon/vdp/graphics.h> against the VDP: shapes drawn, and pixels read
 * back from inside them and just outside, in pixel coordinates with the
 * origin at the bottom left. */
#include "result.h"
#include <agon/vdp.h>

static void at(const char *what, int x, int y)
{
    say("%s (%d,%d) %d\n", what, x, y, (int) vdp_return_pixel_colour(x, y) != 0);
}

int main(void)
{
    vdp_mode(8);
    vdp_set_pixel_coordinates();
    vdp_clear_graphics();
    vdp_set_graphics_colour(GCOLMODE_COLOUR, BRIGHT_WHITE);

    vdp_filled_rectangle(10, 10, 30, 20);
    vdp_flush_drawing_commands();
    at("rectangle inside", 20, 15);
    at("rectangle outside", 40, 15);

    vdp_filled_circle(100, 100, 10);
    vdp_flush_drawing_commands();
    at("circle centre", 100, 100);
    at("circle just in", 108, 100);
    at("circle outside", 113, 100);

    vdp_filled_triangle(150, 10, 190, 10, 170, 50);
    vdp_flush_drawing_commands();
    at("triangle inside", 170, 20);
    at("triangle outside", 152, 45);

    vdp_line(10, 150, 60, 150);
    vdp_flush_drawing_commands();
    at("thin line", 30, 150);
    at("beside thin line", 30, 152);
    vdp_set_line_thickness(7);
    vdp_line(10, 170, 60, 170);
    vdp_flush_drawing_commands();
    at("thick line", 30, 170);
    at("beside thick line", 30, 172);
    vdp_set_line_thickness(1);

    vdp_move_to(200, 200);
    vdp_move_by(5, 5);
    vdp_point(205, 205);
    vdp_flush_drawing_commands();
    at("point", 205, 205);

    vdp_graphics_origin(100, 0);
    vdp_point(0, 120);
    vdp_graphics_origin(0, 0);
    vdp_flush_drawing_commands();
    at("point from moved origin", 100, 120);

    return finish();
}
