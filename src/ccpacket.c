#include <stdlib.h>	/* for malloc */
#include <strings.h>	/* for bzero */
#include "ccpacket.h"

struct packet_counter *packet_counter_init(struct packet_counter *cnt,
	struct log *log)
{
	bzero(cnt, sizeof(struct packet_counter));
	cnt->log = log;
	return cnt;
}

struct packet_counter *packet_counter_new(struct log *log) {
	struct packet_counter *cnt = malloc(sizeof(struct packet_counter));
	if(cnt == NULL)
		return NULL;
	return packet_counter_init(cnt, log);
}

static void packet_counter_print(const struct packet_counter *cnt,
	const char *stat, long long count)
{
	float percent = 100 * (float)count / (float)cnt->n_packets;
	log_println(cnt->log, "%10s: %10lld  %6.2f%%", stat, count, percent);
}

static void packet_counter_display(const struct packet_counter *cnt) {
	log_println(cnt->log, "protozoa statistics: %lld packets",
		cnt->n_packets);
	if(cnt->n_dropped)
		packet_counter_print(cnt, "dropped", cnt->n_dropped);
	if(cnt->n_status)
		packet_counter_print(cnt, "status", cnt->n_status);
	if(cnt->n_pan)
		packet_counter_print(cnt, "pan", cnt->n_pan);
	if(cnt->n_tilt)
		packet_counter_print(cnt, "tilt", cnt->n_tilt);
	if(cnt->n_zoom)
		packet_counter_print(cnt, "zoom", cnt->n_zoom);
	if(cnt->n_lens)
		packet_counter_print(cnt, "lens", cnt->n_lens);
	if(cnt->n_aux)
		packet_counter_print(cnt, "aux", cnt->n_aux);
	if(cnt->n_preset)
		packet_counter_print(cnt, "preset", cnt->n_preset);
}

static void packet_counter_count(struct packet_counter *cnt,
	struct ccpacket *p)
{
	cnt->n_packets++;
	if(p->status)
		cnt->n_status++;
	if(p->command & (CC_PAN_LEFT | CC_PAN_RIGHT) && p->pan)
		cnt->n_pan++;
	if(p->command & (CC_TILT_UP | CC_TILT_DOWN) && p->tilt)
		cnt->n_tilt++;
	if(p->zoom)
		cnt->n_zoom++;
	if(p->focus | p->iris)
		cnt->n_lens++;
	if(p->aux)
		cnt->n_aux++;
	if(p->command & (CC_RECALL | CC_STORE))
		cnt->n_preset++;
	if((cnt->n_packets % 100) == 0)
		packet_counter_display(cnt);
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
		packet_counter_count(p->counter, p);
}

void ccpacket_drop(struct ccpacket *p) {
	if(p->counter) {
		p->counter->n_dropped++;
		ccpacket_count(p);
	}
}
