/*
 * Streams, over the calls MOS already has.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * MOS opens, reads, writes, closes and seeks; what C asks for on top of that
 * is a handle that remembers which way it is going, a buffer so that a
 * program reading or writing a byte at a time does not call MOS a byte at a
 * time, a character that can be pushed back, and the three standard streams.
 *
 * Each file has one buffer, which holds either what has been read ahead or
 * what is waiting to be written -- C asks a program to flush or seek before
 * turning a stream round, and this turns it round anyway: output held is
 * written first, and input read ahead is given back by seeking MOS to where
 * the program has actually got to.
 *
 * stdin is the keyboard, a line at a time through MOS's line editor, with
 * escape as the end of the input. stdout and stderr are the screen, and are
 * not buffered: MOS writes a character at a time and the screen is where a
 * person is waiting for it. freopen turns any of the three into a file.
 *
 * The table is fixed rather than allocated. A program has a few files open
 * at once and never many, and the heap on this machine is worth more to
 * whatever the program is actually doing.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <agon/mos.h>

/* How a stream was opened. */
#define F_READ    1
#define F_WRITE   2
#define F_APPEND  4             /* every write goes at the end */
#define F_CONSOLE 8             /* the keyboard or the screen, not a file */
#define F_TEMP    16            /* tmpfile's: removed when it is closed */

/* What the buffer holds. */
#define B_NONE    0
#define B_INPUT   1
#define B_OUTPUT  2

int acc_rt_putch(int c);

static char  bufs[FOPEN_MAX][BUFSIZ];
static FILE  files[FOPEN_MAX];
static char  temp_names[FOPEN_MAX][L_tmpnam];

/* The keyboard's line, and room for the newline after it. */
static char  line[256];

static FILE  console_in  = { 0, F_READ | F_CONSOLE, 0, 0, 0, _IOLBF, 0, EOF, EOF, 0, 0, 0, line };
static FILE  console_out = { 0, F_WRITE | F_CONSOLE, 0, 0, 0, _IONBF, 0, EOF, EOF, 0, 0, 0, 0 };
static FILE  console_err = { 0, F_WRITE | F_CONSOLE, 0, 0, 0, _IONBF, 0, EOF, EOF, 0, 0, 0, 0 };

FILE *stdin  = &console_in;
FILE *stdout = &console_out;
FILE *stderr = &console_err;

/* The end of the program closes every file: see atexit.c, and close_all
 * below. Named there, set here, so that neither takes the other. */
extern void (*__acc_close_files)(void);
void __acc_exit_arm(void);

/* How putchar, puts and printf reach stdout once freopen has made it a
 * file; they write to the screen themselves while it is not. See stdio.c. */
extern int (*__acc_stdout_putc)(int c);

static int is_file(FILE *f)
{
    return f && f->how && !(f->how & F_CONSOLE);
}

/* Where MOS is in the file, which is past what has been read ahead. */
static long mos_where(FILE *f)
{
    FIL *fil = mos_getfil(f->fh);

    return fil ? (long) fil->fptr : -1;
}

static int mos_seek(FILE *f, long to)
{
    return to >= 0 && mos_flseek(f->fh, (uint32_t) to) == 0 ? 0 : -1;
}

/* Bytes handed to MOS -- at the end of the file, for one opened to append
 * -- and the length of the file kept up with them.
 *
 * The length is kept here because MOS's own is not to be relied on while a
 * file is being written: the emulator's leaves it at what it was when the
 * file was opened, and says 0 for one just made, so a seek from the end of
 * a file the program has written went somewhere else. */
static int write_out(FILE *f, const char *p, int n)
{
    long at;

    if ((f->how & F_APPEND) && mos_seek(f, f->end) != 0) {
        f->error = 1;

        return EOF;
    }
    if ((int) mos_fwrite(f->fh, (char *) p, (uint24_t) n) != n) {
        f->error = 1;

        return EOF;
    }
    at = mos_where(f);
    if (at > f->end)
        f->end = at;

    return 0;
}

/* Everything held for output, handed over. */
static int flush_output(FILE *f)
{
    int n = f->held;

    f->held = 0;
    f->last = B_NONE;
    if (!n)
        return 0;

    return write_out(f, f->buf, n);
}

/* Input read ahead and not used, given back: MOS is put where the program
 * has got to, and the character pushed back is forgotten, as C says a seek
 * forgets it. */
static int drop_input(FILE *f)
{
    long ahead = f->held - f->at + (f->unget != EOF);

    f->held = f->at = 0;
    f->unget = EOF;
    f->wunget = EOF;
    f->last = B_NONE;
    if (!ahead || !is_file(f))
        return 0;

    return mos_seek(f, mos_where(f) - ahead);
}

