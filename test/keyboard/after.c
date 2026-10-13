/* The program run after one that left a key buffer behind: it loads where
 * that one was, writes over the rest of the memory it had, and waits for
 * eight key packets as kbuf does, without a buffer of its own. A key
 * handler still in MOS's hands is then a call into these bytes, and the
 * program never gets to say GOT. */
#include <stdio.h>
#include <string.h>
#include <agon/mos.h>

static unsigned char over[0x10000];

int main(void)
{
    volatile uint8_t *v = mos_sysvars();
    uint8_t start;

    memset(over, 0xff, sizeof over);
    start = v[sysvar_vkeycount];
    printf("READY\r\n");
    while ((uint8_t) (v[sysvar_vkeycount] - start) < 8)
        ;
    printf("GOT alive\r\nEVENTS 0\r\n");

    return 0;
}
