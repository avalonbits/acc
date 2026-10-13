# `agon/vdp/bitmap.h`

Bitmaps and sprites (VDU 23, 27), the mouse (VDU 23, 0, &89), a
character drawn as a bitmap (&92), fonts (&95), the affine transform
(&96), the tile engine (&C2), the copper (&C4) and contexts (&C8). A
program includes [agon/vdp.h](agon-vdp.md), which includes this.

Each call sends one VDU command (a few send two or three) in one call
into MOS, and none waits for an answer. The bytes are libagon's, name
for name, except where libagon sends what VDP 2.16.0 does not take;
those places are said below. The calls libagon does not have send what
the VDP's own source reads at v2.16.0.

In the sequences, a plain argument is one byte, its low byte when the
argument is wider; `x;` is two bytes, low first; `x(3)` is three.

An 8-bit bitmap number `n` is the bitmap in buffer 64000 + `n`. The
calls named `vdp_adv_` take the 16-bit buffer ID itself; buffers are in
[agon/vdp/buffer.h](agon-vdp-buffer.md).

## `VDP_VAR_AFFINE_TRANSFORM`, `VDP_VAR_HW_SPRITES`, `VDP_VAR_TILE_ENGINE`, `VDP_VAR_COPPER`

Parts of the VDP that are off until a program sets their VDP variable
(VDU 23, 0, &F8, id; value;, `vdp_set_variable` in
[agon/vdp/screen.h](agon-vdp-screen.md)). While one is unset its
commands take no bytes at all: the VDP reads what follows as VDU codes.

| Name                       | ID      | Turns on                     |
|----------------------------|---------|------------------------------|
| `VDP_VAR_AFFINE_TRANSFORM` | &0001   | `vdp_adv_use_affine_matrix`  |
| `VDP_VAR_HW_SPRITES`       | &0002   | `vdp_set_hardware_sprite`    |
| `VDP_VAR_TILE_ENGINE`      | &0300   | the `vdp_tile` calls         |
| `VDP_VAR_COPPER`           | &0310   | the `vdp_copper` calls       |

`vdp_copper_enable` sets the copper's for you.

## `vdp_select_bitmap`

    void vdp_select_bitmap(int n);

VDU 23, 27, 0, n. Select bitmap `n` for the commands that follow.

## `vdp_load_bitmap`, `LOAD_BMAP_BLOCK`

    void vdp_load_bitmap(int width, int height, uint8_t *data);

VDU 23, 27, 1, width; height; and then `width * height * 4` bytes from
`data`: RGBA8888 pixels, into the selected bitmap. The pixels go to MOS
in pieces of at most `LOAD_BMAP_BLOCK` (65535) bytes, and none at all
when there are none.

## `vdp_load_bitmap_file`

    int vdp_load_bitmap_file(const char *fname, int width, int height);

`vdp_load_bitmap` with the pixels read from the file `fname`, 4 KB at a
time. The answer is 0, or -1 when the file cannot be opened or the 4 KB
buffer cannot be had (nothing is sent then), or when the file holds
fewer than `width * height * 4` bytes: the bytes it lacks are sent as
zeros all the same, since the VDP waits for every one.

## `vdp_solid_bitmap`

    void vdp_solid_bitmap(int width, int height, int r, int g, int b,
                          int a);

VDU 23, 27, 2, width; height; r, g, b, a. Make the selected bitmap
`width` by `height` of one colour, the four bytes of an RGBA8888 pixel.

## `vdp_draw_bitmap`

    void vdp_draw_bitmap(int x, int y);

VDU 23, 27, 3, x; y;. Draw the selected bitmap at (`x`, `y`).

## `vdp_capture_bitmap`, `vdp_adv_capture_bitmap`

    void vdp_capture_bitmap(uint16_t top, uint16_t left,
                            uint16_t bottom, uint16_t right,
                            uint8_t bitmapID);
    void vdp_adv_capture_bitmap(uint16_t top, uint16_t left,
                                uint16_t bottom, uint16_t right,
                                int bufferId);

