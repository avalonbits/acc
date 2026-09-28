/* kbuf, as a program sees it: every key packet MOS gets while the program is
 * busy is kept, in order, until it is polled -- up to the length kbuf_init
 * was given -- and not only the last.
 *
 * `kbuf <length> <packets> <rounds> [clear]`: starts a buffer of <length>,
 * then <rounds> times says READY, waits without polling until MOS has
 * counted <packets> more key packets, and prints every event the buffer
 * then gives, as <ascii in hex><d or u>. With `clear`, it empties the buffer
 * before reading it. test/keyboard.sh types the keys. */
#include <stdio.h>
#include <stdlib.h>
#include <agon/keyboard.h>
#include <agon/mos.h>

int main(int argc, char **argv)
{
    volatile uint8_t *v = mos_sysvars();
    struct keyboard_event_t e;
    uint8_t start;
    int packets, rounds, n = 0;

    if (argc < 4)
        return 1;
    packets = atoi(argv[2]);
    rounds = atoi(argv[3]);

    kbuf_init((uint8_t) atoi(argv[1]));
    while (rounds-- > 0) {
        start = v[sysvar_vkeycount];
        printf("READY\r\n");
        while ((uint8_t) (v[sysvar_vkeycount] - start) < packets)
            ;
        if (argc > 4)
            kbuf_clear();

        printf("GOT");
        while (kbuf_poll_event(&e)) {
            printf(" %02x%c", e.ascii, e.isdown ? 'd' : 'u');
            n++;
        }
        printf("\r\n");
    }
    printf("EVENTS %d\r\n", n);
    kbuf_deinit();

    return 0;
}
