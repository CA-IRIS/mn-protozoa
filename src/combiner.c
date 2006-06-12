#include "combiner.h"

static int combiner_do_read(struct handler *h, struct buffer *rxbuf) {
	/* Do nothing */
	return 0;
}

static int combiner_do_write(struct combiner *c) {
	/* Do nothing */
	return 0;
}

void combiner_init(struct combiner *cmbnr, struct buffer *txbuf) {
	cmbnr->handler.do_read = combiner_do_read;
	cmbnr->do_write = combiner_do_write;
	ccpacket_init(&cmbnr->packet);
	cmbnr->txbuf = txbuf;
}
