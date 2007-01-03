#ifndef __CCWRITER_H__
#define __CCWRITER_H__

#include <stdint.h>	/* for uint8_t */
#include "ccpacket.h"	/* for struct ccpacket */
#include "channel.h"	/* for struct channel */

struct ccwriter {
	unsigned int (*do_write) (struct ccwriter *wtr, struct ccpacket *pkt);
	struct	log		*log;		/* message logger */
	struct	buffer		*txbuf;		/* transmit buffer */
	int			base;		/* receiver address base */
	int			range;		/* receiver address range */
	struct	ccwriter	*next;		/* next writer in the list */
};

uint8_t *ccwriter_append(struct ccwriter *wtr, size_t n_bytes);
struct ccwriter *ccwriter_create(struct channel *chn, const char *protocol,
	int base, int range);
int ccwriter_get_receiver(const struct ccwriter *wtr, int receiver);

#endif
