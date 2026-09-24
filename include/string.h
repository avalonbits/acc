/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef ACC_STRING_H
#define ACC_STRING_H

#include <stddef.h>

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

/* Not C89's, but every C library has it and a compiler that keeps the text
 * it has read wants it. */
char  *strdup(const char *s);

#endif
