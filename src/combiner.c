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
	int i;
	if(buffer_remaining(c->txbuf) < count) {
		printf("protozoa: output buffer full -- dropping packet\n");
		return;
	}
	for(i = 0; i < count; i++)
		buffer_put(c->txbuf, mess[i]);
}
