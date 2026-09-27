/*
 * agon/vdp.h -- the VDP, as libagon names it, and the rest of what VDP
 * 2.16.0 takes.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Every call here sends one VDU command -- or, for the few that read an
 * answer back, sends a request and reads what MOS puts in its system
 * variables. The names, the arguments and the bytes sent are libagon's,
 * so that a program written against one is built against the other and
 * draws the same; test/agonlib.sh holds each to the bytes libagon sends.
 * Past libagon, every command VDP 2.16.0 has is here as well, named the
 * way libagon names its neighbours, and test/vdpsync.sh plays each into
 * the VDP firmware itself to hold it to what the VDP takes.
 *
 * The declarations are in a header for each part of the VDP, all of them
 * included here: <agon/vdp/screen.h> for text, colour, modes and the
 * system commands, graphics.h for PLOT and the shapes, bitmap.h for
 * bitmaps, sprites, fonts, the mouse and contexts, buffer.h for the
 * buffered commands and audio.h for sound.
 */
#pragma once
#ifndef ACC_AGON_VDP_H
#define ACC_AGON_VDP_H

#include <stdbool.h>
#include <stdint.h>

#include <agon/mos.h>

/* A structure of VDU bytes sent as it lies in memory -- how libagon has a
 * program send one of the typedefs below. */
#define VDP_PUTS(S) mos_puts((char *) &(S), sizeof (S), 0)

#define BACKGROUND_COL_OFFSET 128

enum {
    BLACK = 0,
    RED,
    GREEN,
    YELLOW,
    BLUE,
    MAGENTA,
    CYAN,
    WHITE,
    BRIGHT_BLACK,
    BRIGHT_RED,
    BRIGHT_GREEN,
    BRIGHT_YELLOW,
    BRIGHT_BLUE,
    BRIGHT_MAGENTA,
    BRIGHT_CYAN,
    BRIGHT_WHITE
};

/* libagon's shapes of VDU commands, for VDP_PUTS. A struct here has no
 * padding, so each lies in memory as the bytes the VDP reads. */
typedef struct { uint8_t A; } VDU_A;
typedef struct { uint8_t A; uint8_t n; } VDU_A_n;
typedef struct { uint8_t A; uint8_t CMD; } VDU_A_CMD;
typedef struct { uint8_t A; uint8_t CMD; uint8_t n; } VDU_A_CMD_n;
typedef struct { uint8_t A; uint8_t CMD; uint8_t a; uint8_t b; } VDU_A_CMD_a_b;
typedef struct { uint8_t A; uint8_t c; uint8_t r; } VDU_A_c_r;
typedef struct { uint8_t A; uint8_t a; uint8_t b; uint8_t c; uint8_t d; } VDU_A_a_b_c_d;
typedef struct { uint8_t A; uint8_t l; uint8_t p; uint8_t r; uint8_t g; uint8_t b; } VDU_A_l_p_r_g_b;
typedef struct { uint8_t A; uint16_t x; uint16_t y; } VDU_A_x_y;
typedef struct { uint8_t A; uint8_t CMD; uint16_t x; uint16_t y; } VDU_A_CMD_x_y;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; } VDU_A_B_CMD;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint8_t n; } VDU_A_B_CMD_n;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint16_t x; uint16_t y;
                 uint8_t r; uint8_t g; uint8_t b; uint8_t a; } VDU_A_B_CMD_x_y_rgba;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint16_t x; uint16_t y; } VDU_A_B_CMD_x_y;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint8_t b0; } VDU_A_B_CMD_B;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint16_t w0; } VDU_A_B_CMD_W;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint16_t w0; uint8_t b0; } VDU_A_B_CMD_W_B;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint16_t w0; uint16_t w1; } VDU_A_B_CMD_W_W;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint8_t b0; uint16_t w1; } VDU_A_B_CMD_B_W;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint16_t w0; uint16_t w1; uint8_t b2; } VDU_A_B_CMD_W_W_B;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint8_t b0; uint8_t b1; uint8_t b2; } VDU_A_B_CMD_B_B_B;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint8_t b0; uint8_t b1; uint8_t b2;
                 uint8_t b3; } VDU_A_B_CMD_B_B_B_B;
typedef struct { uint8_t A; uint8_t B; uint8_t b0; uint8_t b1; uint8_t b2; uint8_t b3;
                 uint8_t b4; uint8_t b5; uint8_t b6; uint8_t b7; } VDU_A_B_ui8x8;
