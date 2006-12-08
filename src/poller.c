#include "poller.h"

#define POLL_MASK (POLLHUP | POLLERR | POLLIN)

struct poller *poller_init(struct poller *p, int n_channels,
	struct channel *chn)
{
	int i;

	p->n_channels = n_channels;
	p->chn = chn;
	p->pollfds = malloc(sizeof(struct pollfd) * n_channels);
	if(p->pollfds == NULL)
		return NULL;
	for(i = 0; i < n_channels; i++)
		p->pollfds[i].fd = chn[i].fd;
	return p;
}

static void poller_register_events(struct poller *p) {
	int i;

	for(i = 0; i < p->n_channels; i++) {
		if(buffer_is_empty(p->chn[i].txbuf))
			p->pollfds[i].events = POLL_MASK;
		else
			p->pollfds[i].events = POLL_MASK | POLLOUT;
	}
}

static int poller_do_poll(struct poller *p) {
	int i;
	ssize_t n_bytes;

	if(poll(p->pollfds, p->n_channels, -1) < 0)
		return -1;
	for(i = 0; i < p->n_channels; i++) {
		if(p->pollfds[i].revents & (POLLHUP | POLLERR)) {
			channel_reopen(p->chn + i);
			continue;
		}
		if(p->pollfds[i].revents & POLLOUT) {
			n_bytes = channel_write(p->chn + i);
			if(n_bytes < 0)
				return n_bytes;
		}
		if(p->pollfds[i].revents & POLLIN) {
			n_bytes = channel_read(p->chn + i);
			if(n_bytes < 0)
				return n_bytes;
		}
	}
	return 0;
}

int poller_loop(struct poller *p) {
	int r = 0;
	do {
		poller_register_events(p);
		r = poller_do_poll(p);
	} while(r >= 0);
	return r;
}
