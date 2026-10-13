# `agon/vdp/buffer.h`

The buffered commands: VDU 23, 0, &A0, bufferId; command, arguments. A
buffer is a list of blocks of bytes the VDP keeps under a 16-bit ID; it
can hold VDU commands to call later, bitmap or sample data, or a
matrix. A program includes [agon/vdp.h](28-agon-vdp.md), which includes
this.

The first part is libagon's, name for name, sending libagon's bytes
except where libagon sends what VDP 2.16.0 does not take; those places
are said below. The rest is every other form VDP 2.16.0 takes. Many
commands lay out their arguments by flags in their first argument
byte: for each of those there is a function taking the flag byte and
the rest as bytes (the `_args` ones) beside typed functions for the
common forms, and `vdp_adv_command` sends any command at all. None
waits for an answer.

In the sequences, a plain argument is one byte, its low byte when the
argument is wider; `x;` is two bytes, low first; `x(3)` is three. Every
sequence starts VDU 23, 0, &A0, bufferId;, written `A0 id;` below. A
command whose length depends on its arguments goes out in more than one
piece; the VDP reads a stream and cannot tell.

An advanced offset, which the jump and call offsets always are and the
others are when their `ADVANCED_OFFSETS` flag is set, is 24 bits. With
its top bit set (`VDP_OFFSET_BLOCK`, &800000) a 16-bit block number
follows it, and the offset counts from the start of that block.
Otherwise offsets and counts are 16 bits.

A list of buffers ends with 65535.

## `VDP_BUFFERED_WRITE`, `VDP_BUFFERED_CALL`, `VDP_OFFSET_BLOCK`

The command byte, after bufferId;.

| Name                                | Value | Function                  |
|-------------------------------------|-------|---------------------------|
| `VDP_BUFFERED_WRITE`                | &00   | `vdp_adv_write_block`     |
| `VDP_BUFFERED_CALL`                 | &01   | `vdp_adv_call_buffer`     |
| `VDP_BUFFERED_CLEAR`                | &02   | `vdp_adv_clear_buffer`    |
| `VDP_BUFFERED_CREATE`               | &03   | `vdp_adv_create`          |
| `VDP_BUFFERED_SET_OUTPUT`           | &04   | `vdp_adv_stream`          |
| `VDP_BUFFERED_ADJUST`               | &05   | `vdp_adv_adjust`          |
| `VDP_BUFFERED_COND_CALL`            | &06   | `vdp_adv_call_if`         |
| `VDP_BUFFERED_JUMP`                 | &07   | `vdp_adv_jump_buffer`     |
| `VDP_BUFFERED_COND_JUMP`            | &08   | `vdp_adv_jump_if`         |
| `VDP_BUFFERED_OFFSET_JUMP`          | &09   | `vdp_adv_jump_offset`     |
| `VDP_BUFFERED_OFFSET_COND_JUMP`     | &0A   | `vdp_adv_jump_offset_if`  |
| `VDP_BUFFERED_OFFSET_CALL`          | &0B   | `vdp_adv_call_offset`     |
| `VDP_BUFFERED_OFFSET_COND_CALL`     | &0C   | `vdp_adv_call_offset_if`  |
| `VDP_BUFFERED_COPY`                 | &0D   | `vdp_adv_copy_multiple`   |
| `VDP_BUFFERED_CONSOLIDATE`          | &0E   | `vdp_adv_consolidate`     |
| `VDP_BUFFERED_SPLIT`                | &0F   | `vdp_adv_split`           |
| `VDP_BUFFERED_SPLIT_INTO`           | &10   | `vdp_adv_split_multiple`  |
| `VDP_BUFFERED_SPLIT_FROM`           | &11   | `..._split_multiple_from` |
| `VDP_BUFFERED_SPLIT_BY`             | &12   | `vdp_adv_split_by_width`  |
| `VDP_BUFFERED_SPLIT_BY_INTO`        | &13   | `..._by_width_multiple`   |
| `VDP_BUFFERED_SPLIT_BY_FROM`        | &14   | `..._multiple_from`       |
| `VDP_BUFFERED_SPREAD_INTO`          | &15   | `vdp_adv_spread_multiple` |
| `VDP_BUFFERED_SPREAD_FROM`          | &16   | `..._spread_multiple_from`|
| `VDP_BUFFERED_REVERSE_BLOCKS`       | &17   | `..._reverse_block_order` |
| `VDP_BUFFERED_REVERSE`              | &18   | `..._reverse_block_data`  |
| `VDP_BUFFERED_COPY_REF`             | &19   | `..._by_reference`        |
| `VDP_BUFFERED_COPY_AND_CONSOLIDATE` | &1A   | `..._multiple_consolidate`|
| `VDP_BUFFERED_AFFINE_TRANSFORM`     | &20   | `vdp_adv_affine_float`    |
| `VDP_BUFFERED_AFFINE_TRANSFORM_3D`  | &21   | `vdp_adv_affine_3d_float` |
| `VDP_BUFFERED_MATRIX`               | &22   | `vdp_adv_matrix_float`    |
| `VDP_BUFFERED_TRANSFORM_BITMAP`     | &28   | `..._transform_bitmap`    |
| `VDP_BUFFERED_TRANSFORM_DATA`       | &29   | `vdp_adv_transform_data`  |
| `VDP_BUFFERED_READ_VARIABLE`        | &30   | `vdp_adv_read_variable`   |
| `VDP_BUFFERED_COMPRESS`             | &40   | `..._compress_buffer`     |
| `VDP_BUFFERED_DECOMPRESS`           | &41   | `..._decompress_buffer`   |
| `VDP_BUFFERED_EXPAND_BITMAP`        | &48   | `vdp_adv_expand_bitmap`   |
| `VDP_BUFFERED_ADD_CALLBACK`         | &50   | `vdp_adv_add_callback`    |
| `VDP_BUFFERED_REMOVE_CALLBACK`      | &51   | `vdp_adv_remove_callback` |
| `VDP_BUFFERED_DEBUG_INFO`           | &80   | `vdp_adv_debug_info`      |

