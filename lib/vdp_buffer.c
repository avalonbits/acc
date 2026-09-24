/*
 * <agon/vdp/buffer.h>'s calls: VDU 23, 0, &A0, bufferId; command, ...
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * libagon's calls send what libagon sends, byte for byte, save where
 * libagon sends what the VDP does not take; each of those says so. The
 * rest follow video/vdu_buffered.h in VDP 2.16.0, which is where every
 * argument's order and width below was read off.
 *
 * A command whose length depends on its arguments goes out in more than
 * one piece. The VDP reads a stream, so it cannot tell, and libagon does
 * the same for its lists of buffers.
 */
#include <math.h>
#include <stdarg.h>

#include <agon/vdp.h>

#include "vdp_emit.h"

/* The first bytes of every command here but one. */
#define ADV(id, cmd) 23, 0, 0xa0, W(id), (cmd)

/* An offset or count in 24 bits when the command's flags say so, else in
 * 16. */
static void send_offset(int advanced, int value)
{
    if (advanced)
        SEND(U24(value));
    else
        SEND(W(value));
}

/* A run of argument bytes, which may be none. */
static void send_args(const void *args, int length)
{
    if (length > 0)
        SEND_BYTES(args, length);
}

/*
 * Writing, calling, clearing, creating
 *
 * vdp_adv_write_block announces a block and leaves its bytes to the
 * caller, which must send exactly that many next.
 */

void vdp_adv_write_block(int bufferID, int length)
{
    SEND(ADV(bufferID, VDP_BUFFERED_WRITE), W(length));
}

void vdp_adv_write_block_data(int bufferID, int length, char *data)
{
    SEND(ADV(bufferID, VDP_BUFFERED_WRITE), W(length));
    SEND_BYTES(data, length);
}

void vdp_adv_call_buffer(int bufferID)
{
    SEND(ADV(bufferID, VDP_BUFFERED_CALL));
}

/* 65535 clears every buffer, and every bitmap, font and sample made from
 * one. */
void vdp_adv_clear_buffer(int bufferID)
{
    SEND(ADV(bufferID, VDP_BUFFERED_CLEAR));
}

void vdp_adv_create(int bufferID, int length)
{
    SEND(ADV(bufferID, VDP_BUFFERED_CREATE), W(length));
}

/* Where the VDP's output goes: a buffer vdp_adv_create made, or 0 for
 * back to MOS, or 65535 for nowhere. */
void vdp_adv_stream(int bufferID)
{
    SEND(ADV(bufferID, VDP_BUFFERED_SET_OUTPUT));
}

/* A jump from the top level, which is where a program's commands run, is a
 * call; 65535 jumps to the end of the buffer running. */
void vdp_adv_jump_buffer(int bufferID)
{
    SEND(ADV(bufferID, VDP_BUFFERED_JUMP));
}

void vdp_adv_command(int bufferId, int command, const void *args, int length)
{
    SEND(ADV(bufferId, command));
    send_args(args, length);
}

/*
 * Adjust
 *
 * libagon's vdp_adv_adjust sends the operation and a 16-bit offset and no
 * more: an operation that takes an operand (SET and above) leaves it to
 * the caller. The VDP reads, in order, the offset; a count when either
 * MULTI flag is set, 24 bits with advanced offsets; an operand buffer and
 * offset into it with BUFFER_VALUE; then, when the operands are not
 * fetched from a buffer, the operand bytes themselves -- one, or count of
 * them with MULTI_OPERAND.
 */

void vdp_adv_adjust(int bufferID, int operation, int offset)
{
    SEND(ADV(bufferID, VDP_BUFFERED_ADJUST), operation, W(offset));
}

void vdp_adv_adjust_args(int bufferId, int operation, const void *args, int length)
{
    SEND(ADV(bufferId, VDP_BUFFERED_ADJUST), operation);
    send_args(args, length);
}

/* NOT and NEG take no operand. */
static int adjust_operand(int operation)
{
    return (operation & VDP_ADJUST_OP_MASK) > VDP_ADJUST_NEG;
}

static void adjust_head(int bufferId, int operation, int offset)
{
    SEND(ADV(bufferId, VDP_BUFFERED_ADJUST), operation);
    send_offset(operation & VDP_ADJUST_ADVANCED_OFFSETS, offset);
}

void vdp_adv_adjust_value(int bufferId, int operation, int offset, int value)
{
    adjust_head(bufferId, operation, offset);
    if (adjust_operand(operation))
        SEND(value);
}

