/* No wall clock is needed: the only caller is tcc's -bench timing. */
#ifndef _ACC_SYS_TIME_H
#define _ACC_SYS_TIME_H
struct timeval { long tv_sec; long tv_usec; };
int gettimeofday(struct timeval *tv, void *tz);
#endif
