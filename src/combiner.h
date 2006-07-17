#ifndef __COMBINER_H__
#define __COMBINER_H__

#include "ccpacket.h"
#include "sport.h"

struct combiner {
	struct	handler		handler;	/* "sub-struct" of handler */
	int	(*do_write)	(struct combiner *c);
	struct	ccpacket	packet;		/* camera control packet */
	struct	buffer		*txbuf;		/* transmit buffer */
	int			base;		/* receiver address base */
	bool			verbose;	/* verbose flag */
};

void combiner_init(struct combiner *c);
int combiner_set_output_protocol(struct combiner *c, const char *protocol);
int combiner_set_input_protocol(struct combiner *c, const char *protocol);
void combiner_write(struct combiner *c, uint8_t *mess, size_t count);
int combiner_process_packet(struct combiner *c);
struct combiner *combiner_create_outbound(struct sport *port,
	const char *protocol, bool verbose);
struct combiner *combiner_create_inbound(struct sport *port,
	const char *protocol, struct combiner *out);

#endif
