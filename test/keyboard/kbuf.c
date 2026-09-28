/* kbuf, as a program sees it: every key packet MOS gets while the program is
 * busy is kept, in order, until it is polled -- up to the length kbuf_init
 * was given -- and not only the last.
 *
 * `kbuf <length> <packets>`: starts a buffer of <length>, says READY, then
 * waits without polling until MOS has counted <packets> more key packets,
 * and prints every event the buffer then gives, as <ascii in hex><d or u>.
 * test/keyboard.sh types the keys. */
#include <stdio.h>
#include <stdlib.h>
#include <agon/keyboard.h>
#include <agon/mos.h>

int main(int argc, char **argv)
{
    volatile uint8_t *v = mos_sysvars();
    struct keyboard_event_t e;
    uint8_t start;
    int packets, n = 0;

    if (argc != 3)
        return 1;
    packets = atoi(argv[2]);

    kbuf_init((uint8_t) atoi(argv[1]));
    start = v[sysvar_vkeycount];
    printf("READY\r\n");
    while ((uint8_t) (v[sysvar_vkeycount] - start) < packets)
        ;

    printf("GOT");
    while (kbuf_poll_event(&e)) {
        printf(" %02x%c", e.ascii, e.isdown ? 'd' : 'u');
        n++;
    }
    printf("\r\nEVENTS %d\r\n", n);
    kbuf_deinit();

    return 0;
}
