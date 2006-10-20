#ifndef __CCREADER_H__
#define __CCREADER_H__

#include "buffer.h"
#include "ccpacket.h"

enum decode_t {
	MORE = 0,
	DONE = 1,
};

struct ccreader {
	void	(*do_read)	(struct ccreader *r, struct buffer *rxbuf);
	struct	ccpacket	packet;		/* camera control packet */
	struct	ccwriter	*writer;	/* head of writer list */
	const char		*name;		/* channel name */
	bool			verbose;	/* verbose flag */
};

void ccreader_init(struct ccreader *r);
void ccreader_add_writer(struct ccreader *r, struct ccwriter *w);
unsigned int ccreader_process_packet(struct ccreader *r);
struct ccreader *ccreader_create(const char *name, const char *protocol,
	bool verbose);

#endif
