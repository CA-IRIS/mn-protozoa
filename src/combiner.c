#include <stdio.h>
#include <strings.h>
#include "combiner.h"
#include "manchester.h"
#include "pelco_d.h"
#include "vicon.h"

static int combiner_do_read(struct handler *h, struct buffer *rxbuf) {
	/* Do nothing */
	return 0;
}

static int combiner_do_write(struct combiner *c) {
	/* Do nothing */
	return 0;
}

void combiner_init(struct combiner *c) {
	c->handler.do_read = combiner_do_read;
	c->do_write = combiner_do_write;
	c->txbuf = NULL;
	c->base = 0;
	ccpacket_init(&c->packet);
}

int combiner_set_output_protocol(struct combiner *c, const char *protocol) {
	if(strcasecmp(protocol, "manchester") == 0)
		c->do_write = manchester_do_write;
	else if(strcasecmp(protocol, "pelco_d") == 0)
		c->do_write = pelco_d_do_write;
	else if(strcasecmp(protocol, "vicon") == 0)
		c->do_write = vicon_do_write;
	else {
		fprintf(stderr, "Unknown protocol: %s\n", protocol);
		return -1;
	}
	return 0;
}

int combiner_set_input_protocol(struct combiner *c, const char *protocol) {
	if(strcasecmp(protocol, "manchester") == 0)
		c->handler.do_read = manchester_do_read;
	else if(strcasecmp(protocol, "pelco_d") == 0)
		c->handler.do_read = pelco_d_do_read;
	else if(strcasecmp(protocol, "vicon") == 0)
		c->handler.do_read = vicon_do_read;
	else {
		fprintf(stderr, "Unknown protocol: %s\n", protocol);
		return -1;
	}
	return 0;
}

void combiner_write(struct combiner *c, uint8_t *mess, size_t count) {
	int i;
	if(buffer_remaining(c->txbuf) < count) {
		fprintf(stderr, "protozoa: output buffer full\n");
		ccpacket_drop(&c->packet);
		return;
	} else if(c->packet.status) {
		ccpacket_drop(&c->packet);
		return;
	}
	ccpacket_count(&c->packet);
	for(i = 0; i < count; i++)
		buffer_put(c->txbuf, mess[i]);
}

int combiner_process_packet(struct combiner *c) {
	if(c->packet.receiver == 0)
		return 0;
	c->packet.receiver += c->base;
	int r = c->do_write(c);
	if(r > 0) {
		if(c->verbose)
			ccpacket_debug(&c->packet);
		r = 0;
	}
	ccpacket_init(&c->packet);
	return r;
}

struct combiner *combiner_create_outbound(struct sport *port,
	const char *protocol, bool verbose)
{
	struct combiner *cmbnr = malloc(sizeof(struct combiner));
	if(cmbnr == NULL)
		return NULL;
	combiner_init(cmbnr);
	if(combiner_set_output_protocol(cmbnr, protocol) < 0)
		goto fail;
	cmbnr->txbuf = port->txbuf;
	cmbnr->verbose = verbose;
	port->handler = &cmbnr->handler;
	return cmbnr;
fail:
	free(cmbnr);
	return NULL;
}

struct combiner *combiner_create_inbound(struct sport *port,
	const char *protocol, struct combiner *out)
{
	struct combiner *cmbnr = malloc(sizeof(struct combiner));
	if(cmbnr == NULL)
		return NULL;
	if(out == NULL) {
		fprintf(stderr, "Missing OUT directive\n");
		goto fail;
	}
	combiner_init(cmbnr);
	if(combiner_set_input_protocol(cmbnr, protocol) < 0)
		goto fail;
	cmbnr->do_write = out->do_write;
	cmbnr->txbuf = out->txbuf;
	cmbnr->verbose = out->verbose;
	port->handler = &cmbnr->handler;
	return cmbnr;
fail:
	free(cmbnr);
	return NULL;
}
