#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <stdbool.h>
#include "buffer.h"
#include "ccreader.h"

struct channel {
	char		*name;			/* channel name */
	int		fd;			/* file descriptor */
	int		extra;			/* extra parameter */
	bool		verbose;		/* verbose flag */

	struct buffer	*rxbuf;			/* receive buffer */
	struct buffer	*txbuf;			/* transmit buffer */

	struct ccreader *reader;		/* camera control reader */
};

void channel_debug(struct channel *chn, const char* msg);
bool channel_is_open(const struct channel *chn);
bool channel_is_waiting(const struct channel *chn);
struct channel* channel_init(struct channel *chn, const char *name, int extra,
	bool verbose);
int channel_open(struct channel *chn);
int channel_close(struct channel *chn);
void channel_destroy(struct channel *chn);
ssize_t channel_read(struct channel *chn);
ssize_t channel_write(struct channel *chn);

#endif
