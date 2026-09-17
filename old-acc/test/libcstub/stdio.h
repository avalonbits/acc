#ifndef _S_STDIO_H
#define _S_STDIO_H
#include <stddef.h>
#include <stdarg.h>
typedef struct _FILE FILE;
extern FILE *stdin, *stdout, *stderr;
int printf(const char *, ...);
int fprintf(FILE *, const char *, ...);
int sprintf(char *, const char *, ...);
int snprintf(char *, size_t, const char *, ...);
int vprintf(const char *, va_list);
int vfprintf(FILE *, const char *, va_list);
int vsnprintf(char *, size_t, const char *, va_list);
int puts(const char *);
int putchar(int);
int getchar(void);
int scanf(const char *, ...);
int sscanf(const char *, const char *, ...);
FILE *fopen(const char *, const char *);
int fclose(FILE *);
size_t fread(void *, size_t, size_t, FILE *);
size_t fwrite(const void *, size_t, size_t, FILE *);
int fflush(FILE *);
int fputs(const char *, FILE *);
int fputc(int, FILE *);
int fgetc(FILE *);
char *fgets(char *, int, FILE *);
int feof(FILE *);
long ftell(FILE *);
int fseek(FILE *, long, int);
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define NULL ((void*)0)
#endif