Capture the screen between (`left`, `top`) and (`right`, `bottom`). The
VDP captures the rectangle between the last two graphics positions, so
both send two moves first, VDU 25, 4, left; top; and VDU 25, 4, right;
bottom;, and then:

| Call                     | Capture                           |
|--------------------------|-----------------------------------|
| `vdp_capture_bitmap`     | VDU 23, 27, 1, bitmapID; 0;       |
| `vdp_adv_capture_bitmap` | VDU 23, 27, &21, bufferId; 0;     |

Command 1 with a height of 0 is the capture, into the bitmap its width
names. libagon sends a single byte, &21, in place of that command; acc
sends the command. Command &21 with a height of 0 takes no format byte.

Note the order of the arguments: top before left.

## `vdp_adv_select_bitmap`

    void vdp_adv_select_bitmap(int bufferId);

VDU 23, 27, &20, bufferId;. Select the bitmap in buffer `bufferId`.

## `vdp_adv_bitmap_from_buffer`, `VDP_BITMAP_RGBA8888`, `VDP_BITMAP_RGBA2222`, `VDP_BITMAP_MONO`, `VDP_BITMAP_NATIVE`

    void vdp_adv_bitmap_from_buffer(int width, int height, int format);

VDU 23, 27, &21, width; height; format. Make the selected buffer a
bitmap of `width` by `height` pixels in `format`:

| Name                  | Value | Pixels                              |
|-----------------------|-------|-------------------------------------|
| `VDP_BITMAP_RGBA8888` | 0     | 4 bytes each                        |
| `VDP_BITMAP_RGBA2222` | 1     | a byte each                         |
| `VDP_BITMAP_MONO`     | 2     | 1 bit each, in the graphics colour  |
| `VDP_BITMAP_NATIVE`   | 3     | the VDP's own                       |

A `height` of 0 makes it a capture instead (as
`vdp_adv_capture_bitmap`), which reads no format byte, and then no
format is sent. libagon sends one all the same, and the VDP takes it
for a command of its own.

## `vdp_select_sprite`, `vdp_clear_sprite`, `vdp_add_sprite_bitmap`, `vdp_adv_add_sprite_bitmap`, `vdp_activate_sprites`

    void vdp_select_sprite(int n);
    void vdp_clear_sprite(void);
    void vdp_add_sprite_bitmap(int n);
    void vdp_adv_add_sprite_bitmap(int bitmap_num);
    void vdp_activate_sprites(int n);

| Call                        | Sends                            |
|-----------------------------|----------------------------------|
| `vdp_select_sprite`         | VDU 23, 27, 4, n                 |
| `vdp_clear_sprite`          | VDU 23, 27, 5                    |
| `vdp_add_sprite_bitmap`     | VDU 23, 27, 6, n                 |
| `vdp_adv_add_sprite_bitmap` | VDU 23, 27, &26, bitmap_num;     |
| `vdp_activate_sprites`      | VDU 23, 27, 7, n                 |

Select sprite `n`; clear the selected sprite's frames; add bitmap `n`
(or the bitmap in buffer `bitmap_num`) to it as a frame; and make `n`
sprites active, sprites 0 to `n - 1`.

## `vdp_next_sprite_frame`, `vdp_prev_sprite_frame`, `vdp_nth_sprite_frame`

    void vdp_next_sprite_frame(void);
    void vdp_prev_sprite_frame(void);
    void vdp_nth_sprite_frame(int n);

VDU 23, 27, 8; VDU 23, 27, 9; VDU 23, 27, 10, n. Show the selected
sprite's next frame, its previous one, or frame `n`.

## `vdp_replace_sprite_frame`, `vdp_adv_replace_sprite_frame`

    void vdp_replace_sprite_frame(uint8_t bitmapID);
    void vdp_adv_replace_sprite_frame(int bufferId);

VDU 23, 27, 21, bitmapID and VDU 23, 27, &35, bufferId;. Put a bitmap,
by number or by buffer ID, in place of the selected sprite's frame.

