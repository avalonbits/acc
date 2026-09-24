/* <agon/gpio.h>, <agon/joystick.h> and <agon/timer.h> against libagon: after
 * each call, every register of the port it touched, read back. The
 * emulator keeps the GPIO registers as the chip does. delay is timed in
 * cycles by test/lib.sh: at -u the emulator's clock, which MOS's comes
 * from, does not keep pace with the eZ80's. */
#include <stdio.h>

#include <agon/gpio.h>
#include <agon/joystick.h>
#include <agon/timer.h>

static void regs(const char *what, int port)
{
    printf("%-18s DR %02x DDR %02x ALT1 %02x ALT2 %02x\n", what, io_in(port),
           io_in(port + 1), io_in(port + 2), io_in(port + 3));
}

int main(void)
{
    int i;

    regs("port C at start", PORTC);
    regs("port D at start", PORTD);
    outputMode(PORTC, 3);
    regs("outputMode C3", PORTC);
    outputHigh(PORTC, 3);
    regs("outputHigh C3", PORTC);
    printf("input C3 %d\n", input(PORTC, 3));
    outputLow(PORTC, 3);
    regs("outputLow C3", PORTC);
    printf("input C3 %d inputPinID %d\n", input(PORTC, 3), inputPinID(PC3));
    outputModePinID(PC0 | PC1 | PC7);
    regs("outputModePinID", PORTC);
    outputHighPinID(PC0 | PC7);
    regs("outputHighPinID", PORTC);
    outputLowPinID(PC7);
    regs("outputLowPinID", PORTC);
    printf("inputPort C %02x\n", inputPort(PORTC));
    inputMode(PORTC, 3);
    regs("inputMode C3", PORTC);
    inputModePinID(PC0 | PC1);
    regs("inputModePinID", PORTC);
    outputModePinID(PD4 | PD6);
    outputLowPinID(PD4 | PD6);
    regs("port D out", PORTD);
    printf("buttons %04x\n", getJoystickButtons());
    resetJoysticks();
    regs("reset C", PORTC);
    regs("reset D", PORTD);
    i = getJoystickButtons();
    printf("buttons %04x up1 %d up2 %d b1 %d\n", i, joystick1_UP(i), joystick2_UP(i),
           joystick1_BUTTON1(i));
    io_out(PB_DR, 0x5a);
    printf("io_out PB %02x\n", io_in(PB_DR));

    delay(0);
    delay(1);
    printf("delay returned\n");

    return 0;
}
