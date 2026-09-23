/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef ACC_STDIO_H
#define ACC_STDIO_H

#include <stdarg.h>
#include <stddef.h>

#define EOF (-1)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* How many files may be open at once. A compiler has its source, the header
 * it is reading, and what it is writing; a linker has an object and its
 * output. Eight is more than either wants and the whole table is 80 bytes,
 * which is cheaper than asking the heap for one at a time. */
#define FOPEN_MAX 8

/* The buffer a write goes through. MOS writes to the card, so a write of
 * three bytes -- which is what an object's every symbol and every relocation
 * comes to -- would otherwise be a card write each. */
#define BUFSIZ 512

/* An open file. What is in it is the library's business, but the size has to
 * be here for the table to be laid out without the heap. */
typedef struct {
    unsigned char  fh;          /* what MOS calls it; 0 when this is free */
    unsigned char  screen;      /* writes go to the screen, not to a file */
    unsigned char  writing;
    unsigned char  error;
    int            held;        /* bytes waiting in buf */
    char           buf[BUFSIZ];
} FILE;

extern FILE *stdout;
extern FILE *stderr;

FILE  *fopen(const char *path, const char *mode);
int    fclose(FILE *f);
size_t fread(void *to, size_t size, size_t count, FILE *f);
size_t fwrite(const void *from, size_t size, size_t count, FILE *f);
int    fseek(FILE *f, long offset, int whence);
long   ftell(FILE *f);
int    fflush(FILE *f);
int    fputc(int c, FILE *f);
int    fputs(const char *s, FILE *f);
int    fgetc(FILE *f);
int    ferror(FILE *f);

int putchar(int c);
int puts(const char *s);
int printf(const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int sprintf(char *to, const char *fmt, ...);
int vsprintf(char *to, const char *fmt, va_list ap);
int fprintf(FILE *f, const char *fmt, ...);
int vfprintf(FILE *f, const char *fmt, va_list ap);
int snprintf(char *to, size_t n, const char *fmt, ...);
int vsnprintf(char *to, size_t n, const char *fmt, va_list ap);

#endif