int fflush(FILE *f)
{
    int i, bad = 0;

    if (!f) {
        for (i = 0; i < FOPEN_MAX; i++)
            if (files[i].how && fflush(&files[i]) != 0)
                bad = 1;

        return bad ? EOF : 0;
    }
    if (!is_file(f))
        return 0;
    if (f->last == B_INPUT)
        return drop_input(f) == 0 ? 0 : EOF;

    return flush_output(f);
}

/* The flags a mode string asks for, or 0 for one C does not have: r, w or
 * a, then + and b in either order. */
static int mode_of(const char *mode, uint8_t *mos)
{
    int how;

    switch (*mode++) {
    case 'r': how = F_READ;              *mos = FA_READ | FA_OPEN_EXISTING;  break;
    case 'w': how = F_WRITE;             *mos = FA_WRITE | FA_CREATE_ALWAYS; break;
    case 'a': how = F_WRITE | F_APPEND;  *mos = FA_WRITE | FA_OPEN_APPEND;   break;
    default:  return 0;
    }
    for (; *mode; mode++) {
        if (*mode == '+') {
            how |= F_READ | F_WRITE;
            *mos |= FA_READ | FA_WRITE;
        } else if (*mode != 'b') {
            return 0;
        }
    }

    return how;
}

static void close_all(void)
{
    int i;

    for (i = 0; i < FOPEN_MAX; i++)
        if (files[i].how)
            fclose(&files[i]);
    if (is_file(stdout))
        fclose(stdout);
    if (is_file(stderr))
        fclose(stderr);
}

/* `path` opened into `f`, which is free. */
static FILE *open_into(FILE *f, const char *path, const char *mode, char *buf)
{
    uint8_t flags, fh;
    int how = mode_of(mode, &flags);

    if (!how)
        return NULL;
    fh = mos_fopen(path, flags);
    if (!fh)
        return NULL;

    f->fh = fh;
    f->how = (unsigned char) how;
    f->last = B_NONE;
    f->error = 0;
    f->eof = 0;
    f->vbuf = _IOFBF;
    f->orient = 0;
    f->unget = EOF;
    f->wunget = EOF;
    f->held = 0;
    f->at = 0;
    f->buf = buf;
    {
        FIL *fil = mos_getfil(fh);

        f->end = fil ? (long) fil->obj.objsize : 0;
    }

    __acc_close_files = close_all;
    __acc_exit_arm();

    return f;
}

static int free_slot(void)
{
    int i;

    for (i = 0; i < FOPEN_MAX; i++)
        if (!files[i].how)
            return i;

    return -1;
}

FILE *fopen(const char *path, const char *mode)
{
    int i = free_slot();

    if (i < 0)
        return NULL;

    return open_into(&files[i], path, mode, bufs[i]);
}

int fclose(FILE *f)
{
    int bad, i;

    if (!is_file(f))
        return f && f->how ? 0 : EOF;
    bad = fflush(f) != 0;
    mos_fclose(f->fh);          /* which answers how many are still open */
    f->how = 0;                 /* the slot is free again */
    i = (int) (f - files);
    if (i >= 0 && i < FOPEN_MAX && temp_names[i][0]) {
        remove(temp_names[i]);
        temp_names[i][0] = 0;
    }

    return bad ? EOF : 0;
}

static int stdout_putc(int c)
{
    return fputc(c, stdout);
}

/* The same FILE, onto another file. The standard streams take a buffer of
 * their own the first time, since they are not in the table. */
FILE *freopen(const char *path, const char *mode, FILE *f)
{
    static char std_bufs[3][BUFSIZ];
    char *buf;

    if (!f || !path)
        return NULL;
    if (is_file(f))
        fclose(f);
    if (f == stdin)
        buf = std_bufs[0];
    else if (f == stdout)
        buf = std_bufs[1];
    else if (f == stderr)
        buf = std_bufs[2];
    else
        buf = bufs[f - files];
    f->how = 0;
    if (f == stdout)
        __acc_stdout_putc = NULL;
    if (!open_into(f, path, mode, buf))
        return NULL;
    if (f == stdout)
        __acc_stdout_putc = stdout_putc;

    return f;
}

int setvbuf(FILE *f, char *buf, int mode, size_t size)
{
    (void) buf;                 /* each stream has its own already */
    (void) size;
    if (!f || mode < _IOFBF || mode > _IONBF)
        return -1;
    f->vbuf = (unsigned char) mode;

    return 0;
}

