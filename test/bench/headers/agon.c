/* A small Agon program as it is usually written: a page of its own code
 * and the headers it needs, which are most of what the compiler reads.
 *
 * The benchmark's other inputs have their headers folded in, or none. This
 * one is compiled as it stands, so acc opens each header from
 * /lib/acc/include as it would on the machine, and the cycles are counted
 * over the source and every header it reads (see bench.sh).
 *
 * Compiled to an object and not run: it calls MOS and the VDP.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <agon/keyboard.h>
#include <agon/mos.h>
#include <agon/vdp.h>

#define GRID_W 16
#define GRID_H 12

typedef struct {
    uint8_t x, y;
    int8_t  dx, dy;
    bool    alive;
} Mover;

static uint8_t grid[GRID_H][GRID_W];
static Mover   movers[8];

static void grid_clear(void)
{
    memset(grid, 0, sizeof grid);
}

static void mover_step(Mover *m)
{
    int nx = m->x + m->dx, ny = m->y + m->dy;

    if (nx < 0 || nx >= GRID_W)
        m->dx = (int8_t) -m->dx;
    else
        m->x = (uint8_t) nx;
    if (ny < 0 || ny >= GRID_H)
        m->dy = (int8_t) -m->dy;
    else
        m->y = (uint8_t) ny;
    grid[m->y][m->x]++;
}

static void draw(void)
{
    int x, y;

    vdp_clear_screen();
    for (y = 0; y < GRID_H; y++) {
        for (x = 0; x < GRID_W; x++)
            putchar(grid[y][x] ? '#' : '.');
        putchar('\n');
    }
}

int main(int argc, char **argv)
{
    int steps = argc > 1 ? atoi(argv[1]) : 20, i, s;
    char line[32];

    for (i = 0; i < 8; i++) {
        movers[i].x = (uint8_t) (i * 2);
        movers[i].y = (uint8_t) i;
        movers[i].dx = (int8_t) (i & 1 ? 1 : -1);
        movers[i].dy = 1;
        movers[i].alive = true;
    }
    for (s = 0; s < steps; s++) {
        grid_clear();
        for (i = 0; i < 8; i++)
            if (movers[i].alive)
                mover_step(&movers[i]);
        draw();
    }
    snprintf(line, sizeof line, "%d steps\r\n", steps);
    mos_puts(line, strlen(line), 0);

    return 0;
}
