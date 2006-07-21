#ifndef __CCWRITER_H__
#define __CCWRITER_H__

#include "ccpacket.h"
#include "sport.h"

struct ccwriter {
	int	(*do_write)	(struct ccwriter *w, struct ccpacket *p);
	struct	buffer		*txbuf;		/* transmit buffer */
	int			base;		/* receiver address base */
	struct	ccwriter	*next;
};

void ccwriter_init(struct ccwriter *w);
int ccwriter_set_protocol(struct ccwriter *w, const char *protocol);
uint8_t *ccwriter_append(struct ccwriter *w, size_t count);
struct ccwriter *ccwriter_create(struct sport *port, const char *protocol,
	int base);

#endif
