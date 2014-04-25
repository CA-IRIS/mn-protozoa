/*
 * protozoa -- CCTV transcoder / mixer for PTZ
 * Copyright (C) 2006-2014  Minnesota Department of Transportation
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
#include "stats.h"

/*
 * Packet counter struct.
 */
struct packet_counter {
	struct log	*log;		/* logger */
	long long	n_packets;	/* total count of packets */
	long long	n_dropped;	/* count of dropped packets */
	long long	n_status;	/* count of status packets */
	long long	n_pan;		/* count of pan packets */
	long long	n_tilt;		/* count of tilt packets */
	long long	n_zoom;		/* count of zoom packets */
	long long	n_lens;		/* count of lens packets */
	long long	n_aux;		/* count of aux packets */
	long long	n_preset;	/* count of preset packets */
};

/**
 * Initialize a new packet counter.
 *
 * @param log		Message logger
 * @return		Pointer to packet counter
 */
struct packet_counter *packet_counter_init(struct packet_counter *cnt,
	struct log *log)
{
	memset(cnt, 0, sizeof(struct packet_counter));
	cnt->log = log;
	return cnt;
}

/**
 * Construct a new packet counter.
 *
 * @param log		Message logger
 * @return		Pointer to packet counter; NULL or error
 */
struct packet_counter *packet_counter_new(struct log *log) {
	struct packet_counter *cnt = malloc(sizeof(struct packet_counter));
	if(cnt == NULL)
		return NULL;
	return packet_counter_init(cnt, log);
}

/**
 * Print packet counter statistics.
 *
 * @param stat		Name of statistic to print
 * @param count		Count of the statistic
 */
static void packet_counter_print(const struct packet_counter *cnt,
	const char *stat, long long count)
{
	float percent = 100 * (float)count / (float)cnt->n_packets;
	log_println(cnt->log, "%10s: %10lld  %6.2f%%", stat, count, percent);
}

/**
 * Display all packet counter statistics.
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

/**
 * Count one packet in the packet counter.
 *
 * @param pkt		Packet to count
 */
void packet_counter_count(struct packet_counter *cnt, struct ccpacket *pkt) {
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

/**
 * Drop one camera control packet.
 */
void packet_counter_drop(struct packet_counter *cnt) {
	cnt->n_dropped++;
}