void vdp_adv_adjust_multi(int bufferId, int operation, int offset, int count, int value)
{
    operation |= VDP_ADJUST_MULTI_TARGET;
    adjust_head(bufferId, operation, offset);
    send_offset(operation & VDP_ADJUST_ADVANCED_OFFSETS, count);
    if (adjust_operand(operation))
        SEND(value);
}

void vdp_adv_adjust_multi_data(int bufferId, int operation, int offset, int count, const void *operands)
{
    operation |= VDP_ADJUST_MULTI_OPERAND;
    adjust_head(bufferId, operation, offset);
    send_offset(operation & VDP_ADJUST_ADVANCED_OFFSETS, count);
    if (adjust_operand(operation))
        send_args(operands, count);
}

void vdp_adv_adjust_buffer(int bufferId, int operation, int offset, int operandBufferId, int operandOffset)
{
    operation |= VDP_ADJUST_BUFFER_VALUE;
    adjust_head(bufferId, operation, offset);
    if (!adjust_operand(operation))
        return;

    SEND(W(operandBufferId));
    send_offset(operation & VDP_ADJUST_ADVANCED_OFFSETS, operandOffset);
}

/*
 * Conditional call and jump
 *
 * libagon's vdp_adv_call_conditional and vdp_adv_jump_conditional send the
 * condition, the buffer to check and a 16-bit offset into it, which is the
 * whole command for EXISTS and NOT_EXISTS; the other conditions take an
 * operand the caller sends after. (libagon's header declares
 * vdp_adv_call_conditional twice and vdp_adv_jump_conditional not at all,
 * but its library has both.)
 */

static void conditional(int bufferId, int command, int operation, int checkBufferId, int checkOffset)
{
    SEND(ADV(bufferId, command), operation, W(checkBufferId), W(checkOffset));
}

void vdp_adv_call_conditional(int bufferId, int operation, int checkBufferId, int checkOffset)
{
    conditional(bufferId, VDP_BUFFERED_COND_CALL, operation, checkBufferId, checkOffset);
}

void vdp_adv_jump_conditional(int bufferId, int operation, int checkBufferId, int checkOffset)
{
    conditional(bufferId, VDP_BUFFERED_COND_JUMP, operation, checkBufferId, checkOffset);
}

/* A whole condition, in the order bufferConditional reads it: the buffer
 * or VDP variable to check, the offset into a buffer (a variable has
 * none), and the operand when the condition takes one. */
static void condition(int operation, int checkBufferId, int checkOffset, int operand)
{
    SEND(operation, W(checkBufferId));
    if (!(operation & VDP_COND_VAR_VALUE))
        send_offset(operation & VDP_COND_ADVANCED_OFFSETS, checkOffset);
    if ((operation & VDP_COND_OP_MASK) <= VDP_COND_NOT_EXISTS)
        return;

    if (operation & VDP_COND_16BIT)
        SEND(W(operand));
    else
        SEND(operand);
}

void vdp_adv_call_if(int bufferId, int operation, int checkBufferId, int checkOffset, int operand)
{
    SEND(ADV(bufferId, VDP_BUFFERED_COND_CALL));
    condition(operation, checkBufferId, checkOffset, operand);
}

void vdp_adv_jump_if(int bufferId, int operation, int checkBufferId, int checkOffset, int operand)
{
    SEND(ADV(bufferId, VDP_BUFFERED_COND_JUMP));
    condition(operation, checkBufferId, checkOffset, operand);
}

void vdp_adv_call_if_args(int bufferId, int operation, const void *args, int length)
{
    SEND(ADV(bufferId, VDP_BUFFERED_COND_CALL), operation);
    send_args(args, length);
}

void vdp_adv_jump_if_args(int bufferId, int operation, const void *args, int length)
{
    SEND(ADV(bufferId, VDP_BUFFERED_COND_JUMP), operation);
    send_args(args, length);
}

/*
 * Jump and call at an offset
 *
 * The offset of commands 9 to 12 is always an advanced one: 24 bits, and a
 * block number after them when the top bit is set. libagon's calls without
 * a block send nothing at all for an offset with that bit set, since the
 * VDP would wait for a block number; its calls with a block send nothing
 * unless it is set. That leaves a caller to set it, which is how libagon
 * is used -- but the natural call, block 2 at offset 0, then does nothing,
 * so here the bit is set for the caller; with it already set, the bytes
 * are libagon's. libagon's block calls also all send command 9, a plain
 * jump, whatever they are: they store their command into the template of
 * the calls without a block. Here each sends its own.
 *
 * The conditional ones send the offset and leave the condition to the
 * caller, as libagon does; vdp_adv_jump_offset_if and
 * vdp_adv_call_offset_if send it too.
 */

