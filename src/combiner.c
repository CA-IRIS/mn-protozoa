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

void print_stats(const char *stat, long long count, long long total) {
	int percent = (int)(100 * count / total);
	printf("%10s: %10lld  %3d%%\n", stat, count, percent);
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
		printf("protozoa statistis:\n");
		print_stats("packets", c->n_packets, c->n_packets);
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
		printf("protozoa: output buffer full\n");
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
