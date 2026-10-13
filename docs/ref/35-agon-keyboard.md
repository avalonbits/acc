# `agon/keyboard.h`

The keyboard, as libagon names it: a buffer of key events, filled as MOS
gets each key, so that a program busy for a while between polls still
gets every key it was sent, in order.

MOS sends a packet for every key going down and every key coming up.
`kbuf_init` has MOS call a handler of acc's runtime on each one (with
`mos_setkbvector`, MOS call 0x1D; see [agon/mos.h](27-agon-mos.md)), and
the handler copies the packet's four bytes into a ring on the heap. The
program takes them out with `kbuf_poll_event`. The header includes
`stdint.h` and `stdbool.h`.

    #include <agon/keyboard.h>

    struct keyboard_event_t e;

    kbuf_init(16);
    for (;;) {
        if (!kbuf_poll_event(&e) || !e.isdown)
            continue;
        if (e.ascii == 27)
            break;
        handle(e.vkey);
    }
    kbuf_deinit();

## `keyboard_event_t`

    struct keyboard_event_t {
        uint8_t ascii;
        uint8_t kmod;
        uint8_t vkey;
        uint8_t isdown;
    };

One key going down or coming up, as MOS's packet has it: the character
it means, which modifiers were held, which physical key it was (FabGL's
virtual key code, as MOS's `sysvar_vkeycode` holds it), and which way it
went (`isdown` is nonzero for down). Every field is a byte, so how a
struct is laid out does not come into it.

## `kbuf_init`

    void kbuf_init(uint8_t buf_len);

Start the buffer, with room for `buf_len` events, and have MOS call the
handler on every key packet from now on. The ring is
`(buf_len + 1) * 4` bytes from `malloc`. When it is full, the newest
events are dropped: the ones already in it are kept.

A buffer already started is ended first, with `kbuf_deinit`, and the
events in it are lost. When `malloc` fails there is no buffer and no
handler, and `kbuf_poll_event` answers `false`. A `buf_len` of 0 makes
a buffer that holds nothing.

## `kbuf_poll_event`

    bool kbuf_poll_event(struct keyboard_event_t *e);

Take the oldest event out of the buffer into `*e`, and answer `true`; or
answer `false`, with `*e` untouched, when the buffer is empty or was
never started. It does not wait.

## `kbuf_clear`

    void kbuf_clear(void);

Drop every event the buffer holds. The handler stays, and the next key
packet goes in as usual.

## `kbuf_deinit`

    void kbuf_deinit(void);

End the buffer: tell MOS to call no handler (`mos_setkbvector` with
`NULL`), and free the ring. It does nothing when no buffer was started.

The end of the program ends a buffer still running, however the
program ends: returning from `main`, `exit`, `_Exit` or `abort`. MOS
would otherwise go on calling the handler, which was in the program,
after the program had gone. A program needs `kbuf_deinit` only to stop
taking keys before it ends.