static void at_offset(int bufferId, int command, int offset)
{
    if (offset & VDP_OFFSET_BLOCK)
        return;

    SEND(ADV(bufferId, command), U24(offset));
}

static void at_block(int bufferId, int command, int offset, int block)
{
    SEND(ADV(bufferId, command), U24(offset | VDP_OFFSET_BLOCK), W(block));
}

void vdp_adv_jump_offset(int bufferId, int offset)
{
    at_offset(bufferId, VDP_BUFFERED_OFFSET_JUMP, offset);
}

void vdp_adv_jump_offset_block(int bufferId, int offset, int block)
{
    at_block(bufferId, VDP_BUFFERED_OFFSET_JUMP, offset, block);
}

void vdp_adv_jump_offset_conditional(int bufferId, int offset)
{
    at_offset(bufferId, VDP_BUFFERED_OFFSET_COND_JUMP, offset);
}

void vdp_adv_jump_offset_block_conditional(int bufferId, int offset, int block)
{
    at_block(bufferId, VDP_BUFFERED_OFFSET_COND_JUMP, offset, block);
}

void vdp_adv_call_offset(int bufferId, int offset)
{
    at_offset(bufferId, VDP_BUFFERED_OFFSET_CALL, offset);
}

void vdp_adv_call_offset_block(int bufferId, int offset, int block)
{
    at_block(bufferId, VDP_BUFFERED_OFFSET_CALL, offset, block);
}

void vdp_adv_call_offset_conditional(int bufferId, int offset)
{
    at_offset(bufferId, VDP_BUFFERED_OFFSET_COND_CALL, offset);
}

void vdp_adv_call_offset_block_conditional(int bufferId, int offset, int block)
{
    at_block(bufferId, VDP_BUFFERED_OFFSET_COND_CALL, offset, block);
}

void vdp_adv_jump_offset_if(int bufferId, int offset, int operation, int checkBufferId, int checkOffset,
                            int operand)
{
    SEND(ADV(bufferId, VDP_BUFFERED_OFFSET_COND_JUMP), U24(offset));
    condition(operation, checkBufferId, checkOffset, operand);
}

void vdp_adv_call_offset_if(int bufferId, int offset, int operation, int checkBufferId, int checkOffset,
                            int operand)
{
    SEND(ADV(bufferId, VDP_BUFFERED_OFFSET_COND_CALL), U24(offset));
    condition(operation, checkBufferId, checkOffset, operand);
}

/*
 * Copying, splitting and spreading
 *
 * A list of buffers ends with 65535. libagon takes the list as int
 * arguments and sends the low 16 bits of each.
 */

static void id_list(int count, va_list ap)
{
    while (count-- > 0) {
        int id = va_arg(ap, int);

        SEND(W(id));
    }
    SEND(W(65535));
}

void vdp_adv_copy_multiple(int bufferId, int num_buffers, ...)
{
    va_list ap;

    SEND(ADV(bufferId, VDP_BUFFERED_COPY));
    va_start(ap, num_buffers);
    id_list(num_buffers, ap);
    va_end(ap);
}

void vdp_adv_consolidate(int bufferID)
{
    SEND(ADV(bufferID, VDP_BUFFERED_CONSOLIDATE));
}

void vdp_adv_split(int bufferID, int blockSize)
{
    SEND(ADV(bufferID, VDP_BUFFERED_SPLIT), W(blockSize));
}

void vdp_adv_split_multiple(int bufferId, int blockSize, int num_buffers, ...)
{
    va_list ap;

    SEND(ADV(bufferId, VDP_BUFFERED_SPLIT_INTO), W(blockSize));
    va_start(ap, num_buffers);
    id_list(num_buffers, ap);
    va_end(ap);
}

void vdp_adv_split_multiple_from(int bufferID, int blockSize, int targetBufferID)
{
    SEND(ADV(bufferID, VDP_BUFFERED_SPLIT_FROM), W(blockSize), W(targetBufferID));
}

void vdp_adv_split_by_width(int bufferID, int width, int blockCount)
{
    SEND(ADV(bufferID, VDP_BUFFERED_SPLIT_BY), W(width), W(blockCount));
}

