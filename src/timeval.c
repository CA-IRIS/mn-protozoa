/*
 * protozoa -- CCTV transcoder / mixer for PTZ
 * Copyright (C) 2008  Minnesota Department of Transportation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <stdlib.h>
#include "timeval.h"

/*
 * timeval_set_timeout	Set a timeval to current time plus a timeout (ms).
 */
void timeval_set_timeout(struct timeval *tv, unsigned int timeout) {
	gettimeofday(tv, NULL);
	tv->tv_usec += timeout * 1000;
	while(tv->tv_usec >= 1000000) {
		tv->tv_sec++;
		tv->tv_usec -= 1000000;
	}
}

int time_elapsed(const struct timeval *start, const struct timeval *end) {
	return (end->tv_sec - start->tv_sec) * 1000 +
		(end->tv_usec - start->tv_usec) / 1000;
}

int time_from_now(const struct timeval *tv) {
	struct timeval now;
	int ms;

	gettimeofday(&now, NULL);

	ms = time_elapsed(&now, tv);
	if(ms < 0)
		return 0;
	else
		return ms;
}
