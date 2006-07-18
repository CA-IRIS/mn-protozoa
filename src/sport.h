#ifndef __SPORT_H__
#define __SPORT_H__

#include "buffer.h"

#define BUFFER_SIZE 256

struct handler {
	int	(*do_read)	(struct handler *h, struct buffer *rxbuf);
};

struct sport {
	char		*name;
	int		fd;
	int		baud;
	struct buffer	*rxbuf;
	struct buffer	*txbuf;

	struct handler	*handler;
};

struct sport* sport_init(struct sport *port, const char *name, int baud); 
ssize_t sport_read(struct sport *port);
ssize_t sport_write(struct sport *port);

#endif
