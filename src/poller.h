#ifndef __POLLER_H__
#define __POLLER_H__

#include <sys/poll.h>
#include "sport.h"

struct poller {
	int		n_ports;
	struct sport	*port;
	struct pollfd	*pollfds;
};

struct poller *poller_init(struct poller *p, int n_ports, struct sport *port);
int poller_loop(struct poller *p);

#endif
