#include <stdio.h>
#include "ccpacket.h"

void counter_init(struct packet_counter *c) {
	c->n_packets = 0;
	c->n_dropped = 0;
	c->n_status = 0;
	c->n_pan = 0;
	c->n_tilt = 0;
	c->n_zoom = 0;
	c->n_lens = 0;
	c->n_aux = 0;
	c->n_preset = 0;
}

static void counter_print(const struct packet_counter *c, const char *stat,
	long long count)
{
	float percent = 100 * (float)count / (float)c->n_packets;
	printf("%10s: %10lld  %6.2f%%\n", stat, count, percent);
}

static void counter_display(const struct packet_counter *c) {
	printf("protozoa statistics: %lld packets\n", c->n_packets);
	if(c->n_dropped)
		counter_print(c, "dropped", c->n_dropped);
	if(c->n_status)
		counter_print(c, "status", c->n_status);
	if(c->n_pan)
		counter_print(c, "pan", c->n_pan);
	if(c->n_tilt)
		counter_print(c, "tilt", c->n_tilt);
	if(c->n_zoom)
		counter_print(c, "zoom", c->n_zoom);
	if(c->n_lens)
		counter_print(c, "lens", c->n_lens);
	if(c->n_aux)
		counter_print(c, "aux", c->n_aux);
	if(c->n_preset)
		counter_print(c, "preset", c->n_preset);
}

static void counter_count(struct packet_counter *c, struct ccpacket *p) {
	c->n_packets++;
	if(p->status)
		c->n_status++;
	if(p->command & (CC_PAN_LEFT | CC_PAN_RIGHT))
		c->n_pan++;
	if(p->command & (CC_TILT_UP | CC_TILT_DOWN))
		c->n_tilt++;
	if(p->zoom)
		c->n_zoom++;
	if(p->focus | p->iris)
		c->n_lens++;
	if(p->aux)
		c->n_aux++;
	if(p->command & (CC_RECALL | CC_STORE))
		c->n_preset++;
	if((c->n_packets % 100) == 0)
		counter_display(c);
}

void ccpacket_clear(struct ccpacket *p) {
	p->receiver = 0;
	p->status = STATUS_NONE;
	p->command = 0;
	p->pan = 0;
	p->tilt = 0;
	p->zoom = ZOOM_NONE;
	p->focus = FOCUS_NONE;
	p->iris = IRIS_NONE;
	p->aux = 0;
	p->preset = 0;
}

void ccpacket_init(struct ccpacket *p) {
	p->counter = NULL;
	p->n_packet = 0;
	ccpacket_clear(p);
}

void ccpacket_debug(struct ccpacket *p, const char *name) {
	printf("%s ", name);
	printf("(%lld) ", p->n_packet++);
	printf("rcv: %d", p->receiver);
	if(p->status)
		printf(" status: %d", p->status);
	if(p->command)
		printf(" command: %d", p->command);
	if(p->command & CC_PAN)
		printf(" pan: %d", p->pan);
	if(p->command & CC_TILT)
		printf(" tilt: %d", p->tilt);
	if(p->zoom)
		printf(" zoom: %d", p->zoom);
	if(p->focus)
		printf(" focus: %d", p->focus);
	if(p->iris)
		printf(" iris: %d", p->iris);
	if(p->aux)
		printf(" aux: %d", p->aux);
	if(p->preset)
		printf(" preset: %d", p->preset);
	printf("\n");
}

void ccpacket_count(struct ccpacket *p) {
	if(p->counter)
		counter_count(p->counter, p);
}

void ccpacket_drop(struct ccpacket *p) {
	if(p->counter) {
		p->counter->n_dropped++;
		ccpacket_count(p);
	}
}