void vdp_adv_split_by_width_multiple(int bufferId, int width, int num_buffers, ...)
{
    va_list ap;

    SEND(ADV(bufferId, VDP_BUFFERED_SPLIT_BY_INTO), W(width));
    va_start(ap, num_buffers);
    id_list(num_buffers, ap);
    va_end(ap);
}

void vdp_adv_split_by_width_multiple_from(int bufferID, int width, int blockCount, int targetBufferID)
{
    SEND(ADV(bufferID, VDP_BUFFERED_SPLIT_BY_FROM), W(width), W(blockCount), W(targetBufferID));
}

void vdp_adv_spread_multiple(int bufferId, int num_buffers, ...)
{
    va_list ap;

    SEND(ADV(bufferId, VDP_BUFFERED_SPREAD_INTO));
    va_start(ap, num_buffers);
    id_list(num_buffers, ap);
    va_end(ap);
}

void vdp_adv_spread_multiple_from(int bufferID, int targetBufferID)
{
    SEND(ADV(bufferID, VDP_BUFFERED_SPREAD_FROM), W(targetBufferID));
}

void vdp_adv_reverse_block_order(int bufferID)
{
    SEND(ADV(bufferID, VDP_BUFFERED_REVERSE_BLOCKS));
}

/* A value size follows the options only when both size bits are set, and a
 * chunk size only with CHUNKED. libagon decides otherwise: it sends a
 * value size for options 1 and 2 as well, both sizes for options 0, and
 * neither for any options above 4, CHUNKED or not -- each of which puts
 * the VDP out of step. */
void vdp_adv_reverse_block_data(int bufferID, int options, int valueSize, int chunkSize)
{
    SEND(ADV(bufferID, VDP_BUFFERED_REVERSE), options);
    if ((options & VDP_REVERSE_SIZE) == VDP_REVERSE_SIZE)
        SEND(W(valueSize));
    if (options & VDP_REVERSE_CHUNKED)
        SEND(W(chunkSize));
}

void vdp_adv_copy_multiple_by_reference(int bufferId, int num_buffers, ...)
{
    va_list ap;

    SEND(ADV(bufferId, VDP_BUFFERED_COPY_REF));
    va_start(ap, num_buffers);
    id_list(num_buffers, ap);
    va_end(ap);
}

void vdp_adv_copy_multiple_consolidate(int bufferId, int num_buffers, ...)
{
    va_list ap;

    SEND(ADV(bufferId, VDP_BUFFERED_COPY_AND_CONSOLIDATE));
    va_start(ap, num_buffers);
    id_list(num_buffers, ap);
    va_end(ap);
}

void vdp_adv_compress_buffer(int targetBufferID, int sourceBufferId)
{
    SEND(ADV(targetBufferID, VDP_BUFFERED_COMPRESS), W(sourceBufferId));
}

void vdp_adv_decompress_buffer(int targetBufferID, int sourceBufferId)
{
    SEND(ADV(targetBufferID, VDP_BUFFERED_DECOMPRESS), W(sourceBufferId));
}

/*
 * libagon's transforms
 *
 * Each makes a 2d affine transform with one operation, its arguments in
 * 16-bit fixed point: whole numbers (format &C0) for an angle in degrees
 * and a translation, 8 bits of fraction (format &C8) for a scale or a
 * shear, which libagon rounds to the nearest 1/256 and truncates to 16
 * bits. The VDP only takes any of them, or any other command from &20 to
 * &29, once VDP_VAR_AFFINE_TRANSFORM is set (VDU 23, 0, &F8, 1; 1;);
 * until then it reads the rest of the command as VDU codes. What draws
 * through one of these matrices, vdp_adv_use_affine_matrix, is in
 * <agon/vdp/bitmap.h>.
 */

#define FIXED_16            (VDP_FLOAT_FORMAT_16BIT | VDP_FLOAT_FORMAT_FIXED)

static int fixed8(float v)
{
    return (int) (long) roundf(v * 256.0f);
}

void vdp_adv_create_rotated_buffer(uint16_t transformID, uint16_t angle)
{
    SEND(ADV(transformID, VDP_BUFFERED_AFFINE_TRANSFORM), VDP_AFFINE_ROTATE, FIXED_16, W(angle));
}

void vdp_adv_create_scaled_buffer(uint16_t transformID, float scaleX, float scaleY)
{
    SEND(ADV(transformID, VDP_BUFFERED_AFFINE_TRANSFORM), VDP_AFFINE_SCALE, FIXED_16 | 8,
         W(fixed8(scaleX)), W(fixed8(scaleY)));
}

