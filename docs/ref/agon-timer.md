# `agon/timer.h`

A delay, as libagon names it, counted by the eZ80's timer 0.

## `delay`

    void delay(unsigned int ms);

Wait `ms` milliseconds. A `ms` of 0 returns at once.

`delay` busy-waits on timer 0 of the eZ80 (see
[ez80f92.h](ez80f92.md)): it stops the timer (`TMR0_CTL`, I/O port
0x80), loads 72 into its reload registers (`TMR0_RR_L` and `TMR0_RR_H`),
reads `TMR0_CTL` once to clear any flag left over, and starts it running
continuously on the 18.432 MHz clock divided by 256. That is 72,000
counts a second, so the timer comes round once a millisecond and sets
the top bit of `TMR0_CTL`, which reading clears. `delay` waits for that
bit once for every millisecond asked for, then stops the timer again.

MOS keeps timer 0 for itself but does nothing with it, which is why
libagon uses it too. A program that runs timer 0 itself loses what it
set up there at every `delay`.

    #include <agon/timer.h>

    delay(500);         /* half a second */
