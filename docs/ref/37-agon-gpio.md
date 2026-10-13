# `agon/gpio.h`

The eZ80's GPIO pins, as libagon names them: ports B, C and D, eight
pins each, set up, written and read through the I/O space.

A port has four registers, at consecutive I/O ports: its data register
(the port's own number), then the data direction (a 1 bit makes that pin
an input), then `ALT1` and `ALT2`, which choose an alternate function and
are both 0 for a plain pin. The header includes `ez80f92.h`, which names
all four of each port (see [ez80f92.h](39-ez80f92.md)), and `stdbool.h` and
`stdint.h`.

Every function comes in two forms. One takes a port and a pin number,
0 to 7 (only the low three bits of it count). The other, ending in
`PinID`, takes a pin ID: a port in the high byte and a mask of its pins
in the low, so that one call does several pins of a port at once. Every
change is a read of the register, the bits changed, and a write back, so
the port's other pins keep what they had.

    #include <agon/gpio.h>

    outputMode(PORTC, 3);
    outputHigh(PORTC, 3);

    outputModePinID(PC0 | PC1 | PC7);
    outputLowPinID(PC0 | PC7);

## `PORTB`, `PORTC`, `PORTD`

| Name    | Value   | I/O port |
|---------|---------|----------|
| `PORTB` | `PB_DR` | 0x9A     |
| `PORTC` | `PC_DR` | 0x9E     |
| `PORTD` | `PD_DR` | 0xA2     |

The port numbers the functions with a `portID` take: each is the port's
data register. Its direction, `ALT1` and `ALT2` registers are the next
three ports up.

## `PB0`, `PB1`, `PB2`, `PB3`, `PB4`, `PB5`, `PB6`, `PB7`, `PC0`, `PC1`, `PC2`, `PC3`, `PC4`, `PC5`, `PC6`, `PC7`, `PD0`, `PD1`, `PD2`, `PD3`, `PD4`, `PD5`, `PD6`, `PD7`

    #define PB0 ((PB_DR << 8) | 0x01)
    ...
    #define PD7 ((PD_DR << 8) | 0x80)

The pin IDs: `PB0` is port B's data register in the high byte and pin
0's bit, 0x01, in the low; `PB7`'s bit is 0x80. IDs of one port combine
with `|` into an ID for several of its pins. IDs of two ports do not:
their port numbers would be or-ed together too.

## `outputMode`, `outputModePinID`

    void outputMode(uint8_t portID, uint8_t pinnumber);
    void outputModePinID(uint16_t pinID);

Make the pins outputs: clear their bits in `ALT2`, then `ALT1`, then the
direction register. The alternate bits go first, so that the pin passes
through no other function on the way.

## `inputMode`, `inputModePinID`

    void inputMode(uint8_t portID, uint8_t pinnumber);
    void inputModePinID(uint16_t pinID);

Make the pins inputs: clear their bits in `ALT2`, then `ALT1`, then set
them in the direction register.

## `outputHigh`, `outputHighPinID`, `outputLow`, `outputLowPinID`

    void outputHigh(uint8_t portID, uint8_t pinnumber);
    void outputHighPinID(uint16_t pinID);
    void outputLow(uint8_t portID, uint8_t pinnumber);
    void outputLowPinID(uint16_t pinID);

`outputHigh` sets the pins' bits in the port's data register, and
`outputLow` clears them. Neither changes a pin's direction: make it an
output with `outputMode` first.

## `input`, `inputPinID`, `inputPort`

    bool    input(uint8_t portID, uint8_t pinnumber);
    bool    inputPinID(uint16_t pinID);
    uint8_t inputPort(uint8_t port);

`input` reads the port's data register and answers whether the pin's bit
is 1. `inputPinID` answers whether any of the pins in the ID is 1.

`inputPort` answers the whole data register, a bit a pin. It is
`io_in(port)`, so it reads any I/O port it is given.
