#include <unistd.h>
#include "string.h"
#include "channel.h"

#define BUFFER_SIZE 256

/* Define these prototypes here to avoid circular dependency */
int sport_open(struct channel *chn);
int tcp_open(struct channel *chn);

bool channel_is_open(const struct channel *chn) {
	return (bool)(chn->fd);
}

bool channel_is_waiting(const struct channel *chn) {
	return (!buffer_is_empty(chn->txbuf)) || (chn->reader != NULL);
}

void channel_debug(struct channel *chn, const char* msg) {
	log_println(chn->log, "Channel %s: %s", msg, chn->name);
}

ssize_t channel_read(struct channel *c) {
	ssize_t n_bytes = buffer_read(c->rxbuf, c->fd);
	if(n_bytes <= 0)
		return n_bytes;
	if(c->reader) {
		buffer_debug_in(c->rxbuf, c->log, n_bytes);
		c->reader->do_read(c->reader, c->rxbuf);
		return n_bytes;
	} else {
		/* Data is coming in on the channel, but we're not set up to
		 * handle it -- just ignore. */
		buffer_clear(c->rxbuf);
		return 0;
	}
}

ssize_t channel_write(struct channel *c) {
	buffer_debug_out(c->txbuf, c->log);
	return buffer_write(c->txbuf, c->fd);
}

struct channel* channel_init(struct channel *c, const char *name, int extra,
	struct log *log)
{
	bzero(c, sizeof(struct channel));
	c->name = malloc(strlen(name) + 1);
	if(c->name == NULL)
		goto fail;
	strcpy(c->name, name);
	c->extra = extra;
	c->log = log;
	c->rxbuf = malloc(BUFFER_SIZE);
	if(c->rxbuf == NULL)
		goto fail;
	if(buffer_init(name, c->rxbuf, BUFFER_SIZE) == NULL)
		goto fail;
	c->txbuf = malloc(BUFFER_SIZE);
	if(c->txbuf == NULL)
		goto fail;
	if(buffer_init(name, c->txbuf, BUFFER_SIZE) == NULL)
		goto fail;
	c->reader = NULL;
	return c;
fail:
	free(c->name);
	free(c->rxbuf);
	free(c->txbuf);
	return NULL;
}

static inline bool channel_is_sport(const struct channel *chn) {
	return chn->name[0] == '/';
}

int channel_open(struct channel *chn) {
	channel_debug(chn, "Opening");
	if(channel_is_sport(chn))
		return sport_open(chn);
	else
		return tcp_open(chn);
}

int channel_close(struct channel *chn) {
	buffer_clear(chn->rxbuf);
	buffer_clear(chn->txbuf);
	if(channel_is_open(chn)) {
		channel_debug(chn, "Closing");
		int r = close(chn->fd);
		chn->fd = 0;
		return r;
	} else
		return 0;
}

void channel_destroy(struct channel *c) {
	channel_close(c);
	free(c->name);
	free(c->rxbuf);
	free(c->txbuf);
}
