#ifndef __TIMEVAL_H__
#define __TIMEVAL_H__

#include <sys/time.h>

void timeval_set_timeout(struct timeval *tv, unsigned int timeout);
int time_elapsed(const struct timeval *start, const struct timeval *end);
int time_from_now(const struct timeval *tv);

#endif
