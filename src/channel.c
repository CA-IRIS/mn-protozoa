#include <assert.h>	/* for assert */
#include <unistd.h>	/* for close */
#include <string.h>	/* for bzero, strlen, strcpy */
#include "channel.h"	/* for struct channel and prototypes */

#define BUFFER_SIZE 256

/* Define these prototypes here to avoid circular dependency */
int sport_open(struct channel *chn);
int tcp_open(struct channel *chn);

/*
 * channel_init		Initialize a new I/O channel.
 *
 * name: channel name
 * extra: extra data to initialize the channel
 * log: message logger
 * return: struct channel or NULL on error
 */
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
	if(buffer_init(&chn->rxbuf, BUFFER_SIZE) == NULL)
		goto fail;
	if(buffer_init(&chn->txbuf, BUFFER_SIZE) == NULL)
		goto fail;
	chn->reader = NULL;
	return chn;
fail:
	free(chn->name);
	chn->name = NULL;
	return NULL;
}

/*
 * channel_destroy	Destroy the previously initialized I/O channel.
 */
void channel_destroy(struct channel *chn) {
	channel_close(chn);
	buffer_destroy(&chn->rxbuf);
	buffer_destroy(&chn->txbuf);
	free(chn->name);
}

/*
 * channel_is_sport	Test if the channel is a serial port.
 *
 * return: true if channel is a serial port; otherwise false
 */
static inline bool channel_is_sport(const struct channel *chn) {
	return chn->name[0] == '/';
}

/*
 * channel_open		Open the I/O channel.
 *
 * return: 0 on success; -1 on error
 */
int channel_open(struct channel *chn) {
	assert(chn->fd == 0);
	channel_log(chn, "opening");
	if(channel_is_sport(chn))
		return sport_open(chn);
	else
		return tcp_open(chn);
}

/*
 * channel_close	Close the I/O channel.
 *
 * return: 0 on success; -1 on error
 */
int channel_close(struct channel *chn) {
	buffer_clear(&chn->rxbuf);
	buffer_clear(&chn->txbuf);
	if(channel_is_open(chn)) {
		channel_log(chn, "closing");
		int r = close(chn->fd);
		chn->fd = 0;
		return r;
	} else
		return 0;
}

/*
 * channel_is_open	Test if the I/O channel is currently open.
 *
 * return: true if channel is open; otherwise false
 */
bool channel_is_open(const struct channel *chn) {
	return (bool)(chn->fd);
}

/*
 * channel_has_reader	Test if the I/O channel has a reader.
 *
 * return: true if channel has a reader; otherwise false
 */
bool channel_has_reader(const struct channel *chn) {
	return chn->reader != NULL;
}

/*
 * channel_is_waiting	Test if the I/O channel is waiting to read or write.
 *
 * return: true if the channel is waiting; otherwise false
 */
bool channel_is_waiting(const struct channel *chn) {
	return (!buffer_is_empty(&chn->txbuf)) || (chn->reader != NULL);
}

/*
 * channel_log		Log a message related to the I/O channel.
 *
 * msg: message to write to log
 */
void channel_log(struct channel *chn, const char* msg) {
	log_println(chn->log, "channel: %s %s", msg, chn->name);
}

/*
 * channel_read		Read from the I/O channel.
 *
 * return: number of bytes read; -1 on error
 */
ssize_t channel_read(struct channel *chn) {
	ssize_t n_bytes = buffer_read(&chn->rxbuf, chn->fd);
	if(n_bytes <= 0)
		return n_bytes;
	if(channel_has_reader(chn)) {
		log_buffer_in(chn->log, &chn->rxbuf, chn->name, n_bytes);
		chn->reader->do_read(chn->reader, &chn->rxbuf);
		return n_bytes;
	} else {
		/* Data is coming in on the channel, but we're not set up to
		 * handle it -- just ignore. */
		buffer_clear(&chn->rxbuf);
		return 0;
	}
}

/*
 * channel_write	Write buffered data to the I/O channel.
 *
 * return: number of bytes written; -1 on error
 */
ssize_t channel_write(struct channel *chn) {
	log_buffer_out(chn->log, &chn->txbuf, chn->name);
	return buffer_write(&chn->txbuf, chn->fd);
}
