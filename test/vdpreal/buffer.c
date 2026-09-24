/* <agon/vdp/buffer.h> against the VDP: what the buffered commands do, not
 * only what they send. The buffers hold text cursor moves -- a TAB(x, y),
 * or VDU 9s, each a column to the right -- so where the cursor ends up
 * says what ran: whether a buffer was called, from which offset or block,
 * and what an adjust or a variable read left in it. */
#include "result.h"
#include <agon/vdp.h>

static void cursor(const char *what)
{
    uint8_t x = 99, y = 99;

    vdp_return_text_cursor_position(&x, &y);
    say("%s %d %d\n", what, x, y);
}

/* From the top left. */
static void home(void)
{
    vdp_cursor_tab(0, 0);
}

int main(void)
{
    static char tab[] = { 31, 2, 3 };
    static char right4[] = { 9, 9, 9, 9 };
    static char jump13[] = { 23, 0, 0xa0, 13, 0, 7, 9 };
    static const unsigned char set91[] = { 9, 1 };
    static const unsigned char setvar[] = { 23, 0, 0xf8, 0x34, 0x12, 5, 0 };
    static const unsigned char x_exists[] = { VDP_COND_EXISTS, 10, 0, 1, 0 };
    static const unsigned char x_is_x[] = { 10, 0, 1, 0, 10, 0, 1, 0 };
    int i;

    vdp_mode(8);
    vdp_adv_clear_buffer(65535);

    /* Buffer 10 is a TAB, and adjusting it moves where it goes. */
    vdp_adv_write_block_data(10, 3, tab);
    home();
    vdp_adv_call_buffer(10);
    cursor("call");
    vdp_adv_adjust_value(10, VDP_ADJUST_ADD, 1, 5);
    vdp_adv_call_buffer(10);
    cursor("adjust add x 5");
    vdp_adv_adjust(10, VDP_ADJUST_ADD, 2);
    putch(4);
    vdp_adv_call_buffer(10);
    cursor("adjust add y 4");
    vdp_adv_adjust_multi(10, VDP_ADJUST_SET, 1, 2, 4);
    vdp_adv_call_buffer(10);
    cursor("adjust set both 4");
    vdp_adv_adjust_multi_data(10, VDP_ADJUST_SET | VDP_ADJUST_MULTI_TARGET, 1, 2, set91);
    vdp_adv_call_buffer(10);
    cursor("adjust set 9 1");
    /* Without MULTI_TARGET every operand goes to the one byte. */
    vdp_adv_adjust_multi_data(10, VDP_ADJUST_ADD, 2, 2, set91);
    vdp_adv_call_buffer(10);
    cursor("adjust add 9 and 1 to y");
    vdp_adv_adjust_value(10, VDP_ADJUST_SET, 2, 1);

    /* Buffer 12 is four VDU 9s in one block, 13 three blocks of one. */
    vdp_adv_write_block_data(12, 4, right4);
    for (i = 0; i < 3; i++)
        vdp_adv_write_block_data(13, 1, right4);
    home();
    vdp_adv_call_offset(12, 3);
    cursor("offset 3");
    home();
    vdp_adv_call_offset(12, 1);
    cursor("offset 1");
    home();
    vdp_adv_call_buffer(13);
    cursor("blocks");
    home();
    vdp_adv_call_offset_block(13, 0, 2);
    cursor("block 2");
    home();
    vdp_adv_jump_offset_block(13, 0, 1);
    cursor("jump block 1");

    /* Conditions. VDP 2.16.0 compares a value from a buffer wrongly: it
     * reads the byte (or two) into the bottom of an int32_t that holds -1,
     * so the value is negative, and equal to no operand sent inline. A
     * VDP variable's value is compared right, so the conditions with an
     * operand check variable 0x1234, set to 5. Two buffer values are
     * wrong the same way, which leaves equality between them right. */
    mos_puts((const char *) setvar, sizeof setvar, 0);
    home();
    vdp_adv_call_if(12, VDP_COND_VAR_VALUE | VDP_COND_EQUAL, 0x1234, 0, 5);
    cursor("if var == 5");
    home();
    vdp_adv_call_if(12, VDP_COND_VAR_VALUE | VDP_COND_EQUAL, 0x1234, 0, 6);
    cursor("if var == 6");
    home();
    vdp_adv_call_if(12, VDP_COND_VAR_VALUE | VDP_COND_16BIT | VDP_COND_LESS, 0x1234, 0, 0x105);
    cursor("if var < 0x105");
    home();
    vdp_adv_call_offset_if(12, 2, VDP_COND_VAR_VALUE | VDP_COND_GREATER, 0x1234, 0, 4);
    cursor("offset 2 if var > 4");
    home();
    vdp_adv_jump_if(12, VDP_COND_VAR_VALUE | VDP_COND_NOT_EXISTS, 0x4321, 0, 0);
    cursor("if no var 0x4321");
    home();
    vdp_adv_call_if_args(12, VDP_COND_EQUAL | VDP_COND_BUFFER_VALUE, x_is_x, sizeof x_is_x);
    cursor("if x == x");
    home();
    vdp_adv_call_offset_block_conditional(13, 0, 1);
    mos_puts((const char *) x_exists, sizeof x_exists, 0);
    cursor("block 1 if x");

    /* A jump inside a buffer does not come back: 15 jumps to 13, and the
     * VDU 9 after the jump never runs. */
    vdp_adv_write_block_data(15, 7, jump13);
    home();
    vdp_adv_call_buffer(15);
    cursor("jump");

    /* A VDP variable into buffer 10, written anew (a write adds a block):
     * TAB(5, 6), 6 the default for a variable never set. */
    vdp_adv_clear_buffer(10);
    vdp_adv_write_block_data(10, 3, tab);
    vdp_adv_read_variable(10, 0, 1, 0x1234, 0);
    vdp_adv_read_variable(10, VDP_READ_VAR_USE_DEFAULT, 2, 0x4321, 6);
    vdp_adv_call_buffer(10);
    cursor("variable");

    /* Copies: 12's four and 13's three. */
    vdp_adv_copy_multiple(14, 2, 12, 13);
    home();
    vdp_adv_call_buffer(14);
    cursor("copy");
    vdp_adv_consolidate(14);
    home();
    vdp_adv_call_offset_block(14, 6, 0);
    cursor("consolidated, offset 6");

    return finish();
}
