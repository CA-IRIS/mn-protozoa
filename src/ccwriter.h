#ifndef __CCWRITER_H__
#define __CCWRITER_H__

#include "ccpacket.h"	/* for struct ccpacket */
#include "channel.h"	/* for struct channel */

struct ccwriter {
	unsigned int (*do_write) (struct ccwriter *wtr, struct ccpacket *pkt);
	struct channel		*chn;		/* channel to write */
	struct ccpacket		*packet;	/* previous command packets */
	unsigned int		n_rcv;		/* number of receivers */
	unsigned int		timeout;	/* time command is held (ms) */
	char			*auth;		/* authentication token */
};

struct ccwriter *ccwriter_new(struct channel *chn, const char *protocol,
	const char *auth);
void *ccwriter_append(struct ccwriter *wtr, size_t n_bytes);
int ccwriter_do_write(struct ccwriter *wtr, struct ccpacket *pkt);

#endif
