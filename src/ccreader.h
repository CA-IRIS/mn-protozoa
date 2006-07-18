#ifndef __CCREADER_H__
#define __CCREADER_H__

#include "ccpacket.h"
#include "sport.h"

struct ccreader {
	struct	handler		handler;	/* "sub-struct" of handler */
	struct	ccpacket	packet;		/* camera control packet */
	struct	ccwriter	*writer;	/* head of writer list */
	bool			verbose;	/* verbose flag */
};

void ccreader_init(struct ccreader *r);
void ccreader_add_writer(struct ccreader *r, struct ccwriter *w);
int ccreader_process_packet(struct ccreader *r);
struct ccreader *ccreader_create(struct sport *port, const char *protocol,
	bool verbose);

#endif
