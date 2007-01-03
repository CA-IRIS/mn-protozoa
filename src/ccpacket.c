#include <stdlib.h>
#include "ccpacket.h"
#include "log.h"

struct packet_counter *packet_counter_init(struct packet_counter *cnt,
	struct log *log)
{
	cnt->log = log;
	cnt->n_packets = 0;
	cnt->n_dropped = 0;
	cnt->n_status = 0;
	cnt->n_pan = 0;
	cnt->n_tilt = 0;
	cnt->n_zoom = 0;
	cnt->n_lens = 0;
	cnt->n_aux = 0;
	cnt->n_preset = 0;
	return cnt;
}

struct packet_counter *packet_counter_new(struct log *log) {
	struct packet_counter *cnt = malloc(sizeof(struct packet_counter));
	if(cnt == NULL)
		return NULL;
	return packet_counter_init(cnt, log);
}

static void counter_print(const struct packet_counter *c, const char *stat,
	long long count)
{
	float percent = 100 * (float)count / (float)c->n_packets;
	log_println(c->log, "%10s: %10lld  %6.2f%%", stat, count, percent);
}

static void counter_display(const struct packet_counter *c) {
	log_println(c->log, "protozoa statistics: %lld packets", c->n_packets);
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
	if(p->command & (CC_PAN_LEFT | CC_PAN_RIGHT) && p->pan)
		c->n_pan++;
	if(p->command & (CC_TILT_UP | CC_TILT_DOWN) && p->tilt)
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

void ccpacket_log(struct ccpacket *p, struct log *log, const char *name) {
	log_line_start(log);
	log_printf(log, "packet: %lld %s rcv: %d", p->n_packet++, name,
		p->receiver);
	if(p->status)
		log_printf(log, " status: %d", p->status);
	if(p->command & CC_PAN_LEFT)
		log_printf(log, " pan left: %d", p->pan);
	else if(p->command & CC_PAN_RIGHT)
		log_printf(log, " pan right: %d", p->pan);
	if(p->command & CC_TILT_UP)
		log_printf(log, " tilt up: %d", p->tilt);
	else if(p->command & CC_TILT_DOWN)
		log_printf(log, " tilt down: %d", p->tilt);
	if(p->zoom)
		log_printf(log, " zoom: %d", p->zoom);
	if(p->focus)
		log_printf(log, " focus: %d", p->focus);
	if(p->iris)
		log_printf(log, " iris: %d", p->iris);
	if(p->aux)
		log_printf(log, " aux: %d", p->aux);
	if(p->preset) {
		if(p->command & CC_RECALL)
			log_printf(log, " recall");
		else if(p->command & CC_STORE)
			log_printf(log, " store");
		else if(p->command & CC_CLEAR)
			log_printf(log, " clear");
		log_printf(log, " preset: %d", p->preset);
	}
	if(p->command & CC_AUTO_IRIS)
		log_printf(log, " auto-iris");
	if(p->command & CC_AUTO_PAN)
		log_printf(log, " auto-pan");
	if(p->command & CC_MANUAL_PAN)
		log_printf(log, " manual-pan");
	if(p->command & CC_LENS_SPEED)
		log_printf(log, " lens-speed");
	if(p->command & CC_ACK_ALARM)
		log_printf(log, " ack-alarm");
	log_line_end(log);
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
