/*
 * protozoa -- CCTV transcoder / mixer for PTZ
 * Copyright (C) 2006-2011  Minnesota Department of Transportation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <stdlib.h>	/* for malloc */
#include <string.h>	/* for memset */
#include "ccpacket.h"

/*
 * packet_counter_init	Initialize a new packet counter.
 *
 * log: message logger
 * return: pointer to packet counter
 */
struct packet_counter *packet_counter_init(struct packet_counter *cnt,
	struct log *log)
{
	memset(cnt, 0, sizeof(struct packet_counter));
	cnt->log = log;
	return cnt;
}

/*
 * packet_counter_new	Construct a new packet counter.
 *
 * log: message logger
 * return: pointer to packet counter; NULL or error
 */
struct packet_counter *packet_counter_new(struct log *log) {
	struct packet_counter *cnt = malloc(sizeof(struct packet_counter));
	if(cnt == NULL)
		return NULL;
	return packet_counter_init(cnt, log);
}

/*
 * packet_counter_print		Print packet counter statistics.
 *
 * stat: name of statistic to print
 * count: count of the statistic
 */
static void packet_counter_print(const struct packet_counter *cnt,
	const char *stat, long long count)
{
	float percent = 100 * (float)count / (float)cnt->n_packets;
	log_println(cnt->log, "%10s: %10lld  %6.2f%%", stat, count, percent);
}

/*
 * packet_counter_display	Display all packet counter statistics.
 */
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

/*
 * packet_counter_count		Count one packet in the packet counter.
 *
 * pkt: packet to count
 */
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

/*
 * ccpacket_init	Initialize a new camera control packet.
 */
void ccpacket_init(struct ccpacket *pkt) {
	timeval_set_now(&pkt->expire);
	pkt->counter = NULL;
	pkt->n_packet = 0;
	ccpacket_clear(pkt);
}

/*
 * ccpacket_set_timeout	Set the timeout for a camera control packet.
 */
void ccpacket_set_timeout(struct ccpacket *pkt, unsigned int timeout) {
	timeval_set_now(&pkt->expire);
	timeval_adjust(&pkt->expire, timeout);
}

/*
 * ccpacket_clear	Clear the camera control packet.
 */
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

/*
 * ccpacket_is_stop	Test if the packet is a stop command.
 */
bool ccpacket_is_stop(struct ccpacket *pkt) {
	return (pkt->command | CC_PAN_TILT) == CC_PAN_TILT &&
	       pkt->pan == 0 &&
	       pkt->tilt == 0 &&
	       pkt->zoom == ZOOM_NONE &&
	       pkt->focus == FOCUS_NONE &&
	       pkt->iris == IRIS_NONE &&
	       pkt->aux == 0 &&
	       pkt->status == STATUS_NONE;
}

/*
 * ccpacket_has_command	Test if a packet has a command to encode.
 *
 * pkt: Packet to check for command
 * return: True if command is present; false otherwise
 */
bool ccpacket_has_command(const struct ccpacket *pkt) {
	if(pkt->command & CC_PAN_TILT)
		return true;
	if(pkt->zoom || pkt->focus || pkt->iris)
		return true;
	return false;
}

/*
 * ccpacket_has_aux	Test if the packet has an auxiliary function.
 */
bool ccpacket_has_aux(struct ccpacket *pkt) {
	if(pkt->aux)
		return true;
	else
		return false;
}

/*
 * ccpacket_has_preset	Test if the packet has a preset command.
 */
bool ccpacket_has_preset(struct ccpacket *pkt) {
	return pkt->command & CC_PRESET;
}

/*
 * ccpacket_has_autopan	Test if the packet has an autopan command.
 */
bool ccpacket_has_autopan(const struct ccpacket *pkt) {
	if(pkt->command & (CC_AUTO_PAN | CC_MANUAL_PAN))
		return true;
	else
		return false;
}

/*
 * ccpacket_has_power	Test if the packet has a power command.
 */
