#ifndef __DEFER_H__
#define __DEFER_H__

#include <sys/time.h>	/* for struct timeval */
#include "ccpacket.h"	/* for struct ccpacket */
#include "ccwriter.h"	/* for struct ccwriter */
#include "clump.h"	/* for struct cl_rbtree */

struct deferred_pkt {
	struct timeval		tv;		/* time to send packet */
	struct ccpacket		*packet;	/* packet to be deferred */
	struct ccwriter		*writer;	/* writer to send packet */
};

struct defer {
	struct cl_rbtree	tree;		/* tree of deferred packets */
	struct cl_pool		pool;		/* pool of deferred packets */
};

struct defer *defer_init(struct defer *dfr);
int defer_packet(struct defer *dfr, struct ccpacket *pkt,
	struct ccwriter *wtr);
int defer_next(struct defer *dfr);
int defer_get_fd(struct defer *dfr);

#endif
