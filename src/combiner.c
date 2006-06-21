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
	ccpacket_init(&c->packet);
	c->n_dropped = 0;
	c->n_packets = 0;
	c->n_status = 0;
	c->n_pan = 0;
	c->n_tilt = 0;
	c->n_zoom = 0;
	c->n_lens = 0;
	c->n_aux = 0;
	c->n_preset = 0;
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

static void print_stats(const char *stat, long long count, long long total) {
	float percent = 100 * (float)count / (float)total;
	printf("%10s: %10lld  %6.2f%%\n", stat, count, percent);
}

void combiner_count(struct combiner *c) {
	c->n_packets++;
	if(c->packet.status)
		c->n_status++;
	if(c->packet.command & (CC_PAN_LEFT | CC_PAN_RIGHT))
		c->n_pan++;
	if(c->packet.command & (CC_TILT_UP | CC_TILT_DOWN))
		c->n_tilt++;
	if(c->packet.zoom)
		c->n_zoom++;
	if(c->packet.focus | c->packet.iris)
		c->n_lens++;
	if(c->packet.aux)
		c->n_aux++;
	if(c->packet.command & (CC_RECALL | CC_STORE))
		c->n_preset++;
	if((c->n_packets % 100) == 0) {
		printf("protozoa statistics: %lld packets\n", c->n_packets);
		if(c->n_dropped)
			print_stats("dropped", c->n_dropped, c->n_packets);
		if(c->n_status)
			print_stats("status", c->n_status, c->n_packets);
		if(c->n_pan)
			print_stats("pan", c->n_pan, c->n_packets);
		if(c->n_tilt)
			print_stats("tilt", c->n_tilt, c->n_packets);
		if(c->n_zoom)
			print_stats("zoom", c->n_zoom, c->n_packets);
		if(c->n_lens)
			print_stats("lens", c->n_lens, c->n_packets);
		if(c->n_aux)
			print_stats("aux", c->n_aux, c->n_packets);
		if(c->n_preset)
			print_stats("preset", c->n_preset, c->n_packets);
	}
}

void combiner_drop(struct combiner *c) {
	c->n_dropped++;
	combiner_count(c);
}

void combiner_write(struct combiner *c, uint8_t *mess, size_t count) {
	int i;
	if(buffer_remaining(c->txbuf) < count) {
		fprintf(stderr, "protozoa: output buffer full\n");
		combiner_drop(c);
		return;
	} else if(c->packet.status) {
		combiner_drop(c);
		return;
	}
	combiner_count(c);
	for(i = 0; i < count; i++)
		buffer_put(c->txbuf, mess[i]);
}

int combiner_process_packet(struct combiner *c) {
	if(c->packet.receiver == 0)
		return 0;
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