bool ccpacket_has_power(const struct ccpacket *pkt) {
	if(pkt->command & (CC_CAMERA_ON | CC_CAMERA_OFF))
		return true;
	else
		return false;
}

/*
 * ccpacket_log_pan	Log any pan command in the camera control packet.
 *
 * log: message logger
 */
static inline void ccpacket_log_pan(struct ccpacket *pkt, struct log *log) {
	if(pkt->pan == 0)
		log_printf(log, " pan: 0");
	else if(pkt->command & CC_PAN_LEFT)
		log_printf(log, " pan left: %d", pkt->pan);
	else if(pkt->command & CC_PAN_RIGHT)
		log_printf(log, " pan right: %d", pkt->pan);
}

/*
 * ccpacket_log_tilt	Log any tilt command in the camera control packet.
 *
 * log: message logger
 */
static inline void ccpacket_log_tilt(struct ccpacket *pkt, struct log *log) {
	if(pkt->tilt == 0)
		log_printf(log, " tilt: 0");
	else if(pkt->command & CC_TILT_UP)
		log_printf(log, " tilt up: %d", pkt->tilt);
	else if(pkt->command & CC_TILT_DOWN)
		log_printf(log, " tilt down: %d", pkt->tilt);
}

/*
 * ccpacket_log_lens	Log any lens commands in the camera control packet.
 *
 * log: message logger
 */
static inline void ccpacket_log_lens(struct ccpacket *pkt, struct log *log) {
	if(pkt->zoom)
		log_printf(log, " zoom: %d", pkt->zoom);
	if(pkt->focus)
		log_printf(log, " focus: %d", pkt->focus);
	if(pkt->iris)
		log_printf(log, " iris: %d", pkt->iris);
}

/*
 * ccpacket_log_preset	Log any preset commands in the camera control packet.
 *
 * log: message logger
 */
static inline void ccpacket_log_preset(struct ccpacket *pkt, struct log *log) {
	if(pkt->command & CC_RECALL)
		log_printf(log, " recall");
	else if(pkt->command & CC_STORE)
		log_printf(log, " store");
	else if(pkt->command & CC_CLEAR)
		log_printf(log, " clear");
	log_printf(log, " preset: %d", pkt->preset);
}

/*
 * ccpacket_log_special	Log any special commands in the camera control packet.
 *
 * log: message logger
 */
static inline void ccpacket_log_special(struct ccpacket *pkt, struct log *log) {
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
}

/*
 * ccpacket_log		Log the camera control packet.
 *
 * log: message logger
 */
void ccpacket_log(struct ccpacket *pkt, struct log *log, const char *name) {
	log_line_start(log);
	log_printf(log, "packet: %lld %s rcv: %d", pkt->n_packet++, name,
		pkt->receiver);
	if(pkt->status)
		log_printf(log, " status: %d", pkt->status);
	ccpacket_log_pan(pkt, log);
	ccpacket_log_tilt(pkt, log);
	ccpacket_log_lens(pkt, log);
	if(pkt->aux)
		log_printf(log, " aux: %d", pkt->aux);
	if(pkt->preset)
		ccpacket_log_preset(pkt, log);
	ccpacket_log_special(pkt, log);
	log_line_end(log);
}

/*
 * ccpacket_count	Count the camera control packet statistics.
 */
void ccpacket_count(struct ccpacket *pkt) {
	if(pkt->counter)
		packet_counter_count(pkt->counter, pkt);
}

/*
 * ccpacket_drop	Drop the camera control packet.
 */
void ccpacket_drop(struct ccpacket *pkt) {
	if(pkt->counter) {
		pkt->counter->n_dropped++;
		ccpacket_count(pkt);
	}
}

/*
 * ccpacket_copy	Copy a camera control packet.
 */
void ccpacket_copy(struct ccpacket *dest, struct ccpacket *src) {
	memcpy(dest, src, sizeof(struct ccpacket));
}
