#include <fcntl.h>	/* for open */
#include <string.h>	/* for strerror */
#include <sys/errno.h>	/* for errno */
#include "poller.h"

#define POLL_MASK (POLLHUP | POLLERR | POLLIN)

struct poller *poller_init(struct poller *p, int n_channels,
	struct channel *chn)
{
	p->n_channels = n_channels;
	p->chn = chn;
	p->pollfds = malloc(sizeof(struct pollfd) * n_channels);
	if(p->pollfds == NULL)
		return NULL;
	// open an fd to poll for closed channels
	p->fd_null = open("/dev/null", O_RDONLY);
	return p;
}

static void poller_register_events(struct poller *p) {
	int i;
	struct channel *chn;

	for(i = 0; i < p->n_channels; i++) {
		chn = p->chn + i;
		if(!channel_is_open(chn)) {
			if(channel_is_waiting(chn) && channel_open(chn) < 0) {
				channel_debug(chn, strerror(errno));
				channel_close(chn);
			}
		}
		if(channel_is_open(chn)) {
			p->pollfds[i].fd = chn->fd;
			if(buffer_is_empty(chn->txbuf))
				p->pollfds[i].events = POLL_MASK;
			else
				p->pollfds[i].events = POLL_MASK | POLLOUT;
		} else {
			p->pollfds[i].fd = p->fd_null;
			p->pollfds[i].events = 0;
		}
	}
}

static int poller_do_poll(struct poller *p) {
	int i;
	ssize_t n_bytes;

	if(poll(p->pollfds, p->n_channels, -1) < 0)
		return -1;
	for(i = 0; i < p->n_channels; i++) {
		if(p->pollfds[i].revents & (POLLHUP | POLLERR)) {
			channel_close(p->chn + i);
			continue;
		}
		if(p->pollfds[i].revents & POLLOUT) {
			n_bytes = channel_write(p->chn + i);
			if(n_bytes < 0) {
				channel_debug(p->chn + i, strerror(errno));
				channel_close(p->chn + i);
				continue;
			}
		}
		if(p->pollfds[i].revents & POLLIN) {
			n_bytes = channel_read(p->chn + i);
			if(n_bytes < 0) {
				channel_debug(p->chn + i, strerror(errno));
				channel_close(p->chn + i);
				continue;
			}
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
