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

static void poller_register_events(struct poller *plr) {
	int i;
	struct channel *chn;

	for(i = 0; i < plr->n_channels; i++) {
		chn = plr->chns + i;
		if(!channel_is_open(chn)) {
			if(channel_is_waiting(chn) && channel_open(chn) < 0) {
				channel_log(chn, strerror(errno));
				channel_close(chn);
			}
		}
		if(channel_is_open(chn)) {
			plr->pollfds[i].fd = chn->fd;
			plr->pollfds[i].events = POLLHUP | POLLERR;
			if(channel_has_reader(chn))
				plr->pollfds[i].events |= POLLIN;
			if(!buffer_is_empty(chn->txbuf))
				plr->pollfds[i].events |= POLLOUT;
		} else {
			plr->pollfds[i].fd = plr->fd_null;
			plr->pollfds[i].events = 0;
		}
	}
}

static int poller_do_poll(struct poller *plr) {
	int i;
	ssize_t n_bytes;
	struct channel *chn;

	if(poll(plr->pollfds, plr->n_channels, -1) < 0)
		return -1;
	for(i = 0; i < plr->n_channels; i++) {
		chn = plr->chns + i;
		if(plr->pollfds[i].revents & (POLLHUP | POLLERR)) {
			channel_close(chn);
			continue;
		}
		if(plr->pollfds[i].revents & POLLOUT) {
			n_bytes = channel_write(chn);
			if(n_bytes < 0) {
				channel_log(chn, strerror(errno));
				channel_close(chn);
				continue;
			}
		}
		if(plr->pollfds[i].revents & POLLIN) {
			n_bytes = channel_read(chn);
			if(n_bytes < 0) {
				channel_log(chn, strerror(errno));
				channel_close(chn);
				continue;
			}
		}
	}
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