typedef struct { uint8_t A; uint16_t w0; uint16_t w1; uint16_t w2; uint16_t w3; } VDU_A_ui16x4;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint8_t n; uint8_t b0; uint8_t b1;
                 uint8_t b2; uint8_t b3; uint8_t b4; uint8_t b5; uint8_t b6;
                 uint8_t b7; } VDU_A_B_CMD_n_ui8x8;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint16_t BID; uint8_t CMD; } VDU_ADV_CMD;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint16_t BID; uint8_t CMD; uint8_t b0; } VDU_ADV_CMD_B;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint16_t BID; uint8_t CMD; uint16_t w0; } VDU_ADV_CMD_W;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint16_t BID; uint8_t CMD; uint16_t w0;
                 uint16_t w1; } VDU_ADV_CMD_W_W;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint16_t BID; uint8_t CMD; uint16_t w0;
                 uint16_t w1; uint16_t w2; } VDU_ADV_CMD_W_W_W;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint16_t BID; uint8_t CMD; uint8_t b0;
                 uint16_t w1; } VDU_ADV_CMD_B_W;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint16_t BID; uint8_t CMD; uint8_t b0;
                 uint16_t w1; uint16_t w2; } VDU_ADV_CMD_B_W_W;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint16_t BID; uint8_t CMD; uint24_t ww0; } VDU_ADV_CMD_WW;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint16_t BID; uint8_t CMD; uint24_t ww0;
                 uint16_t w1; } VDU_ADV_CMD_WW_W;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint8_t channel; uint8_t CMD; } VDU_AUDIO_CMD;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint8_t channel; uint8_t CMD; uint8_t b1; } VDU_AUDIO_CMD_B;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint8_t channel; uint8_t CMD; uint16_t w1; } VDU_AUDIO_CMD_W;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint8_t channel; uint8_t CMD; uint8_t b1;
                 uint16_t w2; } VDU_AUDIO_CMD_B_W;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint8_t channel; uint8_t CMD; uint8_t b1;
                 uint16_t w2; uint8_t b3; } VDU_AUDIO_CMD_B_W_B;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint8_t channel; uint8_t CMD; uint8_t b1;
                 uint16_t w2; uint16_t w3; } VDU_AUDIO_CMD_B_W_W;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint8_t channel; uint8_t CMD; uint8_t b1;
                 uint16_t w2; uint16_t w3; uint8_t b4; uint16_t w5; } VDU_AUDIO_CMD_B_W_W_B_W;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint8_t sample; uint8_t CMD; uint8_t TYPE;
                 uint24_t length; } VDU_AUDIO_LOAD_SAMPLE;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint8_t sample; uint8_t CMD; uint8_t b1;
                 uint24_t ww2; } VDU_AUDIO_CMD_B_WW;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint8_t sample; uint8_t CMD; uint8_t b1;
                 uint16_t w2; uint24_t ww3; } VDU_AUDIO_CMD_B_W_WW;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint8_t channel; uint8_t CMD; uint8_t b1;
                 uint8_t b2; uint8_t b3; uint16_t w4; } VDU_AUDIO_CMD_B_B_B_W;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint8_t channel; uint8_t CMD; uint24_t ww1; } VDU_AUDIO_CMD_WW;
typedef struct { uint8_t A; uint8_t B; uint8_t C; uint8_t channel; uint8_t CMD; uint8_t b1;
                 uint8_t b2; } VDU_AUDIO_CMD_B_B;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint8_t SUBCMD; uint16_t buffer_id; } VDU_FONT;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint8_t SUBCMD; uint16_t buffer_id;
                 uint8_t b0; uint8_t b1; } VDU_FONT_B_B;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint8_t SUBCMD; uint16_t buffer_id;
                 uint8_t b0; uint8_t b1; uint8_t b2; uint8_t b3; } VDU_FONT_B_B_B_B;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint8_t SUBCMD; } VDU_CONTEXT;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint8_t SUBCMD; uint8_t b0; } VDU_CONTEXT_B;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint8_t SUBCMD; } VDU_MOUSE;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint8_t SUBCMD; uint16_t w0; } VDU_MOUSE_W;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint8_t SUBCMD; uint16_t w0; uint16_t w1; } VDU_MOUSE_W_W;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint8_t SUBCMD; uint8_t b0; } VDU_MOUSE_B;
typedef struct { uint8_t A; uint8_t B; uint8_t CMD; uint8_t SUBCMD; uint24_t ww0; } VDU_MOUSE_WW;

#include <agon/vdp/screen.h>
#include <agon/vdp/graphics.h>
#include <agon/vdp/bitmap.h>
#include <agon/vdp/buffer.h>
#include <agon/vdp/audio.h>

#endif
