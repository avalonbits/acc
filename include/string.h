/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#ifndef ACC_STRING_H
#define ACC_STRING_H

/* The names of <stddef.h> this header is given (C99 7.1.3), and no others:
 * a program that includes it may define ptrdiff_t or offsetof itself. */
#ifndef ACC_SIZE_T
#define ACC_SIZE_T
typedef unsigned int size_t;
#endif
#ifndef NULL
#define NULL ((void *) 0)
#endif

void  *memchr(const void *s, int c, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
void  *memcpy(void *to, const void *from, size_t n);
void  *memmove(void *to, const void *from, size_t n);
void  *memset(void *s, int c, size_t n);

char  *strcat(char *to, const char *from);
char  *strchr(const char *s, int c);
int    strcmp(const char *a, const char *b);
char  *strcpy(char *to, const char *from);
size_t strlen(const char *s);
char  *strncat(char *to, const char *from, size_t n);
int    strncmp(const char *a, const char *b, size_t n);
char  *strncpy(char *to, const char *from, size_t n);
char  *strrchr(const char *s, int c);
char  *strstr(const char *hay, const char *needle);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
char  *strpbrk(const char *s, const char *accept);
char  *strtok(char *s, const char *delim);
int    strcoll(const char *a, const char *b);
size_t strxfrm(char *to, const char *from, size_t n);

char  *strerror(int n);

/* Not C99's, but every C library has them, and so has agondev's, which
 * programs written for the Agon were built with: POSIX's strdup, strndup,
 * strnlen, stpcpy, stpncpy, strcasecmp and strncasecmp, and GNU's
 * strchrnul. C99 7.26.11 keeps names that begin `str` and a lowercase
 * letter for this header to add; stpcpy and stpncpy are POSIX's.
 *   strnlen: the length, at most maxlen; s[maxlen] and past are not read.
 *   stpcpy: strcpy, answering where the terminator it wrote is.
 *   stpncpy: strncpy, answering to + strnlen(from, n).
 *   strcasecmp: strncasecmp with no limit. */
char  *strdup(const char *s);
char  *strndup(const char *s, size_t n);
size_t strnlen(const char *s, size_t maxlen);
char  *stpcpy(char *to, const char *from);
char  *stpncpy(char *to, const char *from, size_t n);
int    strcasecmp(const char *a, const char *b);
int    strncasecmp(const char *a, const char *b, size_t n);
char  *strchrnul(const char *s, int c);

#endif
