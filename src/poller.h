#ifndef __POLLER_H__
#define __POLLER_H__

#include <sys/poll.h>		/* for struct pollfd */
#include "channel.h"		/* for struct channel */

struct poller {
	int		n_channels;
	struct channel	*chns;
	struct pollfd	*pollfds;
	int		fd_null;
};

struct poller *poller_init(struct poller *plr, int n_channels,
	struct channel *chns);
void poller_destroy(struct poller *plr);
int poller_loop(struct poller *plr);

#endif
