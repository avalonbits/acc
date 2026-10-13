# `agon/joystick.h`

The two joysticks on the Console8's GPIO pins, as libagon names them:
one call reads them, and a macro for each switch tests what it read.

A pin reads 0 while its switch is closed, so each macro is a NOT of its
bit: it is true while that direction is pushed or that button is held.
The header includes `stdint.h`.

    #include <agon/joystick.h>

    uint16_t b;

    resetJoysticks();
    b = getJoystickButtons();
    if (joystick1_UP(b))
        y--;
    if (joystick1_BUTTON1(b))
        fire();

## `getJoystickButtons`

    uint16_t getJoystickButtons(void);

Read both joysticks: port C's data register (`PC_DR`, I/O port 0x9E) in
the low byte, the directions of the two joysticks interleaved, and port
D's (`PD_DR`, 0xA2) in the high byte, whose top four bits are the
buttons. Pass the answer to the macros below.

## `resetJoysticks`

    void resetJoysticks(void);

Make every pin of port C, and the top four of port D, inputs, as
`inputModePinID` does (see [agon/gpio.h](37-agon-gpio.md)): their `ALT2`
and `ALT1` bits cleared, and their direction bits set: the pins
`getJoystickButtons` reads, set up to be read.

## `joystick1_UP`, `joystick1_DOWN`, `joystick1_LEFT`, `joystick1_RIGHT`, `joystick2_UP`, `joystick2_DOWN`, `joystick2_LEFT`, `joystick2_RIGHT`

    #define joystick1_UP(buttons) (!((buttons) & (1U << (1))))

True while the direction is pushed. `buttons` is what
`getJoystickButtons` answered.

| Bit | Pin | Joystick 1        | Joystick 2        |
|-----|-----|-------------------|-------------------|
| 0   | PC0 |                   | `joystick2_UP`    |
| 1   | PC1 | `joystick1_UP`    |                   |
| 2   | PC2 |                   | `joystick2_DOWN`  |
| 3   | PC3 | `joystick1_DOWN`  |                   |
| 4   | PC4 |                   | `joystick2_LEFT`  |
| 5   | PC5 | `joystick1_LEFT`  |                   |
| 6   | PC6 |                   | `joystick2_RIGHT` |
| 7   | PC7 | `joystick1_RIGHT` |                   |

## `joystick1_BUTTON1`, `joystick1_BUTTON2`, `joystick2_BUTTON1`, `joystick2_BUTTON2`

    #define joystick1_BUTTON1(buttons) (!((buttons) & (1U << (13))))

True while the button is held.

| Bit | Pin | Joystick 1          | Joystick 2          |
|-----|-----|---------------------|---------------------|
| 12  | PD4 |                     | `joystick2_BUTTON1` |
| 13  | PD5 | `joystick1_BUTTON1` |                     |
| 14  | PD6 |                     | `joystick2_BUTTON2` |
| 15  | PD7 | `joystick1_BUTTON2` |                     |
