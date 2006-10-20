#include "string.h"
#include "channel.h"

#define BUFFER_SIZE 256

ssize_t channel_read(struct channel *c) {
	ssize_t n_bytes = buffer_read(c->rxbuf, c->fd);
	if(n_bytes <= 0)
		return n_bytes;
	if(c->reader) {
		buffer_debug_in(c->rxbuf, n_bytes);
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
	buffer_debug_out(c->txbuf);
	return buffer_write(c->txbuf, c->fd);
}

struct channel* channel_init(struct channel *c, const char *name) {
	bzero(c, sizeof(struct channel));
	c->name = malloc(strlen(name) + 1);
	if(c->name == NULL)
		goto fail;
	strcpy(c->name, name);
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

void channel_destroy(struct channel *c) {
	free(c->name);
	free(c->rxbuf);
	free(c->txbuf);
}