void setbuf(FILE *f, char *buf)
{
    setvbuf(f, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
}

/* More input into the buffer: a line from the keyboard, or as much of the
 * file as fits. 0 at the end of the input. */
static int fill(FILE *f)
{
    int n;

    if (f->last == B_OUTPUT && flush_output(f) != 0)
        return 0;
    f->held = f->at = 0;
    f->last = B_INPUT;
    if (f->eof)
        return 0;

    if (f->how & F_CONSOLE) {
        line[0] = 0;
        if (mos_editline(line, sizeof line - 1, 1) == 27) {
            acc_rt_putch('\r');
            acc_rt_putch('\n');
            f->eof = 1;

            return 0;
        }
        acc_rt_putch('\r');
        acc_rt_putch('\n');
        n = (int) strlen(line);
        line[n++] = '\n';
    } else {
        n = (int) mos_fread(f->fh, f->buf, BUFSIZ);
    }
    if (!n)
        f->eof = 1;
    f->held = n;

    return n;
}

int fgetc(FILE *f)
{
    int c;

    if (f && !f->orient)
        f->orient = -1;         /* the first byte read or written says so */
    if (!f || !(f->how & F_READ)) {
        if (f)
            f->error = 1;

        return EOF;
    }
    if (f->unget != EOF) {
        c = f->unget;
        f->unget = EOF;

        return c;
    }
    if ((f->last != B_INPUT || f->at == f->held) && !fill(f))
        return EOF;

    return (unsigned char) f->buf[f->at++];
}

int getc(FILE *f)
{
    return fgetc(f);
}

int getchar(void)
{
    return fgetc(stdin);
}

int ungetc(int c, FILE *f)
{
    if (!f || c == EOF || f->unget != EOF)
        return EOF;
    f->unget = (unsigned char) c;
    f->eof = 0;

    return (unsigned char) c;
}

char *fgets(char *s, int n, FILE *f)
{
    char *p = s;
    int c = 0;

    if (n <= 0)
        return NULL;
    while (--n > 0 && (c = fgetc(f)) != EOF) {
        *p++ = (char) c;
        if (c == '\n')
            break;
    }
    if (p == s || (c == EOF && f->error))
        return NULL;
    *p = 0;

    return s;
}

/* C99 has it, and says nothing stops it writing past the end of `s`. */
char *gets(char *s)
{
    char *p = s;
    int c;

    while ((c = getchar()) != EOF && c != '\n')
        *p++ = (char) c;
    if (c == EOF && (p == s || stdin->error))
        return NULL;
    *p = 0;

    return s;
}

size_t fread(void *to, size_t size, size_t count, FILE *f)
{
    char *p = to;
    size_t want = size * count, got = 0;
    int n;

    if (!want)
        return 0;
    if (f && !f->orient)
        f->orient = -1;
    if (!f || !(f->how & F_READ)) {
        if (f)
            f->error = 1;

        return 0;
    }
    if (f->unget != EOF) {
        *p++ = (char) f->unget;
        f->unget = EOF;
        got++;
    }
    while (got < want) {
        if (f->last == B_INPUT && f->at < f->held) {
            n = f->held - f->at;
            if ((size_t) n > want - got)
                n = (int) (want - got);
            memcpy(p, f->buf + f->at, (size_t) n);
            f->at += n;
        } else if (is_file(f) && want - got >= (size_t) BUFSIZ) {
            /* A big read has nothing to gain by being copied through the
             * buffer. */
            if (f->last == B_OUTPUT && flush_output(f) != 0)
                break;
            f->last = B_NONE;
            n = (int) mos_fread(f->fh, p, (uint24_t) (want - got));
            if (!n) {
                f->eof = 1;
                break;
            }
        } else if (!fill(f)) {
            break;
        } else {
            continue;
        }
        p += n;
        got += (size_t) n;
    }

    return got / size;
}

int fputc(int c, FILE *f)
{
    if (f && !f->orient)
        f->orient = -1;
    if (!f || !(f->how & F_WRITE)) {
        if (f)
            f->error = 1;

        return EOF;
    }
    if (f->how & F_CONSOLE)
        return acc_rt_putch((unsigned char) c);
    if (f->last == B_INPUT && drop_input(f) != 0) {
        f->error = 1;

        return EOF;
    }
    if (f->held == BUFSIZ && flush_output(f) != 0)
        return EOF;
    f->last = B_OUTPUT;
    f->buf[f->held++] = (char) c;
    if (f->vbuf == _IONBF || (f->vbuf == _IOLBF && c == '\n'))
        if (flush_output(f) != 0)
            return EOF;

    return (unsigned char) c;
}

int putc(int c, FILE *f)
{
    return fputc(c, f);
}

size_t fwrite(const void *from, size_t size, size_t count, FILE *f)
{
    const char *p = from;
    size_t want = size * count, left = want;

    if (!want)
        return 0;
    if (f && !f->orient)
        f->orient = -1;
    if (!f || !(f->how & F_WRITE)) {
        if (f)
            f->error = 1;

        return 0;
    }
    if ((f->how & F_CONSOLE) || f->vbuf != _IOFBF) {
        while (left) {
            if (fputc((unsigned char) *p++, f) == EOF)
                return (want - left) / size;
            left--;
        }

        return count;
    }
    if (f->last == B_INPUT && drop_input(f) != 0) {
        f->error = 1;

        return 0;
    }

    /* Anything that would not leave room goes straight out: a big write has
     * nothing to gain by being copied into a buffer first. */
    if (left >= (size_t) BUFSIZ) {
        if (flush_output(f) != 0 || write_out(f, p, (int) left) != 0)
            return 0;

        return count;
    }

    if (f->held + (int) left > BUFSIZ && flush_output(f) != 0)
        return 0;
    memcpy(f->buf + f->held, p, left);
    f->held += (int) left;
    f->last = B_OUTPUT;

    return count;
}

int fputs(const char *s, FILE *f)
{
    size_t n = strlen(s);

    return fwrite(s, 1, n, f) == n ? 0 : EOF;
}

/* Where the program has got to: MOS's place, less what has been read ahead
 * and not used, plus what is held to be written. */
long ftell(FILE *f)
{
    long at;

    if (!is_file(f))
        return -1;
    at = mos_where(f);
    if (at < 0)
        return -1;
    if (f->last == B_INPUT)
        at -= f->held - f->at;
    else if (f->last == B_OUTPUT)
        at += f->held;
    if (f->unget != EOF)
        at--;

    return at;
}

int fseek(FILE *f, long offset, int whence)
{
    long to;

    if (!is_file(f))
        return -1;
    if (whence == SEEK_CUR) {
        offset += ftell(f);
        whence = SEEK_SET;
    }
    if (fflush(f) != 0)
        return -1;
    f->unget = EOF;
    f->wunget = EOF;
    f->held = f->at = 0;
    f->last = B_NONE;

    /* MOS seeks to a place and not by an amount, so the end is worked out
     * from the length kept in write_out. */
    if (whence == SEEK_SET)
        to = offset;
    else if (whence == SEEK_END)
        to = f->end + offset;
    else
        return -1;
    if (mos_seek(f, to) != 0)
        return -1;
    f->eof = 0;

    return 0;
}

void rewind(FILE *f)
{
    fseek(f, 0L, SEEK_SET);
    clearerr(f);
}

int fgetpos(FILE *f, fpos_t *pos)
{
    long at = ftell(f);

    if (at < 0)
        return -1;
    *pos = at;

    return 0;
}

int fsetpos(FILE *f, const fpos_t *pos)
{
    return fseek(f, *pos, SEEK_SET);
}

void clearerr(FILE *f)
{
    if (f) {
        f->error = 0;
        f->eof = 0;
    }
}

int feof(FILE *f)
{
    return f ? f->eof : 0;
}

int ferror(FILE *f)
{
    return f ? f->error : 1;
}

int remove(const char *path)
{
    return mos_del(path) == 0 ? 0 : -1;
}

int rename(const char *from, const char *to)
{
    return mos_ren(from, to) == 0 ? 0 : -1;
}

/* A name no file on the card has: acctmpNNN.tmp, the first free one of
 * TMP_MAX, going on from where the last call stopped. */
char *tmpnam(char *s)
{
    static char name[L_tmpnam];
    static int next;
    FILINFO info;
    int tries;

    if (!s)
        s = name;
    for (tries = 0; tries < TMP_MAX; tries++) {
        int n = next++ % TMP_MAX;

        memcpy(s, "acctmp000.tmp", 14);
        s[6] = (char) ('0' + n / 100);
        s[7] = (char) ('0' + n / 10 % 10);
        s[8] = (char) ('0' + n % 10);
        if (ffs_stat(&info, s) != 0)
            return s;           /* not there: it is ours */
    }

    return NULL;
}

FILE *tmpfile(void)
{
    int i = free_slot();
    FILE *f;

    if (i < 0 || !tmpnam(temp_names[i]))
        return NULL;
    f = open_into(&files[i], temp_names[i], "w+", bufs[i]);
    if (!f)
        temp_names[i][0] = 0;

    return f;
}

/* Printing to a file, which is the formatter in lib/printf.c with this file
 * as where the characters go. */
extern FILE  *acc_sink_file;
extern int  (*acc_sink_putc)(int, FILE *);
extern char  *acc_sink_buf;
int acc_format(const char *fmt, va_list ap);

int vfprintf(FILE *f, const char *fmt, va_list ap)
{
    int n;

    acc_sink_buf = NULL;
    acc_sink_file = f;
    acc_sink_putc = fputc;
    n = acc_format(fmt, ap);
    acc_sink_file = NULL;

    return n;
}

int fprintf(FILE *f, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vfprintf(f, fmt, ap);
    va_end(ap);

    return n;
}
