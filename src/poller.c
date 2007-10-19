/*
 * protozoa -- CCTV transcoder / mixer for PTZ
 * Copyright (C) 2006-2007  Minnesota Department of Transportation
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
#include <fcntl.h>	/* for open */
#include <string.h>	/* for memset, strerror */
#include <sys/errno.h>	/* for errno */
#include <unistd.h>	/* for close */
#include "poller.h"	/* for struct poller, prototypes */

/*
 * poller_init		Initialize a new I/O channel poller.
 *
 * n_channels: number of channels to poll
 * chns: linked list of channels (poller takes ownership of memory)
 * return: pointer to struct poller or NULL on error
 */
struct poller *poller_init(struct poller *plr, int n_channels,
	struct channel *chns)
{
	memset(plr, 0, sizeof(struct poller));
	plr->n_channels = n_channels;
	plr->chns = chns;
	plr->pollfds = malloc(sizeof(struct pollfd) * n_channels);
	if(plr->pollfds == NULL)
		return NULL;
	/* open an fd to poll for closed channels */
	plr->fd_null = open("/dev/null", O_RDONLY);
	return plr;
}

/*
 * poller_destroy	Destroy a previously initialized poller.
 */
void poller_destroy(struct poller *plr) {
	struct channel *chn = plr->chns;
	close(plr->fd_null);
	free(plr->pollfds);
	while(chn) {
		struct channel *nchn = chn->next;
		channel_destroy(chn);
		free(chn);
		chn = nchn;
	}
	memset(plr, 0, sizeof(struct poller));
}

/*
 * poller_register_channel	Register events to poll for one channel.
 *
 * chn: channel to register events for
 * pfd: poll fd structure
 */
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
		if(channel_needs_reading(chn))
			pfd->events |= POLLIN;
		if(channel_needs_writing(chn))
			pfd->events |= POLLOUT;
	} else {
		pfd->fd = plr->fd_null;
		pfd->events = 0;
	}
}

/*
 * poller_register_events	Register events for all channels to poll.
 */
static void poller_register_events(struct poller *plr) {
	int i;
	struct channel *chn = plr->chns;

	for(i = 0; i < plr->n_channels; i++, chn = chn->next)
		poller_register_channel(plr, chn, plr->pollfds + i);
}

static void debug_log(struct channel *chn, const char *msg) {
	log_println(chn->log, "debug: %s %s:%d", msg, chn->name, chn->extra);
}

/*
 * debug_poll_events		Debug the channel poll events.
 *
 * pfd: pollfd struct
 */
static void debug_poll_events(struct channel *chn, const struct pollfd *pfd) {
	if(pfd->revents & POLLHUP)
		debug_log(chn, "POLLHUP");
	if(pfd->revents & POLLERR)
		debug_log(chn, "POLLERR");
	if(pfd->revents & POLLIN)
		debug_log(chn, "POLLIN");
	if(pfd->revents & POLLOUT)
		debug_log(chn, "POLLOUT");
}

/*
 * poller_channel_events	Process polled events for one channel.
 *
 * chn: channel to process
 * pfd: poll fd structure
 */
static inline void poller_channel_events(struct poller *plr,
	struct channel *chn, struct pollfd *pfd)
{
	if(chn->log->debug)
		debug_poll_events(chn, pfd);
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
		if(n_bytes < 0)
			channel_log(chn, strerror(errno));
		if(n_bytes <= 0) {
			channel_close(chn);
			return;
		}
	}
}

/*
 * poller_do_poll	Poll all channels for new events.
 */
static int poller_do_poll(struct poller *plr) {
	int i;
	struct channel *chn = plr->chns;

	if(poll(plr->pollfds, plr->n_channels, -1) < 0)
		return -1;
	for(i = 0; i < plr->n_channels; i++, chn = chn->next)
		poller_channel_events(plr, chn, plr->pollfds + i);
	return 0;
}

/*
 * poller_loop		Poll all channels for events in a continuous loop.
 *
 * return: -1 on error, otherwise does not return
 */
int poller_loop(struct poller *plr) {
	int r = 0;
	do {
		poller_register_events(plr);
		r = poller_do_poll(plr);
	} while(r >= 0);
	return r;
}
