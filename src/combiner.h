#ifndef __COMBINER_H__
#define __COMBINER_H__

#include "ccpacket.h"
#include "sport.h"

struct combiner {
	struct	handler	handler;		/* "sub-struct" of handler */
	int	(*do_write)	(struct combiner *c);
	struct	ccpacket	packet;		/* camera control packet */
	struct	buffer		*txbuf;		/* transmit buffer */
};

void combiner_init(struct combiner *cmbnr, struct buffer *txbuf);
void combiner_write(struct combiner *c, uint8_t *mess, size_t count);

#endif
