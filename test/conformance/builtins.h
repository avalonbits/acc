/* What the builtin spellings in sources.txt are mapped to, declared: gcc's
 * tests call __builtin_strcmp and its like without declaring them, which
 * gcc and clang allow of a builtin, and once the mapping has made that
 * strcmp, C99 needs strcmp declared. Given to both compilers with -include,
 * as part of the same measuring device as the mapping.
 *
 * Declared without their parameters, which C99 still allows (6.11.6): a
 * declaration that says nothing of them agrees with whatever prototype a
 * test gives the function itself, where the standard headers' prototypes
 * disagreed with some and brought typedefs -- FILE, div_t -- others define.
 *
 * Except the ones that take `...`, which `()` does not agree with (C99
 * 6.7.5.3p15): those have their prototypes, as a test that declares one
 * itself has to give it. */
void abort();
void exit();
void free();
void *malloc();
void *memcpy();
void *memmove();
void *memset();
int memcmp();
int strcmp();
__SIZE_TYPE__ strlen();
char *strcpy();
char *strncpy();
char *strcat();
char *strchr();
char *strrchr();
int printf(const char *, ...);
int sprintf(char *, const char *, ...);
int snprintf(char *, __SIZE_TYPE__, const char *, ...);
int abs();
long labs();
double fabs();
double copysign();
