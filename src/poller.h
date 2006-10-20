#ifndef __POLLER_H__
#define __POLLER_H__

#include <sys/poll.h>
#include "channel.h"

struct poller {
	int		n_channels;
	struct channel	*chn;
	struct pollfd	*pollfds;
};

struct poller *poller_init(struct poller *p, int n_channels,
	struct channel *chn);
int poller_loop(struct poller *p);

#endif
