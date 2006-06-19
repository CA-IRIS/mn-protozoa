#ifndef __COMBINER_H__
#define __COMBINER_H__

#include "ccpacket.h"
#include "sport.h"

struct combiner {
	struct	handler		handler;	/* "sub-struct" of handler */
	int	(*do_write)	(struct combiner *c);
	struct	ccpacket	packet;		/* camera control packet */
	struct	buffer		*txbuf;		/* transmit buffer */

	long long		n_dropped;	/* count of dropped packets */
	long long		n_packets;	/* total count of packets */
	long long		n_status;	/* count of status packets */
	long long		n_pan;		/* count of pan packets */
	long long		n_tilt;		/* count of tilt packets */
	long long		n_zoom;		/* count of zoom packets */
	long long		n_lens;		/* count of lens packets */
	long long		n_aux;		/* count of aux packets */
	long long		n_preset;	/* count of preset packets */
};

void combiner_init(struct combiner *cmbnr);
void combiner_count(struct combiner *c);
void combiner_drop(struct combiner *c);
void combiner_write(struct combiner *c, uint8_t *mess, size_t count);

#endif
