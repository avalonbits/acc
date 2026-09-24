/*
 * agon/gpio.h -- the GPIO pins, as libagon names them.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Ports B, C and D, eight pins each. A port is named by its data register
 * (PORTB is PB_DR), and each of its other three registers is the next port
 * number up: the direction (1 for input), then the two that choose an
 * alternate function, both 0 for a plain pin. A pin ID is a port and a mask
 * of pins together, for the calls that do several of one port at once.
 */
#ifndef ACC_AGON_GPIO_H
#define ACC_AGON_GPIO_H

#include <stdbool.h>
#include <stdint.h>
#include <ez80f92.h>

void    outputMode(uint8_t portID, uint8_t pinnumber);
void    outputModePinID(uint16_t pinID);
void    inputMode(uint8_t portID, uint8_t pinnumber);
void    inputModePinID(uint16_t pinID);
void    outputHigh(uint8_t portID, uint8_t pinnumber);
void    outputHighPinID(uint16_t pinID);
void    outputLow(uint8_t portID, uint8_t pinnumber);
void    outputLowPinID(uint16_t pinID);
bool    input(uint8_t portID, uint8_t pinnumber);
bool    inputPinID(uint16_t pinID);
uint8_t inputPort(uint8_t port);

#define PORTB PB_DR
#define PORTC PC_DR
#define PORTD PD_DR

#define PB0 ((PB_DR << 8) | 0x01)
#define PB1 ((PB_DR << 8) | 0x02)
#define PB2 ((PB_DR << 8) | 0x04)
#define PB3 ((PB_DR << 8) | 0x08)
#define PB4 ((PB_DR << 8) | 0x10)
#define PB5 ((PB_DR << 8) | 0x20)
#define PB6 ((PB_DR << 8) | 0x40)
#define PB7 ((PB_DR << 8) | 0x80)
#define PC0 ((PC_DR << 8) | 0x01)
#define PC1 ((PC_DR << 8) | 0x02)
#define PC2 ((PC_DR << 8) | 0x04)
#define PC3 ((PC_DR << 8) | 0x08)
#define PC4 ((PC_DR << 8) | 0x10)
#define PC5 ((PC_DR << 8) | 0x20)
#define PC6 ((PC_DR << 8) | 0x40)
#define PC7 ((PC_DR << 8) | 0x80)
#define PD0 ((PD_DR << 8) | 0x01)
#define PD1 ((PD_DR << 8) | 0x02)
#define PD2 ((PD_DR << 8) | 0x04)
#define PD3 ((PD_DR << 8) | 0x08)
#define PD4 ((PD_DR << 8) | 0x10)
#define PD5 ((PD_DR << 8) | 0x20)
#define PD6 ((PD_DR << 8) | 0x40)
#define PD7 ((PD_DR << 8) | 0x80)

#endif
