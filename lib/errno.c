/*
 * errno, and the two functions that turn it into words: strerror and perror.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * A program that only reads errno takes the int and nothing else; the words
 * come with strerror, and the file layer only with perror.
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>

int errno;

/* The words are glibc's, so that a message reads the same on the Agon as
 * it does where the program was written, and test/hosted.sh can hold them
 * to the host's. */
char *strerror(int n)
{
    static char unknown[] = "Unknown error -8388608";
    char *p = unknown + sizeof unknown - 1;
    unsigned u = n < 0 ? 0u - (unsigned) n : (unsigned) n;

    switch (n) {
    case 0:      return "Success";
    case EDOM:   return "Numerical argument out of domain";
    case ERANGE: return "Numerical result out of range";
    case EILSEQ: return "Invalid or incomplete multibyte or wide character";
    case EINVAL: return "Invalid argument";
    }

    /* The number written backwards from the end of the buffer, and the
     * words moved up against it. */
    *p = 0;
    do
        *--p = (char) ('0' + u % 10);
    while (u /= 10);
    if (n < 0)
        *--p = '-';
    p -= 14;
    memcpy(p, "Unknown error ", 14);

    return p;
}

/* The message for errno on stderr, after `s` and a colon when there is an
 * `s` to put in front of it. Read before anything is written, since C asks
 * for the message errno had when perror was called. */
void perror(const char *s)
{
    const char *message = strerror(errno);

    if (s && *s) {
        fputs(s, stderr);
        fputs(": ", stderr);
    }
    fputs(message, stderr);
    fputc('\n', stderr);
}
