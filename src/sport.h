#ifndef __SPORT_H__
#define __SPORT_H__

#include "buffer.h"

#define BUFFER_SIZE 256

struct handler {
	int	(*do_read)	(struct handler *h, struct buffer *rxbuf);
	int	(*do_write)	(struct handler *h, struct buffer *txbuf);
};

struct sport {
	int		fd;
	struct buffer	rxbuf;
	struct buffer	txbuf;

	struct handler	*handler;
};

struct sport* sport_init(
	struct sport	*port,
	const char	*name);

ssize_t sport_read(struct sport *port);
ssize_t sport_write(struct sport *port);

#endif
