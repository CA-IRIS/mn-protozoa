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

bool channel_has_reader(const struct channel *chn) {
	return chn->reader != NULL;
}

bool channel_is_waiting(const struct channel *chn) {
	return (!buffer_is_empty(chn->txbuf)) || (chn->reader != NULL);
}

void channel_log(struct channel *chn, const char* msg) {
	log_println(chn->log, "channel: %s %s", msg, chn->name);
}

ssize_t channel_read(struct channel *chn) {
	ssize_t n_bytes = buffer_read(chn->rxbuf, chn->fd);
	if(n_bytes <= 0)
		return n_bytes;
	if(chn->reader) {
		log_buffer_in(chn->log, chn->rxbuf, n_bytes);
		chn->reader->do_read(chn->reader, chn->rxbuf);
		return n_bytes;
	} else {
		/* Data is coming in on the channel, but we're not set up to
		 * handle it -- just ignore. */
		buffer_clear(chn->rxbuf);
		return 0;
	}
}

ssize_t channel_write(struct channel *chn) {
	log_buffer_out(chn->log, chn->txbuf);
	return buffer_write(chn->txbuf, chn->fd);
}

struct channel* channel_init(struct channel *chn, const char *name, int extra,
	struct log *log)
{
	bzero(chn, sizeof(struct channel));
	chn->extra = extra;
	chn->log = log;
	chn->name = malloc(strlen(name) + 1);
	if(chn->name == NULL)
		goto fail;
	strcpy(chn->name, name);
	chn->rxbuf = malloc(BUFFER_SIZE);
	if(chn->rxbuf == NULL)
		goto fail;
	if(buffer_init(name, chn->rxbuf, BUFFER_SIZE) == NULL)
		goto fail;
	chn->txbuf = malloc(BUFFER_SIZE);
	if(chn->txbuf == NULL)
		goto fail;
	if(buffer_init(name, chn->txbuf, BUFFER_SIZE) == NULL)
		goto fail;
	chn->reader = NULL;
	return chn;
fail:
	free(chn->name);
	free(chn->rxbuf);
	free(chn->txbuf);
	chn->name = NULL;
	chn->rxbuf = NULL;
	chn->txbuf = NULL;
	return NULL;
}

static inline bool channel_is_sport(const struct channel *chn) {
	return chn->name[0] == '/';
}

int channel_open(struct channel *chn) {
	channel_log(chn, "opening");
	if(channel_is_sport(chn))
		return sport_open(chn);
	else
		return tcp_open(chn);
}

int channel_close(struct channel *chn) {
	buffer_clear(chn->rxbuf);
	buffer_clear(chn->txbuf);
	if(channel_is_open(chn)) {
		channel_log(chn, "closing");
		int r = close(chn->fd);
		chn->fd = 0;
		return r;
	} else
		return 0;
}

void channel_destroy(struct channel *chn) {
	channel_close(chn);
	free(chn->name);
	free(chn->rxbuf);
	free(chn->txbuf);
}
