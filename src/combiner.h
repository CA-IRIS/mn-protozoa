#ifndef __COMBINER_H__
#define __COMBINER_H__

#include "ccpacket.h"
#include "sport.h"

struct combiner {
	struct	handler		handler;	/* "sub-struct" of handler */
	int	(*do_write)	(struct combiner *c);
	struct	ccpacket	packet;		/* camera control packet */
	struct	buffer		*txbuf;		/* transmit buffer */

	int			n_dropped;	/* count of dropped packets */
	int			n_packets;	/* total count of packets */
	int			n_status;	/* count of status packets */
	int			n_pan;		/* count of pan packets */
	int			n_tilt;		/* count of tilt packets */
	int			n_zoom;		/* count of zoom packets */
	int			n_lens;		/* count of lens packets */
	int			n_aux;		/* count of aux packets */
	int			n_preset;	/* count of preset packets */
};

void combiner_init(struct combiner *cmbnr);
void combiner_count(struct combiner *c);
void combiner_drop(struct combiner *c);
void combiner_write(struct combiner *c, uint8_t *mess, size_t count);

#endif