The VDP takes the commands from &20 to &29 only once
`VDP_VAR_AFFINE_TRANSFORM` is set (VDU 23, 0, &F8, 1; 1;, see
[agon/vdp/bitmap.h](32-agon-vdp-bitmap.md)); until then it reads the rest
of the command as VDU codes.

## `vdp_adv_command`

    void vdp_adv_command(int bufferId, int command, const void *args,
                         int length);

A0 id; command, then the `length` bytes at `args` (none when `length`
is 0 or less). Any command at all.

## `vdp_adv_write_block`, `vdp_adv_write_block_data`

    void vdp_adv_write_block(int bufferID, int length);
    void vdp_adv_write_block_data(int bufferID, int length, char *data);

A0 id; 0, length;. Add a block of `length` bytes to the buffer.
`vdp_adv_write_block_data` sends the `length` bytes at `data` after it;
`vdp_adv_write_block` announces the block and leaves its bytes to the
caller, which must send exactly that many next.

    static const char beep[] = { 7 };

    vdp_adv_clear_buffer(1);
    vdp_adv_write_block_data(1, sizeof beep, (char *) beep);
    vdp_adv_call_buffer(1);

## `vdp_adv_call_buffer`, `vdp_adv_jump_buffer`

    void vdp_adv_call_buffer(int bufferID);
    void vdp_adv_jump_buffer(int bufferID);

A0 id; 1 and A0 id; 7. Run the VDU commands in the buffer. A jump from
the top level, where a program's commands run, is a call; a jump to
65535 goes to the end of the buffer running.

## `vdp_adv_clear_buffer`

    void vdp_adv_clear_buffer(int bufferID);

A0 id; 2. Clear the buffer. 65535 clears every buffer, and every
bitmap, font and sample made from one.

## `vdp_adv_create`, `vdp_adv_stream`

    void vdp_adv_create(int bufferID, int length);
    void vdp_adv_stream(int bufferID);

`vdp_adv_create` sends A0 id; 3, length;: make a buffer of `length`
bytes. `vdp_adv_stream` sends A0 id; 4: where the VDP's output
goes, a buffer `vdp_adv_create` made, 0 for back to MOS, or 65535 for
nowhere.

## `vdp_adv_adjust`, `vdp_adv_adjust_args`, `VDP_ADJUST_SET`

    void vdp_adv_adjust(int bufferID, int operation, int offset);
    void vdp_adv_adjust_args(int bufferId, int operation,
                             const void *args, int length);

Command 5 changes bytes in the buffer. The operation is in the low four
bits, flags above:

| Name                          | Value | Meaning                       |
|-------------------------------|-------|-------------------------------|
| `VDP_ADJUST_NOT`              | &00   | not; no operand               |
| `VDP_ADJUST_NEG`              | &01   | negate; no operand            |
| `VDP_ADJUST_SET`              | &02   | set                           |
| `VDP_ADJUST_ADD`              | &03   | add                           |
| `VDP_ADJUST_ADD_CARRY`        | &04   | add with carry                |
| `VDP_ADJUST_AND`              | &05   | and                           |
| `VDP_ADJUST_OR`               | &06   | or                            |
| `VDP_ADJUST_XOR`              | &07   | exclusive or                  |
| `VDP_ADJUST_OP_MASK`          | &0F   | the operation's bits          |
| `VDP_ADJUST_ADVANCED_OFFSETS` | &10   | 24-bit offset and count       |
| `VDP_ADJUST_BUFFER_VALUE`     | &20   | the operand from a buffer     |
| `VDP_ADJUST_MULTI_TARGET`     | &40   | a count of target bytes       |
| `VDP_ADJUST_MULTI_OPERAND`    | &80   | a count of operand bytes      |

