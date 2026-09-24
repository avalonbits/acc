/*
 * delay, counted by the eZ80's timer 0.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The timer counts the 18.432 MHz clock divided by 256 -- 72,000 a second,
 * 72 a millisecond -- down from 72, starts again at zero, and says so in
 * the top bit of its control register, which reading clears. So a delay is
 * that bit seen once for every millisecond asked for. MOS keeps timer 0 to
 * itself for nothing, which is why libagon uses it too.
 */
#include <agon/timer.h>
#include <ez80f92.h>

#define PRT_IRQ         0x80
#define PRT_CONTINUOUS  0x10
#define PRT_DIV256      0x0c
#define PRT_RST_EN      0x02
#define PRT_EN          0x01

void delay(unsigned int ms)
{
    io_out(TMR0_CTL, 0);
    io_out(TMR0_RR_L, 72);
    io_out(TMR0_RR_H, 0);
    io_in(TMR0_CTL);                    /* any flag left over, cleared */
    io_out(TMR0_CTL, PRT_CONTINUOUS | PRT_DIV256 | PRT_RST_EN | PRT_EN);
    while (ms--)
        while (!(io_in(TMR0_CTL) & PRT_IRQ))
            ;
    io_out(TMR0_CTL, 0);
}
