#ifndef __CCWRITER_H__
#define __CCWRITER_H__

#include <stdint.h>	/* for uint8_t */
#include "ccpacket.h"
#include "channel.h"

struct ccwriter {
	unsigned int (*do_write) (struct ccwriter *w, struct ccpacket *p);
	struct	log		*log;		/* message logger */
	struct	buffer		*txbuf;		/* transmit buffer */
	int			base;		/* receiver address base */
	int			range;		/* receiver address range */
	struct	ccwriter	*next;		/* next writer in the list */
};

uint8_t *ccwriter_append(struct ccwriter *w, size_t n_bytes);
struct ccwriter *ccwriter_create(struct channel *chn, const char *protocol,
	int base, int range);
void ccwriter_set_receiver(const struct ccwriter *w, struct ccpacket *p);

#endif