## `vdp_show_sprite`, `vdp_hide_sprite`, `vdp_move_sprite_to`, `vdp_move_sprite_by`

    void vdp_show_sprite(void);
    void vdp_hide_sprite(void);
    void vdp_move_sprite_to(int x, int y);
    void vdp_move_sprite_by(int x, int y);

| Call                 | Sends                      |
|----------------------|----------------------------|
| `vdp_show_sprite`    | VDU 23, 27, 11             |
| `vdp_hide_sprite`    | VDU 23, 27, 12             |
| `vdp_move_sprite_to` | VDU 23, 27, 13, x; y;      |
| `vdp_move_sprite_by` | VDU 23, 27, 14, x; y;      |

Each acts on the selected sprite.

## `vdp_refresh_sprites`, `vdp_reset_sprites`, `vdp_reset_sprites_only`

    void vdp_refresh_sprites(void);
    void vdp_reset_sprites(void);
    void vdp_reset_sprites_only(void);

VDU 23, 27, 15 refreshes the sprites. VDU 23, 27, 16 resets the sprites,
the bitmaps and the screen; VDU 23, 27, 17 the sprites alone.

## `vdp_set_sprite_paint_mode`, `vdp_set_hardware_sprite`, `vdp_set_software_sprite`

    void vdp_set_sprite_paint_mode(int n);
    void vdp_set_hardware_sprite(void);
    void vdp_set_software_sprite(void);

VDU 23, 27, 18, n sets the selected sprite's paint mode, a GCOL mode.
VDU 23, 27, 19 makes it a hardware sprite, which the VDP takes only once
`VDP_VAR_HW_SPRITES` is set; VDU 23, 27, 20 a software one.

## `vdp_load_sprite_bitmaps`, `vdp_adv_load_sprite_bitmaps`

    int vdp_load_sprite_bitmaps(const char *fname_prefix,
                                const char *fname_format,
                                int width, int height, int num,
                                int bitmap_num);
    int vdp_adv_load_sprite_bitmaps(const char *fname_prefix,
                                    const char *fname_format,
                                    int width, int height, int num,
                                    int bitmap_num);

Load `num` RGBA8888 bitmaps of `width` by `height` from files, frame
`i` from the file named by `sprintf(name, fname_format, fname_prefix,
i)` (at most 255 characters), into bitmap `bitmap_num + i`: selected
with `vdp_select_bitmap`, or for the `_adv_` one with
`vdp_adv_select_bitmap` (a buffer ID), and then `vdp_load_bitmap`.

The answer is how many were loaded. It stops at the first file that
cannot be opened or is short, as libagon's does, and is 0 when the
`width * height * 4`-byte buffer cannot be had.

    n = vdp_load_sprite_bitmaps("ship", "%s%d.rgba", 16, 16, 4, 0);

## `vdp_create_sprite`, `vdp_adv_create_sprite`

    void vdp_create_sprite(int sprite, int bitmap_num, int frames);
    void vdp_adv_create_sprite(int sprite, int bitmap_num, int frames);

Select `sprite`, clear it, add `frames` frames from bitmap `bitmap_num`
onwards (`vdp_add_sprite_bitmap`, or `vdp_adv_add_sprite_bitmap` with
buffer IDs), and then `vdp_activate_sprites(sprite)`.

That last is libagon's arithmetic: it activates `sprite` sprites, which
leaves the sprite itself out. Activate `sprite + 1` afterwards to show
it.

    vdp_create_sprite(0, 0, 4);
    vdp_activate_sprites(1);
    vdp_show_sprite();

## `vdp_mouse_enable`, `vdp_mouse_disable`, `vdp_mouse_reset`

    void vdp_mouse_enable(void);
    void vdp_mouse_disable(void);
    void vdp_mouse_reset(void);

VDU 23, 0, &89, 0; VDU 23, 0, &89, 1; VDU 23, 0, &89, 2.

## `vdp_mouse_set_cursor`, `vdp_mouse_set_position`, `vdp_mouse_set_area`, `vdp_mouse_set_bitmap`

    void vdp_mouse_set_cursor(int cursorId);
    void vdp_mouse_set_position(int X, int Y);
    void vdp_mouse_set_area(int x1, int y1, int x2, int y2);
    void vdp_mouse_set_bitmap(int hotx, int hoty);

