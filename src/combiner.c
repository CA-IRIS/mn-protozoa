#include <stdio.h>
#include "combiner.h"

static int combiner_do_read(struct handler *h, struct buffer *rxbuf) {
	/* Do nothing */
	return 0;
}

static int combiner_do_write(struct combiner *c) {
	/* Do nothing */
	return 0;
}

void combiner_init(struct combiner *c, struct buffer *txbuf) {
	c->handler.do_read = combiner_do_read;
	c->do_write = combiner_do_write;
	ccpacket_init(&c->packet);
	c->txbuf = txbuf;
}

void combiner_write(struct combiner *c, uint8_t *mess, size_t count) {
	printf("out: %02x %02x %02x\n", mess[0], mess[1], mess[2]);
}