/* libagon sends x's high byte where y's should be. */
void vdp_adv_create_translated_buffer(uint16_t transformID, int16_t x, int16_t y)
{
    SEND(ADV(transformID, VDP_BUFFERED_AFFINE_TRANSFORM), VDP_AFFINE_TRANSLATE, FIXED_16, W(x), W(y));
}

/* A shear, as libagon has it, not the VDP's SKEW, which takes angles. */
void vdp_adv_create_skewed_buffer(uint16_t transformID, float scaleX, float scaleY)
{
    SEND(ADV(transformID, VDP_BUFFERED_AFFINE_TRANSFORM), VDP_AFFINE_SHEAR, FIXED_16 | 8,
         W(fixed8(scaleX)), W(fixed8(scaleY)));
}

/* Command &29 with an offset and a stride, on whole 16-bit numbers. */
void vdp_adv_apply_transformation_with_stride(uint16_t sourceID, uint16_t destID, uint16_t transformID,
                                              uint16_t offset, uint16_t stride)
{
    SEND(ADV(destID, VDP_BUFFERED_TRANSFORM_DATA),
         VDP_TRANSFORM_DATA_HAS_OFFSET | VDP_TRANSFORM_DATA_HAS_STRIDE, FIXED_16);
    SEND(W(transformID), W(sourceID), W(offset), W(stride));
}

/*
 * Affine transforms and matrices
 *
 * Their number arguments come after a format byte; format 0 is a 32-bit
 * float, which is what a float is here, in the VDP's byte order. With
 * MULTI_FORMAT every argument has a format byte of its own. IDENTITY and
 * INVERT take no arguments, so no format either.
 */

static void floats(int count, const float *values, int each)
{
    int i;

    for (i = 0; i < count; i++) {
        if (i == 0 || each)
            SEND(0);
        SEND_BYTES(&values[i], sizeof values[i]);
    }
}

static void affine(int bufferId, int command, int operation, int count, const float *values)
{
    SEND(ADV(bufferId, command), operation);
    floats(count, values, operation & VDP_AFFINE_OP_MULTI_FORMAT);
}

void vdp_adv_affine_identity(int bufferId)
{
    affine(bufferId, VDP_BUFFERED_AFFINE_TRANSFORM, VDP_AFFINE_IDENTITY, 0, 0);
}

void vdp_adv_affine_invert(int bufferId)
{
    affine(bufferId, VDP_BUFFERED_AFFINE_TRANSFORM, VDP_AFFINE_INVERT, 0, 0);
}

void vdp_adv_affine_float(int bufferId, int operation, int count, const float *values)
{
    affine(bufferId, VDP_BUFFERED_AFFINE_TRANSFORM, operation, count, values);
}

void vdp_adv_affine_args(int bufferId, int operation, const void *args, int length)
{
    SEND(ADV(bufferId, VDP_BUFFERED_AFFINE_TRANSFORM), operation);
    send_args(args, length);
}

void vdp_adv_affine_3d_identity(int bufferId)
{
    affine(bufferId, VDP_BUFFERED_AFFINE_TRANSFORM_3D, VDP_AFFINE_IDENTITY, 0, 0);
}

void vdp_adv_affine_3d_invert(int bufferId)
{
    affine(bufferId, VDP_BUFFERED_AFFINE_TRANSFORM_3D, VDP_AFFINE_INVERT, 0, 0);
}

void vdp_adv_affine_3d_float(int bufferId, int operation, int count, const float *values)
{
    affine(bufferId, VDP_BUFFERED_AFFINE_TRANSFORM_3D, operation, count, values);
}

void vdp_adv_affine_3d_args(int bufferId, int operation, const void *args, int length)
{
    SEND(ADV(bufferId, VDP_BUFFERED_AFFINE_TRANSFORM_3D), operation);
    send_args(args, length);
}

/* The matrix's size comes before any argument. Its number arguments share
 * one format byte. */
void vdp_adv_matrix_float(int bufferId, int operation, int rows, int columns, int count, const float *values)
{
    SEND(ADV(bufferId, VDP_BUFFERED_MATRIX), operation, rows, columns);
    floats(count, values, 0);
}

/* ADD, SUBTRACT and MULTIPLY: two source matrices. */
void vdp_adv_matrix_combine(int bufferId, int operation, int rows, int columns, int sourceId1, int sourceId2)
{
    SEND(ADV(bufferId, VDP_BUFFERED_MATRIX), operation, rows, columns, W(sourceId1), W(sourceId2));
}

