/* One thread, so a semaphore is a no-op. CONFIG_TCC_SEMLOCK is 0 and this
   should not be reached; it exists so the include in tcc.h resolves. */
#ifndef _ACC_SEMAPHORE_H
#define _ACC_SEMAPHORE_H
typedef int sem_t;
static inline int sem_init(sem_t *s, int p, unsigned v) { (void)p; *s = (int)v; return 0; }
static inline int sem_wait(sem_t *s) { (void)s; return 0; }
static inline int sem_post(sem_t *s) { (void)s; return 0; }
#endif
