/*
 * agon/joystick.h -- the two joysticks on the Console8's GPIO, as libagon
 * names them.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * getJoystickButtons answers port C in its low byte -- the directions, the
 * two joysticks interleaved -- and port D in its high byte, whose top four
 * bits are the buttons. A pin reads 0 while its switch is closed, which is
 * why each test below is a NOT.
 */
#pragma once
#ifndef ACC_AGON_JOYSTICK_H
#define ACC_AGON_JOYSTICK_H

#include <stdint.h>

#define joystick2_UP(buttons)         (!((buttons) & (1U << (0))))
#define joystick1_UP(buttons)         (!((buttons) & (1U << (1))))
#define joystick2_DOWN(buttons)       (!((buttons) & (1U << (2))))
#define joystick1_DOWN(buttons)       (!((buttons) & (1U << (3))))
#define joystick2_LEFT(buttons)       (!((buttons) & (1U << (4))))
#define joystick1_LEFT(buttons)       (!((buttons) & (1U << (5))))
#define joystick2_RIGHT(buttons)      (!((buttons) & (1U << (6))))
#define joystick1_RIGHT(buttons)      (!((buttons) & (1U << (7))))
#define joystick2_BUTTON1(buttons)    (!((buttons) & (1U << (12))))
#define joystick1_BUTTON1(buttons)    (!((buttons) & (1U << (13))))
#define joystick2_BUTTON2(buttons)    (!((buttons) & (1U << (14))))
#define joystick1_BUTTON2(buttons)    (!((buttons) & (1U << (15))))

uint16_t getJoystickButtons(void);
void     resetJoysticks(void);

#endif