The VDP reads, in order: the offset; a count when either MULTI flag is
set; an operand buffer and an offset into it with BUFFER_VALUE; then,
when the operands are not from a buffer, the operand bytes, one or
(with MULTI_OPERAND) count of them.

`vdp_adv_adjust` is libagon's: A0 id; 5, operation, offset;, and no
more. An operation that takes an operand (SET and above) leaves it to
the caller. `vdp_adv_adjust_args` sends A0 id; 5, operation and the
`length` bytes at `args`.

## `vdp_adv_adjust_value`, `vdp_adv_adjust_multi`, `vdp_adv_adjust_multi_data`, `vdp_adv_adjust_buffer`

    void vdp_adv_adjust_value(int bufferId, int operation, int offset,
                              int value);
    void vdp_adv_adjust_multi(int bufferId, int operation, int offset,
                              int count, int value);
    void vdp_adv_adjust_multi_data(int bufferId, int operation,
                                   int offset, int count,
                                   const void *operands);
    void vdp_adv_adjust_buffer(int bufferId, int operation, int offset,
                               int operandBufferId, int operandOffset);

Whole adjust commands. Each sends A0 id; 5, operation, then the offset,
and the count where there is one, as 16 bits, or 24 with
`VDP_ADJUST_ADVANCED_OFFSETS` in `operation`. An operand goes only with
the operations that take one (SET and above).

| Call                   | Flag added   | Then                         |
|------------------------|--------------|------------------------------|
| `..._adjust_value`     | none         | value                        |
| `..._adjust_multi`     | MULTI_TARGET | count, value                 |
| `..._adjust_multi_data`| MULTI_OPERAND| count, `count` operand bytes |
| `..._adjust_buffer`    | BUFFER_VALUE | operandBufferId;,            |
|                        |              | operandOffset                |

    /* add 1 to byte 4 of buffer 10 */
    vdp_adv_adjust_value(10, VDP_ADJUST_ADD, 4, 1);

## `vdp_adv_call_conditional`, `vdp_adv_jump_conditional`, `VDP_COND_EQUAL`

    void vdp_adv_call_conditional(int bufferId, int operation,
                                  int checkBufferId, int checkOffset);
    void vdp_adv_jump_conditional(int bufferId, int operation,
                                  int checkBufferId, int checkOffset);

libagon's: A0 id; 6 (call) or 8 (jump), operation, checkBufferId;,
checkOffset;. That is the whole command for EXISTS and NOT_EXISTS; the
other conditions take an operand the caller sends after. (libagon's
header declares `vdp_adv_call_conditional` twice and
`vdp_adv_jump_conditional` not at all; its library has both.)

The condition byte of commands 6, 8, &0A and &0C:

| Name                        | Value | Meaning                   |
|-----------------------------|-------|---------------------------|
| `VDP_COND_EXISTS`           | &00   | exists; no operand        |
| `VDP_COND_NOT_EXISTS`       | &01   | does not; no operand      |
| `VDP_COND_EQUAL`            | &02   | =                         |
| `VDP_COND_NOT_EQUAL`        | &03   | !=                        |
| `VDP_COND_LESS`             | &04   | <                         |
| `VDP_COND_GREATER`          | &05   | >                         |
| `VDP_COND_LESS_EQUAL`       | &06   | <=                        |
| `VDP_COND_GREATER_EQUAL`    | &07   | >=                        |
| `VDP_COND_AND`              | &08   | and                       |
| `VDP_COND_OR`               | &09   | or                        |
| `VDP_COND_OP_MASK`          | &0F   | the condition's bits      |
| `VDP_COND_ADVANCED_OFFSETS` | &10   | 24-bit check offset       |
| `VDP_COND_BUFFER_VALUE`     | &20   | the operand from a buffer |
| `VDP_COND_VAR_VALUE`        | &40   | check a VDP variable      |
| `VDP_COND_16BIT`            | &80   | a 16-bit operand          |

