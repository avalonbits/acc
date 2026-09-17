/* The part of <fcntl.h> acc uses on the Agon.
 *
 * MOS has no file descriptors; libagon has stdio. agon/agon.c implements
 * these four over FILE *. Only reading is needed: acc opens sources, objects
 * and archives read-only, and writes its output through fopen directly. */
#ifndef _ACC_FCNTL_H
#define _ACC_FCNTL_H

#define O_RDONLY  0
#define O_WRONLY  1
#define O_RDWR    2
#define O_CREAT   0x40
#define O_TRUNC   0x200
#define O_BINARY  0

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

int open(const char *path, int flags, ...);
int close(int fd);
int read(int fd, void *buf, unsigned int count);
int write(int fd, const void *buf, unsigned int count);
long lseek(int fd, long offset, int whence);

/* Named by paths acc does not take -- the ELF writer and the archive tools,
   neither of which the Agon build reaches -- but they still have to compile.
   <stdio.h> comes first so that FILE is agondev's own. */
#include <stdio.h>
FILE *fdopen(int fd, const char *mode);

#endif
