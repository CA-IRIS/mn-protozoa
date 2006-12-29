#ifndef __POLLER_H__
#define __POLLER_H__

#include <sys/poll.h>
#include "channel.h"

struct poller {
	int		n_channels;
	struct channel	*chns;
	struct pollfd	*pollfds;
	int		fd_null;
};

struct poller *poller_init(struct poller *plr, int n_channels,
	struct channel *chns);
int poller_loop(struct poller *plr);

#endif
