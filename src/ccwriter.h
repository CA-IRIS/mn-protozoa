#ifndef __CCWRITER_H__
#define __CCWRITER_H__

#include "ccpacket.h"
#include "channel.h"

struct ccwriter {
	unsigned int (*do_write) (struct ccwriter *w, struct ccpacket *p);
	struct	buffer		*txbuf;		/* transmit buffer */
	int			base;		/* receiver address base */
	struct	ccwriter	*next;
};

void ccwriter_init(struct ccwriter *w);
int ccwriter_set_protocol(struct ccwriter *w, const char *protocol);
uint8_t *ccwriter_append(struct ccwriter *w, size_t n_bytes);
struct ccwriter *ccwriter_create(struct channel *chn, const char *protocol,
	int base);

#endif