| Call                     | Sends                                   |
|--------------------------|-----------------------------------------|
| `vdp_mouse_set_cursor`   | VDU 23, 0, &89, 3, cursorId;            |
| `vdp_mouse_set_position` | VDU 23, 0, &89, 4, X; Y;                |
| `vdp_mouse_set_area`     | VDU 23, 0, &89, 5, x1; y1; x2; y2;      |
| `vdp_mouse_set_bitmap`   | VDU 23, 27, &40, hotx, hoty             |

VDP 2.16.0 reads the area's four corners and does nothing with them.
`vdp_mouse_set_bitmap` makes the selected bitmap the cursor, with its
hot spot at (`hotx`, `hoty`).

## `vdp_mouse_sample_rate`, `vdp_mouse_resolution`, `vdp_mouse_scaling`, `vdp_mouse_acceleration`, `vdp_mouse_wheel_accel`

    void vdp_mouse_sample_rate(int sampleRate);
    void vdp_mouse_resolution(int resolution);
    void vdp_mouse_scaling(int scaling);
    void vdp_mouse_acceleration(int acceleration);
    void vdp_mouse_wheel_accel(int wheelAccel);

| Call                     | Sends                               |
|--------------------------|-------------------------------------|
| `vdp_mouse_sample_rate`  | VDU 23, 0, &89, 6, sampleRate       |
| `vdp_mouse_resolution`   | VDU 23, 0, &89, 7, resolution       |
| `vdp_mouse_scaling`      | VDU 23, 0, &89, 8, scaling          |
| `vdp_mouse_acceleration` | VDU 23, 0, &89, 9, acceleration;    |
| `vdp_mouse_wheel_accel`  | VDU 23, 0, &89, 10, wheelAccel(3)   |

## `vdp_map_char_to_bitmap`

    void vdp_map_char_to_bitmap(int char_num, int bitmap_num);

VDU 23, 0, &92, char_num, bitmap_num;. Draw the bitmap in buffer
`bitmap_num` in place of character `char_num`.

## `vdp_font_select`, `VDP_FONT_SELECT_ADJUSTBASE`

    void vdp_font_select(int buffer_id, int flags);

VDU 23, 0, &95, 0, buffer_id; flags. Use the font in buffer
`buffer_id`, the buffer that holds its glyphs; 65535 is the system
font. `VDP_FONT_SELECT_ADJUSTBASE` (1) is the one flag named.

## `vdp_font_create`

    void vdp_font_create(int buffer_id, int width, int height,
                         int ascent, int flags);

VDU 23, 0, &95, 1, buffer_id; width, height, ascent, flags. Make the
glyphs in buffer `buffer_id` a font.

## `vdp_font_adjust`, `vdp_font_set_property`, `VDP_FONT_INFO_WIDTH`

    void vdp_font_adjust(int buffer_id, int field, int value);
    void vdp_font_set_property(int buffer_id, int field, int value);

VDU 23, 0, &95, 2, buffer_id; field, value;. Set one field of a font.
`vdp_font_set_property` is libagon's other name for it, which libagon's
header leaves out.

The value is 16 bits. libagon sends only its low byte, and the VDP then
takes the next command's first byte for the high one; acc sends both.

| Name                        | `field` |
|-----------------------------|---------|
| `VDP_FONT_INFO_WIDTH`       | 0       |
| `VDP_FONT_INFO_HEIGHT`      | 1       |
| `VDP_FONT_INFO_ASCENT`      | 2       |
| `VDP_FONT_INFO_FLAGS`       | 3       |
| `VDP_FONT_INFO_CHARPTRS`    | 4       |
| `VDP_FONT_INFO_POINTSIZE`   | 5       |
| `VDP_FONT_INFO_INLEADING`   | 6       |
| `VDP_FONT_INFO_EXLEADING`   | 7       |
| `VDP_FONT_INFO_WEIGHT`      | 8       |
| `VDP_FONT_INFO_CHARSET`     | 9       |
| `VDP_FONT_INFO_CODEPAGE`    | 10      |

