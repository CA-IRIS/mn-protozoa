#ifndef __CCREADER_H__
#define __CCREADER_H__

#include "buffer.h"
#include "ccpacket.h"
#include "log.h"

enum decode_t {
	MORE = 0,
	DONE = 1,
};

struct ccreader {
	void	(*do_read)	(struct ccreader *rdr, struct buffer *rxbuf);
	struct	ccpacket	packet;		/* camera control packet */
	struct	ccwriter	*writer;	/* head of writer list */
	const char		*name;		/* channel name */
	struct	log		*log;		/* log file */
};

void ccreader_add_writer(struct ccreader *rdr, struct ccwriter *w);
unsigned int ccreader_process_packet(struct ccreader *rdr);
struct ccreader *ccreader_create(const char *name, const char *protocol,
	struct log *log);

#endif
