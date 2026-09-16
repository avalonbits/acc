/*
 * What acc needs from the host that MOS does not provide.
 *
 * tinycc reads its inputs through POSIX file descriptors. MOS has no such
 * thing and libagon has stdio, so the four calls acc actually uses are
 * implemented here over FILE *. Everything else in this file is a stub for
 * something the Agon has no equivalent of -- an environment, symbolic links,
 * a working directory that can be asked for -- and acc's paths through those
 * are the ones that do nothing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fcntl.h"

/* Descriptors are indices into this table. Eight is generous: acc has the
 * source open, plus its include stack, plus one library at a time. */
#define NFD 8

static FILE *fdtab[NFD];

int open(const char *path, int flags, ...)
{
    int i;
    FILE *f;
    const char *mode;

    /* Writing matters as well as reading: `acc -c` creates its object with
     * open() and then hands the descriptor to fdopen(). The flat-image writer
     * uses fopen directly, but the object path does not. */
    if ((flags & 3) == O_RDONLY)
        mode = "rb";
    else if (flags & O_TRUNC)
        mode = "wb";
    else if ((flags & 3) == O_RDWR)
        mode = "r+b";
    else
        mode = "wb";

    for (i = 0; i < NFD; i++)
        if (!fdtab[i])
            break;
    if (i == NFD)
        return -1;

    f = fopen(path, mode);
    if (!f)
        return -1;
    fdtab[i] = f;

    return i;
}

/* The FILE the descriptor already stands for. tcc opens a file and then wants
 * a stream for it, which on a system with real descriptors means attaching
 * one; here the stream is what the descriptor was all along. */
FILE *fdopen(int fd, const char *mode)
{
    (void) mode;
    if (fd < 0 || fd >= NFD)
        return NULL;

    return fdtab[fd];
}

int close(int fd)
{
    if (fd < 0 || fd >= NFD || !fdtab[fd])
        return -1;
    fclose(fdtab[fd]);
    fdtab[fd] = NULL;

    return 0;
}

int write(int fd, const void *buf, unsigned int count)
{
    if (fd < 0 || fd >= NFD || !fdtab[fd])
        return -1;

    return (int) fwrite(buf, 1, count, fdtab[fd]);
}

int read(int fd, void *buf, unsigned int count)
{
    if (fd < 0 || fd >= NFD || !fdtab[fd])
        return -1;

    return (int) fread(buf, 1, count, fdtab[fd]);
}

long lseek(int fd, long offset, int whence)
{
    if (fd < 0 || fd >= NFD || !fdtab[fd])
        return -1;
    if (fseek(fdtab[fd], offset, whence) != 0)
        return -1;

    return ftell(fdtab[fd]);
}

/* ------------------------------------------------------------------ */
/* things the Agon has no equivalent of                                */

int unlink(const char *path)
{
    return remove(path);
}

/* MOS has a current directory but no getcwd. acc only uses it to make a
 * relative include path absolute, and a relative path works as it stands. */
char *getcwd(char *buf, unsigned int size)
{
    if (size < 2)
        return NULL;
    buf[0] = '.';
    buf[1] = '\0';

    return buf;
}

/* No symbolic links and no canonical form to resolve to. The only caller asks
 * whether two paths name the same file, and falls back to comparing the
 * strings when this returns NULL. */
char *realpath(const char *path, char *resolved)
{
    (void) path;
    (void) resolved;

    return NULL;
}

/* No environment. Every caller treats NULL as "not set". */
char *getenv(const char *name)
{
    (void) name;

    return NULL;
}

/* Only -bench asks the time, and it asks for a difference. */
struct timeval { long tv_sec; long tv_usec; };
int gettimeofday(struct timeval *tv, void *tz)
{
    (void) tz;
    if (tv)
        tv->tv_sec = 0, tv->tv_usec = 0;

    return 0;
}

/* tcc names this when it cannot open an output file. */
char *strerror(int errnum)
{
    (void) errnum;

    return "error";
}