void vdp_adv_matrix_args(int bufferId, int operation, int rows, int columns, const void *args, int length)
{
    SEND(ADV(bufferId, VDP_BUFFERED_MATRIX), operation, rows, columns);
    send_args(args, length);
}

/*
 * Transforming bitmaps and data
 */

void vdp_adv_transform_bitmap(int bufferId, int options, int transformBufferId, int bitmapId,
                              int width, int height)
{
    SEND(ADV(bufferId, VDP_BUFFERED_TRANSFORM_BITMAP), options, W(transformBufferId), W(bitmapId));
    if (options & VDP_TRANSFORM_BITMAP_EXPLICIT_SIZE)
        SEND(W(width), W(height));
}

/* The size is a byte; the offset is advanced with ADVANCED, but the
 * stride and limit are 16 bits either way. With BUFFER_ARGS each is a
 * buffer and offset instead, which is what the _args form is for. */
void vdp_adv_transform_data(int bufferId, int options, int format, int transformBufferId,
                            int sourceBufferId, int size, int offset, int stride, int limit)
{
    SEND(ADV(bufferId, VDP_BUFFERED_TRANSFORM_DATA), options, format,
         W(transformBufferId), W(sourceBufferId));
    if (options & VDP_TRANSFORM_DATA_HAS_SIZE)
        SEND(size);
    if (options & VDP_TRANSFORM_DATA_HAS_OFFSET)
        send_offset(options & VDP_TRANSFORM_DATA_ADVANCED, offset);
    if (options & VDP_TRANSFORM_DATA_HAS_STRIDE)
        SEND(W(stride));
    if (options & VDP_TRANSFORM_DATA_HAS_LIMIT)
        SEND(W(limit));
}

void vdp_adv_transform_data_args(int bufferId, int options, int format, int transformBufferId,
                                 int sourceBufferId, const void *args, int length)
{
    SEND(ADV(bufferId, VDP_BUFFERED_TRANSFORM_DATA), options, format,
         W(transformBufferId), W(sourceBufferId));
    send_args(args, length);
}

/*
 * The rest
 */

/* The default, sent only with USE_DEFAULT, is as wide as the value. */
void vdp_adv_read_variable(int bufferId, int options, int offset, int variableId, int defaultValue)
{
    SEND(ADV(bufferId, VDP_BUFFERED_READ_VARIABLE), options);
    send_offset(options & VDP_READ_VAR_ADVANCED_OFFSETS, offset);
    SEND(W(variableId));
    if (!(options & VDP_READ_VAR_USE_DEFAULT))
        return;

    if (options & VDP_READ_VAR_16BIT)
        SEND(W(defaultValue));
    else
        SEND(defaultValue);
}

static void expand_head(int bufferId, int options, int sourceBufferId, int width)
{
    SEND(ADV(bufferId, VDP_BUFFERED_EXPAND_BITMAP), options, W(sourceBufferId));
    if (options & VDP_EXPAND_BITMAP_ALIGNED)
        SEND(W(width));
}

/* The map is sent: a byte for each value a pixel can have, 2 to the bits
 * a pixel, which are 8 when the options say 0. */
void vdp_adv_expand_bitmap(int bufferId, int options, int sourceBufferId, int width, const void *map)
{
    int bits;

    options &= ~VDP_EXPAND_BITMAP_USEBUFFER;
    expand_head(bufferId, options, sourceBufferId, width);
    bits = options & VDP_EXPAND_BITMAP_SIZE;
    SEND_BYTES(map, 1 << (bits ? bits : 8));
}

void vdp_adv_expand_bitmap_mapped(int bufferId, int options, int sourceBufferId, int width, int mapBufferId)
{
    options |= VDP_EXPAND_BITMAP_USEBUFFER;
    expand_head(bufferId, options, sourceBufferId, width);
    SEND(W(mapBufferId));
}

void vdp_adv_add_callback(int bufferId, int type)
{
    SEND(ADV(bufferId, VDP_BUFFERED_ADD_CALLBACK), W(type));
}

/* 65535 as the buffer removes every buffer from that type; as the type,
 * the buffer from every type. */
void vdp_adv_remove_callback(int bufferId, int type)
{
    SEND(ADV(bufferId, VDP_BUFFERED_REMOVE_CALLBACK), W(type));
}

/* Logged by the VDP to its own debug output only. */
void vdp_adv_debug_info(int bufferId)
{
    SEND(ADV(bufferId, VDP_BUFFERED_DEBUG_INFO));
}
