/* Csmith's platform_generic.h, for test/csmith.sh: a program Csmith
 * writes ends by printing a CRC of every global it has, and its own prints
 * that uint32_t with %X, which reads an unsigned int -- 24 bits here, so
 * the top byte of the answer would be lost, or worse. This prints it as
 * the unsigned long it is. Nothing else differs; test/csmith.sh puts this
 * in a copy of Csmith's runtime in place of its own. */
#ifndef PLATFORM_GENERIC_H
#define PLATFORM_GENERIC_H

#include <stdio.h>

static void platform_main_begin(void)
{
}

static void platform_main_end(uint32_t crc, int flag)
{
    (void) flag;
    printf("checksum = %lX\n", (unsigned long) crc);
}

#define MB (1 << 20)

#endif
