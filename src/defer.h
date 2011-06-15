#ifndef __DEFER_H__
#define __DEFER_H__

#include <sys/time.h>	/* for struct timeval */
#include "ccpacket.h"	/* for struct ccpacket */
#include "clump.h"	/* for struct cl_rbtree */

struct ccwriter;	/* avoid circular dependency */

struct deferred_pkt {
	struct ccwriter		*writer;	/* writer to send packet */
	struct timeval		tv;		/* time to send packet */
	struct timeval		sent;		/* last sent time */
	struct ccpacket		packet;		/* packet to be deferred */
	unsigned int		n_cnt;		/* number of times deferred */
};

struct defer {
	struct cl_rbtree	tree;		/* tree of deferred packets */
};

struct defer *defer_init(struct defer *dfr);
void defer_destroy(struct defer *dfr);
int defer_packet(struct defer *dfr, struct deferred_pkt *dpkt,
	struct ccpacket *pkt, unsigned int ms);
int defer_next(struct defer *dfr);
int defer_get_fd(struct defer *dfr);

#endif
