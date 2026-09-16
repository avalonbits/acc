#ifndef _ACC_UNISTD_H
#define _ACC_UNISTD_H
#include <fcntl.h>
typedef int ssize_t;
char *getcwd(char *buf, unsigned int size);
int unlink(const char *path);

/* Declared here because tcc.h includes <unistd.h> and agondev's own headers
   do not have them. All three are stubs in agon/src/agon.c: the Agon has no
   environment, no symbolic links and no canonical path to resolve to. */
char *realpath(const char *path, char *resolved);
char *getenv(const char *name);
char *strerror(int errnum);
#endif
