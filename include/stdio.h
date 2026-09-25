/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef ACC_STDIO_H
#define ACC_STDIO_H

#include <stdarg.h>
/* The names of <stddef.h> this header is given (C99 7.1.3), and no others:
 * a program that includes it may define ptrdiff_t or offsetof itself. */
#ifndef ACC_SIZE_T
#define ACC_SIZE_T
typedef unsigned int size_t;
#endif
#ifndef NULL
#define NULL ((void *) 0)
#endif

#define EOF (-1)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* How many files may be open at once, besides the three standard streams.
 * A compiler has its source, the header it is reading, and what it is
 * writing; a linker has an object and its output. Eight is more than
 * either wants, and the table is fixed rather than taken from the heap. */
#define FOPEN_MAX 8

/* The buffer each open file reads and writes through. MOS writes to the
 * card, so a write of three bytes would otherwise be a card write each,
 * and a read of one a call into MOS each. */
#define BUFSIZ 512

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

#define FILENAME_MAX 256
#define L_tmpnam     16
#define TMP_MAX      1000

typedef long fpos_t;

/* An open stream. What is in it is the library's business; it is here so
 * that the table can be laid out without the heap. Tagged, so that
 * <wchar.h> can name it without declaring the rest of this header. */
typedef struct __acc_file {
    unsigned char  fh;          /* what MOS calls it, for a file */
    unsigned char  how;         /* how it was opened: see file.c */
    unsigned char  last;        /* whether buf holds input or output */
    unsigned char  error;
    unsigned char  eof;
    unsigned char  vbuf;        /* _IOFBF, _IOLBF or _IONBF */
    signed char    orient;      /* wide above 0, bytes below, not yet 0 */
    int            unget;       /* a character pushed back, or EOF */
    int            wunget;      /* a wide one, or WEOF */
    int            held;        /* bytes in buf */
    int            at;          /* and how many of them input has used */
    long           end;         /* how long the file is */
    char          *buf;
} FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int    remove(const char *path);
int    rename(const char *from, const char *to);
FILE  *tmpfile(void);
char  *tmpnam(char *s);

FILE  *fopen(const char *path, const char *mode);
FILE  *freopen(const char *path, const char *mode, FILE *f);
int    fclose(FILE *f);
int    fflush(FILE *f);
void   setbuf(FILE *f, char *buf);
int    setvbuf(FILE *f, char *buf, int mode, size_t size);

size_t fread(void *to, size_t size, size_t count, FILE *f);
size_t fwrite(const void *from, size_t size, size_t count, FILE *f);

int    fgetc(FILE *f);
char  *fgets(char *s, int n, FILE *f);
int    fputc(int c, FILE *f);
int    fputs(const char *s, FILE *f);
int    getc(FILE *f);
int    getchar(void);
char  *gets(char *s);
int    putc(int c, FILE *f);
int    putchar(int c);
int    puts(const char *s);
int    ungetc(int c, FILE *f);

int    fgetpos(FILE *f, fpos_t *pos);
int    fseek(FILE *f, long offset, int whence);
int    fsetpos(FILE *f, const fpos_t *pos);
long   ftell(FILE *f);
void   rewind(FILE *f);

void   clearerr(FILE *f);
int    feof(FILE *f);
int    ferror(FILE *f);
void   perror(const char *s);

int printf(const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int sprintf(char *to, const char *fmt, ...);
int vsprintf(char *to, const char *fmt, va_list ap);
int fprintf(FILE *f, const char *fmt, ...);
int vfprintf(FILE *f, const char *fmt, va_list ap);
int snprintf(char *to, size_t n, const char *fmt, ...);
int vsnprintf(char *to, size_t n, const char *fmt, va_list ap);

int scanf(const char *fmt, ...);
int vscanf(const char *fmt, va_list ap);
int sscanf(const char *s, const char *fmt, ...);
int vsscanf(const char *s, const char *fmt, va_list ap);
int fscanf(FILE *f, const char *fmt, ...);
int vfscanf(FILE *f, const char *fmt, va_list ap);

#endif