## `vdp_font_delete`, `vdp_font_copy`, `vdp_font_debug`

    void vdp_font_delete(int buffer_id);
    void vdp_font_copy(int buffer_id);
    void vdp_font_debug(int buffer_id);

VDU 23, 0, &95, 4, buffer_id; deletes a font; buffer 65535 clears every
font. VDU 23, 0, &95, 5, buffer_id; is the copy, and VDU 23, 0, &95,
&20, buffer_id; writes the font to the VDP's debug log.

## `vdp_adv_use_affine_matrix`

    void vdp_adv_use_affine_matrix(uint8_t flags, uint16_t affineID);

VDU 23, 0, &96, flags, affineID;. With bit 0 of `flags` set, bitmaps are
drawn through the affine matrix in buffer `affineID`. The VDP takes it
only once `VDP_VAR_AFFINE_TRANSFORM` is set. The matrices are made by
the commands in [agon/vdp/buffer.h](agon-vdp-buffer.md).

## `vdp_tilebank_init`, `vdp_tilebank_load`, `vdp_tilebank_draw`, `vdp_tilebank_free`

    void vdp_tilebank_init(int bank, int bit_depth);
    void vdp_tilebank_load(int bank, int tile_id,
                           const uint8_t *pixels);
    void vdp_tilebank_draw(int bank, int tile_id, int palette, int x,
                           int y, int x_offset, int y_offset,
                           int attribute);
    void vdp_tilebank_free(int bank);

The tile engine, VDU 23, 0, &C2, command, which the VDP takes only once
`VDP_VAR_TILE_ENGINE` is set. Tiles are 8 by 8, a byte a pixel. Every
argument is a byte; the zeros are the VDP's reserved arguments, which it
reads and ignores.

| Call                | Sends (after VDU 23, 0, &C2)                  |
|---------------------|-----------------------------------------------|
| `vdp_tilebank_init` | &00, bank, bit_depth, 0, 0                    |
| `vdp_tilebank_load` | &01, bank, tile_id, then 64 bytes of pixels   |
| `vdp_tilebank_draw` | &06, bank, tile_id, palette, x, y, x_offset,  |
|                     | y_offset, attribute                           |
| `vdp_tilebank_free` | &07, bank                                     |

Make a bank with `vdp_tilebank_init` before loading tiles into it, or
the VDP takes the pixels for commands.

## `vdp_tilemap_init`, `vdp_tilemap_set_tile`, `vdp_tilemap_free`

    void vdp_tilemap_init(int layer, int size);
    void vdp_tilemap_set_tile(int layer, int x, int y, int tile_id,
                              int attribute);
    void vdp_tilemap_free(int layer);

| Call                   | Sends (after VDU 23, 0, &C2)               |
|------------------------|--------------------------------------------|
| `vdp_tilemap_init`     | &10, layer, size, 0, 0                     |
| `vdp_tilemap_set_tile` | &11, layer, x, y, tile_id, attribute       |
| `vdp_tilemap_free`     | &17, layer                                 |

## `vdp_tilelayer_init`, `vdp_tilelayer_set_scroll`, `vdp_tilelayer_update_layerbuffer`, `vdp_tilelayer_draw_layerbuffer`, `vdp_tilelayer_draw`, `vdp_tilelayer_free`

    void vdp_tilelayer_init(int layer, int layer_size, int tile_size);
    void vdp_tilelayer_set_scroll(int layer, int x, int y, int x_offset,
                                  int y_offset);
    void vdp_tilelayer_update_layerbuffer(int layer);
    void vdp_tilelayer_draw_layerbuffer(int layer);
    void vdp_tilelayer_draw(int layer);
    void vdp_tilelayer_free(int layer);