## `vdp_adv_call_if`, `vdp_adv_jump_if`, `vdp_adv_call_if_args`, `vdp_adv_jump_if_args`

    void vdp_adv_call_if(int bufferId, int operation, int checkBufferId,
                         int checkOffset, int operand);
    void vdp_adv_jump_if(int bufferId, int operation, int checkBufferId,
                         int checkOffset, int operand);
    void vdp_adv_call_if_args(int bufferId, int operation,
                              const void *args, int length);
    void vdp_adv_jump_if_args(int bufferId, int operation,
                              const void *args, int length);

A whole conditional call (command 6) or jump (8). `vdp_adv_call_if` and
`vdp_adv_jump_if` send A0 id; 6 or 8, operation, checkBufferId; (a
variable's ID with `VDP_COND_VAR_VALUE`), then the check offset, 16 or
24 bits by `VDP_COND_ADVANCED_OFFSETS` and none for a variable, then
the operand for the conditions that take one (EQUAL and above): a byte,
or `operand;` with `VDP_COND_16BIT`.

An operand from a buffer, or an offset with a block number, goes
through the `_args` forms, which send A0 id; 6 or 8, operation, and the
`length` bytes at `args`.

    /* call buffer 2 when byte 0 of buffer 3 is 5 */
    vdp_adv_call_if(2, VDP_COND_EQUAL, 3, 0, 5);

## `vdp_adv_jump_offset`, `vdp_adv_call_offset`, `vdp_adv_jump_offset_block`, `vdp_adv_call_offset_block`

    void vdp_adv_jump_offset(int bufferId, int offset);
    void vdp_adv_call_offset(int bufferId, int offset);
    void vdp_adv_jump_offset_block(int bufferId, int offset, int block);
    void vdp_adv_call_offset_block(int bufferId, int offset, int block);

Jump (command 9) or call (&0B) into a buffer at an offset, which is
always an advanced one.

`vdp_adv_jump_offset` and `vdp_adv_call_offset` send A0 id; 9 or &0B,
offset(3). With `VDP_OFFSET_BLOCK` set in `offset` they send nothing at
all, since the VDP would wait for a block number.

The `_block` forms send A0 id; 9 or &0B, (offset | `VDP_OFFSET_BLOCK`)(3),
block;. acc sets the bit for the caller; libagon's send nothing unless
it is set, so block 2 at offset 0 does nothing there. libagon's block
calls also all send command 9, whatever they are; here each sends its
own.

## `vdp_adv_jump_offset_conditional`, `vdp_adv_call_offset_conditional`, `vdp_adv_jump_offset_block_conditional`, `vdp_adv_call_offset_block_conditional`

    void vdp_adv_jump_offset_conditional(int bufferId, int offset);
    void vdp_adv_call_offset_conditional(int bufferId, int offset);
    void vdp_adv_jump_offset_block_conditional(int bufferId,
                                               int offset, int block);
    void vdp_adv_call_offset_block_conditional(int bufferId,
                                               int offset, int block);

As the four above with commands &0A (jump) and &0C (call): they send the
offset, and the block, and leave the condition to the caller, as
libagon does. `vdp_adv_jump_offset_if` and `vdp_adv_call_offset_if`
send it too.

## `vdp_adv_jump_offset_if`, `vdp_adv_call_offset_if`

    void vdp_adv_jump_offset_if(int bufferId, int offset,
                                int operation, int checkBufferId,
                                int checkOffset, int operand);
    void vdp_adv_call_offset_if(int bufferId, int offset,
                                int operation, int checkBufferId,
                                int checkOffset, int operand);

A0 id; &0A (jump) or &0C (call), offset(3), then the condition as
`vdp_adv_call_if` sends it. No block number is sent, so `offset` must
not have `VDP_OFFSET_BLOCK` set; for that, use `vdp_adv_command`.

## `vdp_adv_copy_multiple`, `vdp_adv_copy_multiple_by_reference`, `vdp_adv_copy_multiple_consolidate`

    void vdp_adv_copy_multiple(int bufferId, int num_buffers, ...);
    void vdp_adv_copy_multiple_by_reference(int bufferId,
                                            int num_buffers, ...);
    void vdp_adv_copy_multiple_consolidate(int bufferId,
                                           int num_buffers, ...);

A0 id; &0D, &19 or &1A, then the `num_buffers` IDs that follow, each
`int` sent as 16 bits, and 65535;. Copy the blocks of those buffers
into `bufferId`; &19 copies by reference, &1A consolidates the copy.

    vdp_adv_copy_multiple(20, 3, 1, 2, 3);

## `vdp_adv_consolidate`, `vdp_adv_split`, `vdp_adv_split_multiple`, `vdp_adv_split_multiple_from`

    void vdp_adv_consolidate(int bufferID);
    void vdp_adv_split(int bufferID, int blockSize);
    void vdp_adv_split_multiple(int bufferId, int blockSize,
                                int num_buffers, ...);
    void vdp_adv_split_multiple_from(int bufferID, int blockSize,
                                     int targetBufferID);

| Call                     | Sends (after A0 id;)                     |
|--------------------------|------------------------------------------|
| `vdp_adv_consolidate`    | &0E                                      |
| `vdp_adv_split`          | &0F, blockSize;                          |
| `..._split_multiple`     | &10, blockSize;, the IDs;, 65535;        |
| `..._split_multiple_from`| &11, blockSize;, targetBufferID;         |

Split the buffer into blocks of `blockSize`: in place, into the
buffers listed, or from `targetBufferID`.

## `vdp_adv_split_by_width`, `vdp_adv_split_by_width_multiple`, `vdp_adv_split_by_width_multiple_from`

    void vdp_adv_split_by_width(int bufferID, int width,
                                int blockCount);
    void vdp_adv_split_by_width_multiple(int bufferId, int width,
                                         int num_buffers, ...);
    void vdp_adv_split_by_width_multiple_from(int bufferID, int width,
                                              int blockCount,
                                              int targetBufferID);

| Call                     | Sends (after A0 id;)                     |
|--------------------------|------------------------------------------|
| `..._split_by_width`     | &12, width;, blockCount;                 |
| `..._by_width_multiple`  | &13, width;, the IDs;, 65535;            |
| `..._multiple_from`      | &14, width;, blockCount;, targetBufferID;|

## `vdp_adv_spread_multiple`, `vdp_adv_spread_multiple_from`

    void vdp_adv_spread_multiple(int bufferId, int num_buffers, ...);
    void vdp_adv_spread_multiple_from(int bufferID, int targetBufferID);

A0 id; &15, the IDs;, 65535; and A0 id; &16, targetBufferID;. Spread the
buffer's blocks into the buffers listed, or from `targetBufferID`.

## `vdp_adv_reverse_block_order`, `vdp_adv_reverse_block_data`, `VDP_REVERSE_16BIT`

    void vdp_adv_reverse_block_order(int bufferID);
    void vdp_adv_reverse_block_data(int bufferID, int options,
                                    int valueSize, int chunkSize);

`vdp_adv_reverse_block_order` sends A0 id; &17: the blocks in reverse
order. `vdp_adv_reverse_block_data` sends A0 id; &18, options, then
valueSize; only when both size bits are set, and chunkSize; only with
`VDP_REVERSE_CHUNKED`.

| Name                      | Value | Option                          |
|---------------------------|-------|---------------------------------|
| `VDP_REVERSE_16BIT`       | &01   | 16-bit values                   |
| `VDP_REVERSE_32BIT`       | &02   | 32-bit values                   |
| `VDP_REVERSE_SIZE`        | &03   | both: a value size follows      |
| `VDP_REVERSE_CHUNKED`     | &04   | a chunk size follows            |
| `VDP_REVERSE_BLOCK`       | &08   | block                           |
| `VDP_REVERSE_UNUSED_BITS` | &F0   | unused                          |

libagon decides otherwise: it sends a value size for options 1 and 2 as
well, both sizes for options 0, and neither for any options above 4,
chunked or not, each of which puts the VDP out of step.

## `vdp_adv_compress_buffer`, `vdp_adv_decompress_buffer`

    void vdp_adv_compress_buffer(int targetBufferID,
                                 int sourceBufferId);
    void vdp_adv_decompress_buffer(int targetBufferID,
                                   int sourceBufferId);

A0 targetBufferID; &40 or &41, sourceBufferId;.

## `vdp_adv_create_rotated_buffer`, `vdp_adv_create_scaled_buffer`, `vdp_adv_create_translated_buffer`, `vdp_adv_create_skewed_buffer`

    void vdp_adv_create_rotated_buffer(uint16_t transformID,
                                       uint16_t angle);
    void vdp_adv_create_scaled_buffer(uint16_t transformID,
                                      float scaleX, float scaleY);
    void vdp_adv_create_translated_buffer(uint16_t transformID,
                                          int16_t x, int16_t y);
    void vdp_adv_create_skewed_buffer(uint16_t transformID,
                                      float scaleX, float scaleY);

libagon's: each makes a 2d affine transform in buffer `transformID`
with one operation, its arguments 16-bit fixed point. Each sends A0
transformID; &20, then:

| Call            | Then                                         |
|-----------------|----------------------------------------------|
| `..._rotated`   | 2 (ROTATE), &C0, angle; (degrees)            |
| `..._scaled`    | 5 (SCALE), &C8, scaleX;, scaleY;             |
| `..._translated`| 6 (TRANSLATE), &C0, x;, y;                   |
| `..._skewed`    | 8 (SHEAR), &C8, scaleX;, scaleY;             |

Format &C0 is whole numbers, &C8 is 8 bits of fraction: a scale or a
shear is rounded to the nearest 1/256 and cut to 16 bits. libagon sends
x's high byte where y's should be in the translation; acc sends y.
"Skewed" is a shear, as libagon has it, not the VDP's SKEW, which takes
angles. The matrix is used to draw by `vdp_adv_use_affine_matrix` in
[agon/vdp/bitmap.h](32-agon-vdp-bitmap.md).

## `vdp_adv_apply_transformation_with_stride`

    void vdp_adv_apply_transformation_with_stride(uint16_t sourceID,
                                                  uint16_t destID,
                                                  uint16_t transformID,
                                                  uint16_t offset,
                                                  uint16_t stride);

A0 destID; &29, 6 (HAS_OFFSET and HAS_STRIDE), &C0, transformID;,
sourceID;, offset;, stride;. Transform data, on whole 16-bit numbers.

## `vdp_adv_affine_identity`, `vdp_adv_affine_invert`, `vdp_adv_affine_float`, `vdp_adv_affine_args`, `VDP_AFFINE_ROTATE`

    void vdp_adv_affine_identity(int bufferId);
    void vdp_adv_affine_invert(int bufferId);
    void vdp_adv_affine_float(int bufferId, int operation, int count,
                              const float *values);
    void vdp_adv_affine_args(int bufferId, int operation,
                             const void *args, int length);

2d affine transforms, command &20: A0 id; &20, operation, then the
arguments. IDENTITY and INVERT take none, so no format byte either.
`vdp_adv_affine_float` sends a format byte 0 (a 32-bit float) and then
the `count` floats of `values`, 4 bytes each; with
`VDP_AFFINE_OP_MULTI_FORMAT` every value has a format byte of its own.
`vdp_adv_affine_args` sends the `length` bytes at `args`, formats and
values as they are to go.

| Name                             | Value | Operation               |
|----------------------------------|-------|-------------------------|
| `VDP_AFFINE_IDENTITY`            | 0     | identity                |
| `VDP_AFFINE_INVERT`              | 1     | invert                  |
| `VDP_AFFINE_ROTATE`              | 2     | rotate, degrees         |
| `VDP_AFFINE_ROTATE_RAD`          | 3     | rotate, radians         |
| `VDP_AFFINE_MULTIPLY`            | 4     | multiply                |
| `VDP_AFFINE_SCALE`               | 5     | scale                   |
| `VDP_AFFINE_TRANSLATE`           | 6     | translate               |
| `VDP_AFFINE_TRANSLATE_OS_COORDS` | 7     | translate, OS coords    |
| `VDP_AFFINE_SHEAR`               | 8     | shear                   |
| `VDP_AFFINE_SKEW`                | 9     | skew, degrees           |
| `VDP_AFFINE_SKEW_RAD`            | 10    | skew, radians           |
| `VDP_AFFINE_TRANSFORM`           | 11    | transform               |
| `VDP_AFFINE_TRANSLATE_BITMAP`    | 12    | translate bitmap        |
| `VDP_AFFINE_OP_MASK`             | &0F   | the operation's bits    |
| `VDP_AFFINE_OP_ADVANCED_OFFSETS` | &10   | advanced offsets        |
| `VDP_AFFINE_OP_BUFFER_VALUE`     | &20   | values from a buffer    |
| `VDP_AFFINE_OP_MULTI_FORMAT`     | &40   | a format for each value |

    float scale[2] = { 2.0f, 2.0f };

    vdp_adv_affine_float(5, VDP_AFFINE_SCALE, 2, scale);

## `vdp_adv_affine_3d_identity`, `vdp_adv_affine_3d_invert`, `vdp_adv_affine_3d_float`, `vdp_adv_affine_3d_args`

    void vdp_adv_affine_3d_identity(int bufferId);
    void vdp_adv_affine_3d_invert(int bufferId);
    void vdp_adv_affine_3d_float(int bufferId, int operation, int count,
                                 const float *values);
    void vdp_adv_affine_3d_args(int bufferId, int operation,
                                const void *args, int length);

The same for 3d transforms, command &21.

## `VDP_FLOAT_FORMAT_FIXED`, `VDP_FLOAT_FORMAT_16BIT`

The format byte before a run of number arguments. Zero is a 32-bit
float; otherwise 16-bit and/or fixed point, the low five bits a signed
shift of the binary point.

| Name                            | Value | Meaning                   |
|---------------------------------|-------|---------------------------|
| `VDP_FLOAT_FORMAT_SHIFT_MASK`   | &1F   | the shift                 |
| `VDP_FLOAT_FORMAT_SHIFT_TOPBIT` | &10   | the shift's sign          |
| `VDP_FLOAT_FORMAT_FLAGS`        | &E0   | the flags                 |
| `VDP_FLOAT_FORMAT_FIXED`        | &40   | fixed point               |
| `VDP_FLOAT_FORMAT_16BIT`        | &80   | 16 bits                   |

## `vdp_adv_matrix_float`, `vdp_adv_matrix_combine`, `vdp_adv_matrix_args`, `VDP_MATRIX_SET`

    void vdp_adv_matrix_float(int bufferId, int operation, int rows,
                              int columns, int count,
                              const float *values);
    void vdp_adv_matrix_combine(int bufferId, int operation, int rows,
                                int columns, int sourceId1,
                                int sourceId2);
    void vdp_adv_matrix_args(int bufferId, int operation, int rows,
                             int columns, const void *args, int length);

Matrices of any size, command &22: A0 id; &22, operation, rows,
columns, then the arguments. `vdp_adv_matrix_float` sends one format
byte 0 and the `count` floats, all sharing it.
`vdp_adv_matrix_combine` sends sourceId1;, sourceId2;, for ADD,
SUBTRACT and MULTIPLY. `vdp_adv_matrix_args` sends the `length` bytes
at `args`.

| Name                             | Value | Operation               |
|----------------------------------|-------|-------------------------|
| `VDP_MATRIX_SET`                 | 0     | set                     |
| `VDP_MATRIX_SET_VALUE`           | 1     | set a value             |
| `VDP_MATRIX_FILL`                | 2     | fill                    |
| `VDP_MATRIX_DIAGONAL`            | 3     | diagonal                |
| `VDP_MATRIX_ADD`                 | 4     | add two                 |
| `VDP_MATRIX_SUBTRACT`            | 5     | subtract two            |
| `VDP_MATRIX_MULTIPLY`            | 6     | multiply two            |
| `VDP_MATRIX_SCALAR_MULTIPLY`     | 7     | multiply by a number    |
| `VDP_MATRIX_SUBMATRIX`           | 8     | submatrix               |
| `VDP_MATRIX_INSERT_ROW`          | 9     | insert a row            |
| `VDP_MATRIX_INSERT_COLUMN`       | 10    | insert a column         |
| `VDP_MATRIX_DELETE_ROW`          | 11    | delete a row            |
| `VDP_MATRIX_DELETE_COLUMN`       | 12    | delete a column         |
| `VDP_MATRIX_OP_MASK`             | &0F   | the operation's bits    |
| `VDP_MATRIX_OP_ADVANCED_OFFSETS` | &10   | advanced offsets        |
| `VDP_MATRIX_OP_BUFFER_VALUE`     | &20   | values from a buffer    |

## `vdp_adv_transform_bitmap`, `VDP_TRANSFORM_BITMAP_RESIZE`

    void vdp_adv_transform_bitmap(int bufferId, int options,
                                  int transformBufferId, int bitmapId,
                                  int width, int height);

A0 id; &28, options, transformBufferId;, bitmapId;, and width;
height; only with `VDP_TRANSFORM_BITMAP_EXPLICIT_SIZE`.

| Name                                 | Value |
|--------------------------------------|-------|
| `VDP_TRANSFORM_BITMAP_RESIZE`        | &01   |
| `VDP_TRANSFORM_BITMAP_EXPLICIT_SIZE` | &02   |
| `VDP_TRANSFORM_BITMAP_TRANSLATE`     | &04   |

## `vdp_adv_transform_data`, `vdp_adv_transform_data_args`, `VDP_TRANSFORM_DATA_HAS_SIZE`

    void vdp_adv_transform_data(int bufferId, int options, int format,
                                int transformBufferId,
                                int sourceBufferId, int size,
                                int offset, int stride, int limit);
    void vdp_adv_transform_data_args(int bufferId, int options,
                                     int format, int transformBufferId,
                                     int sourceBufferId,
                                     const void *args, int length);

Both send A0 id; &29, options, format, transformBufferId;,
sourceBufferId;. Then `vdp_adv_transform_data` sends each of these
only when its option is set: size (a byte), offset (16 bits, or 24
with `VDP_TRANSFORM_DATA_ADVANCED`), stride; and limit; (16 bits
either way). With `VDP_TRANSFORM_DATA_BUFFER_ARGS` each is a buffer
and offset instead, which is what the `_args` form, sending the
`length` bytes at `args`, is for.

| Name                             | Value | Option                     |
|----------------------------------|-------|----------------------------|
| `VDP_TRANSFORM_DATA_HAS_SIZE`    | &01   | a size                     |
| `VDP_TRANSFORM_DATA_HAS_OFFSET`  | &02   | an offset                  |
| `VDP_TRANSFORM_DATA_HAS_STRIDE`  | &04   | a stride                   |
| `VDP_TRANSFORM_DATA_HAS_LIMIT`   | &08   | a limit                    |
| `VDP_TRANSFORM_DATA_ADVANCED`    | &10   | a 24-bit offset            |
| `VDP_TRANSFORM_DATA_BUFFER_ARGS` | &20   | arguments from buffers     |
| `VDP_TRANSFORM_DATA_PER_BLOCK`   | &40   | per block                  |

## `vdp_adv_read_variable`, `VDP_READ_VAR_16BIT`

    void vdp_adv_read_variable(int bufferId, int options, int offset,
                               int variableId, int defaultValue);

A0 id; &30, options, offset (16 bits, or 24 with
`VDP_READ_VAR_ADVANCED_OFFSETS`), variableId;, and with
`VDP_READ_VAR_USE_DEFAULT` the default, as wide as the value: a byte,
or defaultValue; with `VDP_READ_VAR_16BIT`. Read a VDP variable into
the buffer at `offset`.

| Name                            | Value | Option                    |
|---------------------------------|-------|---------------------------|
| `VDP_READ_VAR_BIG_ENDIAN`       | &01   | high byte first           |
| `VDP_READ_VAR_ADVANCED_OFFSETS` | &10   | a 24-bit offset           |
| `VDP_READ_VAR_USE_DEFAULT`      | &40   | a default follows         |
| `VDP_READ_VAR_16BIT`            | &80   | a 16-bit value            |

## `vdp_adv_expand_bitmap`, `vdp_adv_expand_bitmap_mapped`, `VDP_EXPAND_BITMAP_SIZE`

    void vdp_adv_expand_bitmap(int bufferId, int options,
                               int sourceBufferId, int width,
                               const void *map);
    void vdp_adv_expand_bitmap_mapped(int bufferId, int options,
                                      int sourceBufferId, int width,
                                      int mapBufferId);

A0 id; &48, options, sourceBufferId;, and width; only with
`VDP_EXPAND_BITMAP_ALIGNED`, then the map. `vdp_adv_expand_bitmap`
takes `VDP_EXPAND_BITMAP_USEBUFFER` out of the options and sends the
map, a byte for each value a pixel can have: 2 to the bits a pixel,
which are 8 (256 bytes) when the options say 0.
`vdp_adv_expand_bitmap_mapped` puts USEBUFFER in and sends
mapBufferId;.

| Name                          | Value | Option                      |
|-------------------------------|-------|-----------------------------|
| `VDP_EXPAND_BITMAP_SIZE`      | &07   | bits a pixel, 0 for 8       |
| `VDP_EXPAND_BITMAP_ALIGNED`   | &08   | a width follows             |
| `VDP_EXPAND_BITMAP_USEBUFFER` | &10   | the map is in a buffer      |

## `vdp_adv_add_callback`, `vdp_adv_remove_callback`, `VDP_CALLBACK_VSYNC`

    void vdp_adv_add_callback(int bufferId, int type);
    void vdp_adv_remove_callback(int bufferId, int type);

A0 id; &50, type; and A0 id; &51, type;. Have the VDP call the buffer
on an event, or stop. In a remove, 65535 as the buffer removes every
buffer from that type; as the type, the buffer from every type.

| Name                       | Value  | Called on                   |
|----------------------------|--------|-----------------------------|
| `VDP_CALLBACK_VSYNC`       | 0      | vsync                       |
| `VDP_CALLBACK_MODE_CHANGE` | 1      | a mode change               |
| `VDP_CALLBACK_KEYBOARD`    | 2      | the keyboard                |
| `VDP_CALLBACK_MOUSE`       | 3      | the mouse                   |
| `VDP_CALLBACK_PALETTE`     | 4      | the palette                 |
| `VDP_CALLBACK_READPIXEL`   | 5      | a pixel read                |
| `VDP_CALLBACK_SENDING_VDPP`| &0100  | sending VDPP                |
| `VDP_CALLBACK_SENT_VDPP`   | &0180  | VDPP sent                   |

## `vdp_adv_debug_info`

    void vdp_adv_debug_info(int bufferId);

A0 id; &80. The VDP writes what it knows of the buffer to its own
debug output only.
