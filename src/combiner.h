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

#endif