| Call                         | Sends (after VDU 23, 0, &C2)         |
|------------------------------|--------------------------------------|
| `vdp_tilelayer_init`         | &18, layer, layer_size, tile_size, 0 |
| `vdp_tilelayer_set_scroll`   | &1A, layer, x, y, x_offset, y_offset |
| `..._update_layerbuffer`     | &1C, layer                           |
| `..._draw_layerbuffer`       | &1D, layer                           |
| `vdp_tilelayer_draw`         | &1E, layer                           |
| `vdp_tilelayer_free`         | &1F, layer                           |

## `vdp_copper_enable`, `vdp_copper_disable`

    void vdp_copper_enable(void);
    void vdp_copper_disable(void);

The copper changes palettes as the screen is drawn. `vdp_copper_enable`
sets its VDP variable, VDU 23, 0, &F8, &0310; 0;, and
`vdp_copper_disable` clears it, VDU 23, 0, &F9, &0310;. The VDP asks
only that the variable be there; libagon sets it to 0. The other copper
calls take no bytes until it is enabled.

## `vdp_copper_create_palette`, `vdp_copper_delete_palette`, `vdp_copper_set_palette_entry`, `vdp_copper_set_signal_list`, `vdp_copper_reset_signal_list`

    void vdp_copper_create_palette(uint16_t paletteID);
    void vdp_copper_delete_palette(uint16_t paletteID);
    void vdp_copper_set_palette_entry(uint16_t paletteID, uint8_t indx,
                                      uint8_t red, uint8_t green,
                                      uint8_t blue);
    void vdp_copper_set_signal_list(uint16_t bufferID);
    void vdp_copper_reset_signal_list(void);

| Call                     | Sends (after VDU 23, 0, &C4)            |
|--------------------------|-----------------------------------------|
| `..._create_palette`     | 0, paletteID;                           |
| `..._delete_palette`     | 1, paletteID;                           |
| `..._set_palette_entry`  | 2, paletteID; indx, red, green, blue    |
| `..._set_signal_list`    | 3, bufferID;                            |
| `..._reset_signal_list`  | 4                                       |

## `vdp_context_select`, `vdp_context_delete`, `vdp_context_save`, `vdp_context_restore`, `vdp_context_save_copy`, `vdp_context_restore_all`, `vdp_context_clear_stack`, `vdp_context_debug`

    void vdp_context_select(int context_id);
    void vdp_context_delete(int context_id);
    void vdp_context_save(void);
    void vdp_context_restore(void);
    void vdp_context_save_copy(int context_id);
    void vdp_context_restore_all(void);
    void vdp_context_clear_stack(void);
    void vdp_context_debug(void);

Contexts are stacks of the VDP's drawing state.

| Call                      | Sends (after VDU 23, 0, &C8)  |
|---------------------------|-------------------------------|
| `vdp_context_select`      | 0, context_id                 |
| `vdp_context_delete`      | 1, context_id                 |
| `vdp_context_reset`       | 2, flags                      |
| `vdp_context_save`        | 3                             |
| `vdp_context_restore`     | 4                             |
| `vdp_context_save_copy`   | 5, context_id                 |
| `vdp_context_restore_all` | 6                             |
| `vdp_context_clear_stack` | 7                             |
| `vdp_context_debug`       | &80, to the VDP's debug log   |

## `vdp_context_reset`, `VDP_CONTEXT_RESET_GPAINT`

    void vdp_context_reset(int flags);

VDU 23, 0, &C8, 2, flags. Reset the parts of the context that `flags`
names:

| Name                            | Bit   | Resets                   |
|---------------------------------|-------|--------------------------|
| `VDP_CONTEXT_RESET_GPAINT`      | &01   | graphics paint           |
| `VDP_CONTEXT_RESET_GPOS`        | &02   | graphics position        |
| `VDP_CONTEXT_RESET_TPAINT`      | &04   | text paint               |
| `VDP_CONTEXT_RESET_TCURSOR`     | &08   | text cursor              |
| `VDP_CONTEXT_RESET_TBEHAVIOUR`  | &10   | text behaviour           |
| `VDP_CONTEXT_RESET_FONTS`       | &20   | fonts                    |
| `VDP_CONTEXT_RESET_CHAR2BITMAP` | &40   | characters mapped to     |
|                                 |       | bitmaps                  |
