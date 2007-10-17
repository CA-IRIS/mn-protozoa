#ifndef __CCWRITER_H__
#define __CCWRITER_H__

#include "ccpacket.h"	/* for struct ccpacket */
#include "channel.h"	/* for struct channel */

struct ccwriter {
	unsigned int (*do_write) (struct ccwriter *wtr, struct ccpacket *pkt);
	struct	log		*log;		/* message logger */
	struct	buffer		*txbuf;		/* transmit buffer */
	char			*auth;		/* authentication token */
};

struct ccwriter *ccwriter_new(struct channel *chn, const char *protocol,
	const char *auth);
void *ccwriter_append(struct ccwriter *wtr, size_t n_bytes);
int ccwriter_get_receiver(const struct ccwriter *wtr, int receiver);

#endif
