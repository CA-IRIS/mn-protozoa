#include <fcntl.h>	/* for open */
#include <string.h>	/* for strerror */
#include <sys/errno.h>	/* for errno */
#include "poller.h"

struct poller *poller_init(struct poller *plr, int n_channels,
	struct channel *chns)
{
	plr->n_channels = n_channels;
	plr->chns = chns;
	plr->pollfds = malloc(sizeof(struct pollfd) * n_channels);
	if(plr->pollfds == NULL)
		return NULL;
	/* open an fd to poll for closed channels */
	plr->fd_null = open("/dev/null", O_RDONLY);
	return plr;
}

static inline void poller_register_channel(struct poller *plr,
	struct channel *chn, struct pollfd *pfd)
{
	if(!channel_is_open(chn)) {
		if(channel_is_waiting(chn) && channel_open(chn) < 0) {
			channel_log(chn, strerror(errno));
			channel_close(chn);
		}
	}
	if(channel_is_open(chn)) {
		pfd->fd = chn->fd;
		pfd->events = POLLHUP | POLLERR;
		if(channel_has_reader(chn))
			pfd->events |= POLLIN;
		if(!buffer_is_empty(chn->txbuf))
			pfd->events |= POLLOUT;
	} else {
		pfd->fd = plr->fd_null;
		pfd->events = 0;
	}
}

static void poller_register_events(struct poller *plr) {
	int i;

	for(i = 0; i < plr->n_channels; i++)
		poller_register_channel(plr, plr->chns + i, plr->pollfds + i);
}

static inline void poller_channel_events(struct poller *plr,
	struct channel *chn, struct pollfd *pfd)
{
	if(pfd->revents & (POLLHUP | POLLERR)) {
		channel_close(chn);
		return;
	}
	if(pfd->revents & POLLOUT) {
		ssize_t n_bytes = channel_write(chn);
		if(n_bytes < 0) {
			channel_log(chn, strerror(errno));
			channel_close(chn);
			return;
		}
	}
	if(pfd->revents & POLLIN) {
		ssize_t n_bytes = channel_read(chn);
		if(n_bytes < 0) {
			channel_log(chn, strerror(errno));
			channel_close(chn);
			return;
		}
	}
}

static int poller_do_poll(struct poller *plr) {
	int i;

	if(poll(plr->pollfds, plr->n_channels, -1) < 0)
		return -1;
	for(i = 0; i < plr->n_channels; i++)
		poller_channel_events(plr, plr->chns + i, plr->pollfds + i);
	return 0;
}

int poller_loop(struct poller *plr) {
	int r = 0;
	do {
		poller_register_events(plr);
		r = poller_do_poll(plr);
	} while(r >= 0);
	return r;
}
