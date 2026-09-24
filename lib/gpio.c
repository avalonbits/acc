/*
 * The eZ80's I/O space, its GPIO pins, and the joysticks on them.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * A pin is one bit of a port's four registers: its data (the port's own
 * number), its direction one up (1 for input), and two more that choose an
 * alternate function, 0 and 0 for a plain pin. Making a pin an output or
 * an input clears the two alternate bits first, so that it passes through
 * no other function on the way.
 */
#include <agon/gpio.h>
#include <agon/joystick.h>

int  acc_rt_port_in(int port);
void acc_rt_port_out(int port, int value);

uint8_t io_in(int addr)
{
    return (uint8_t) acc_rt_port_in(addr);
}

void io_out(int addr, uint8_t value)
{
    acc_rt_port_out(addr, value);
}

#define DDR(port)   ((port) + 1)
#define ALT1(port)  ((port) + 2)
#define ALT2(port)  ((port) + 3)

static void clear_bits(int reg, uint8_t mask)
{
    io_out(reg, (uint8_t) (io_in(reg) & ~mask));
}

static void set_bits(int reg, uint8_t mask)
{
    io_out(reg, (uint8_t) (io_in(reg) | mask));
}

static void mode(int port, uint8_t mask, int as_input)
{
    clear_bits(ALT2(port), mask);
    clear_bits(ALT1(port), mask);
    if (as_input)
        set_bits(DDR(port), mask);
    else
        clear_bits(DDR(port), mask);
}

void outputModePinID(uint16_t pinID)
{
    mode(pinID >> 8, (uint8_t) pinID, 0);
}

void outputMode(uint8_t portID, uint8_t pinnumber)
{
    mode(portID, (uint8_t) (1 << (pinnumber & 7)), 0);
}

void inputModePinID(uint16_t pinID)
{
    mode(pinID >> 8, (uint8_t) pinID, 1);
}

void inputMode(uint8_t portID, uint8_t pinnumber)
{
    mode(portID, (uint8_t) (1 << (pinnumber & 7)), 1);
}

void outputHighPinID(uint16_t pinID)
{
    set_bits(pinID >> 8, (uint8_t) pinID);
}

void outputHigh(uint8_t portID, uint8_t pinnumber)
{
    set_bits(portID, (uint8_t) (1 << (pinnumber & 7)));
}

void outputLowPinID(uint16_t pinID)
{
    clear_bits(pinID >> 8, (uint8_t) pinID);
}

void outputLow(uint8_t portID, uint8_t pinnumber)
{
    clear_bits(portID, (uint8_t) (1 << (pinnumber & 7)));
}

bool inputPinID(uint16_t pinID)
{
    return (io_in(pinID >> 8) & (uint8_t) pinID) != 0;
}

bool input(uint8_t portID, uint8_t pinnumber)
{
    return (io_in(portID) & (1 << (pinnumber & 7))) != 0;
}

uint8_t inputPort(uint8_t port)
{
    return io_in(port);
}

/* Port C in the low byte and port D in the high, as the macros in
 * <agon/joystick.h> read them. */
uint16_t getJoystickButtons(void)
{
    return (uint16_t) (io_in(PC_DR) | io_in(PD_DR) << 8);
}

/* Every pin of port C, and the top four of port D, as inputs. */
void resetJoysticks(void)
{
    mode(PC_DR, 0xff, 1);
    mode(PD_DR, 0xf0, 1);
}
