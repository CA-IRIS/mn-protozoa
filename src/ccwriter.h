#ifndef __CCWRITER_H__
#define __CCWRITER_H__

#include <sys/time.h>	/* for struct timeval */
#include "ccpacket.h"	/* for struct ccpacket */
#include "channel.h"	/* for struct channel, gettimeofday */

struct ccwriter {
	unsigned int (*do_write) (struct ccwriter *wtr, struct ccpacket *pkt);
	struct channel		*chn;		/* channel to write */
	struct timeval		*ptime;		/* previous command times */
	char			*auth;		/* authentication token */
};

struct ccwriter *ccwriter_new(struct channel *chn, const char *protocol,
	const char *auth);
void *ccwriter_append(struct ccwriter *wtr, size_t n_bytes);
void ccwriter_command_receiver(struct ccwriter *wtr, const int receiver);

#endif
