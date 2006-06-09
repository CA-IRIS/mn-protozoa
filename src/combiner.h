#ifndef __COMBINER_H__
#define __COMBINER_H__

#include "ccpacket.h"
#include "sport.h"

struct combiner {
	struct handler	handler;	/* "sub-struct" of handler */
	struct ccpacket	packet;		/* current camera control packet */
	struct buffer	*txbuf;		/* transmit buffer */
};

#endif
