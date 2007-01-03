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
	struct ccpacket *pkt)
{
	cnt->n_packets++;
	if(pkt->status)
		cnt->n_status++;
	if(pkt->command & (CC_PAN_LEFT | CC_PAN_RIGHT) && pkt->pan)
		cnt->n_pan++;
	if(pkt->command & (CC_TILT_UP | CC_TILT_DOWN) && pkt->tilt)
		cnt->n_tilt++;
	if(pkt->zoom)
		cnt->n_zoom++;
	if(pkt->focus | pkt->iris)
		cnt->n_lens++;
	if(pkt->aux)
		cnt->n_aux++;
	if(pkt->command & (CC_RECALL | CC_STORE))
		cnt->n_preset++;
	if((cnt->n_packets % 100) == 0)
		packet_counter_display(cnt);
}

void ccpacket_init(struct ccpacket *pkt) {
	pkt->counter = NULL;
	pkt->n_packet = 0;
	ccpacket_clear(pkt);
}

void ccpacket_clear(struct ccpacket *pkt) {
	pkt->receiver = 0;
	pkt->status = STATUS_NONE;
	pkt->command = 0;
	pkt->pan = 0;
	pkt->tilt = 0;
	pkt->zoom = ZOOM_NONE;
	pkt->focus = FOCUS_NONE;
	pkt->iris = IRIS_NONE;
	pkt->aux = 0;
	pkt->preset = 0;
}

void ccpacket_log(struct ccpacket *pkt, struct log *log, const char *name) {
	log_line_start(log);
	log_printf(log, "packet: %lld %s rcv: %d", pkt->n_packet++, name,
		pkt->receiver);
	if(pkt->status)
		log_printf(log, " status: %d", pkt->status);
	if(pkt->command & CC_PAN_LEFT)
		log_printf(log, " pan left: %d", pkt->pan);
	else if(pkt->command & CC_PAN_RIGHT)
		log_printf(log, " pan right: %d", pkt->pan);
	if(pkt->command & CC_TILT_UP)
		log_printf(log, " tilt up: %d", pkt->tilt);
	else if(pkt->command & CC_TILT_DOWN)
		log_printf(log, " tilt down: %d", pkt->tilt);
	if(pkt->zoom)
		log_printf(log, " zoom: %d", pkt->zoom);
	if(pkt->focus)
		log_printf(log, " focus: %d", pkt->focus);
	if(pkt->iris)
		log_printf(log, " iris: %d", pkt->iris);
	if(pkt->aux)
		log_printf(log, " aux: %d", pkt->aux);
	if(pkt->preset) {
		if(pkt->command & CC_RECALL)
			log_printf(log, " recall");
		else if(pkt->command & CC_STORE)
			log_printf(log, " store");
		else if(pkt->command & CC_CLEAR)
			log_printf(log, " clear");
		log_printf(log, " preset: %d", pkt->preset);
	}
	if(pkt->command & CC_AUTO_IRIS)
		log_printf(log, " auto-iris");
	if(pkt->command & CC_AUTO_PAN)
		log_printf(log, " auto-pan");
	if(pkt->command & CC_MANUAL_PAN)
		log_printf(log, " manual-pan");
	if(pkt->command & CC_LENS_SPEED)
		log_printf(log, " lens-speed");
	if(pkt->command & CC_ACK_ALARM)
		log_printf(log, " ack-alarm");
	log_line_end(log);
}

void ccpacket_count(struct ccpacket *pkt) {
	if(pkt->counter)
		packet_counter_count(pkt->counter, pkt);
}

void ccpacket_drop(struct ccpacket *pkt) {
	if(pkt->counter) {
		pkt->counter->n_dropped++;
		ccpacket_count(pkt);
	}
}
