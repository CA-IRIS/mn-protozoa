#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include "buffer.h"
#include "ccreader.h"

struct channel {
	char		*name;			/* channel name */
	int		fd;			/* file descriptor */
	int		extra;			/* extra parameter */

	struct buffer	*rxbuf;			/* receive buffer */
	struct buffer	*txbuf;			/* transmit buffer */

	struct ccreader *reader;		/* camera control reader */
};

struct channel* channel_init(struct channel *c, const char *name, int extra);
void channel_destroy(struct channel *c);
ssize_t channel_read(struct channel *c);
ssize_t channel_write(struct channel *c);

#endif
