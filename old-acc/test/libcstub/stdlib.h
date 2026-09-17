#ifndef _S_STDLIB_H
#define _S_STDLIB_H
#include <stddef.h>
void *malloc(size_t); void *calloc(size_t, size_t);
void *realloc(void *, size_t); void free(void *);
void exit(int); void abort(void);
int atoi(const char *); long atol(const char *); double atof(const char *);
long strtol(const char *, char **, int);
unsigned long strtoul(const char *, char **, int);
long long strtoll(const char *, char **, int);
unsigned long long strtoull(const char *, char **, int);
double strtod(const char *, char **);
int abs(int); long labs(long);
void qsort(void *, size_t, size_t, int (*)(const void *, const void *));
int rand(void); void srand(unsigned);
char *getenv(const char *);
int system(const char *);
#define NULL ((void*)0)
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 2147483647
#endif
