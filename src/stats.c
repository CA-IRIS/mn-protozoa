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
 * Packet stats struct.
 */
struct ptz_stats {
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
 * Initialize packet stats.
 *
 * @param log		Message logger
 * @return		Pointer to packet counter
 */
static struct ptz_stats *ptz_stats_init(struct ptz_stats *self,
	struct log *log)
{
	memset(self, 0, sizeof(struct ptz_stats));
	self->log = log;
	return self;
}

/**
 * Construct packet stats.
 *
 * @param log		Message logger
 * @return		Pointer to packet stats; NULL or error
 */
struct ptz_stats *ptz_stats_new(struct log *log) {
	struct ptz_stats *self = malloc(sizeof(struct ptz_stats));
	if(self)
		return ptz_stats_init(self, log);
	else
		return NULL;
}

/**
 * Print packet statistics.
 *
 * @param stat		Name of statistic to print
 * @param count		Count of the statistic
 */
static void ptz_stats_print(const struct ptz_stats *self,
	const char *stat, long long count)
{
	float percent = 100 * (float)count / (float)self->n_packets;
	log_println(self->log, "%10s: %10lld  %6.2f%%", stat, count, percent);
}

/**
 * Display all packet statistics.
 */
static void ptz_stats_display(const struct ptz_stats *self) {
	log_println(self->log, "protozoa statistics: %lld packets",
		self->n_packets);
	if(self->n_dropped)
		ptz_stats_print(self, "dropped", self->n_dropped);
	if(self->n_status)
		ptz_stats_print(self, "status", self->n_status);
	if(self->n_pan)
		ptz_stats_print(self, "pan", self->n_pan);
	if(self->n_tilt)
		ptz_stats_print(self, "tilt", self->n_tilt);
	if(self->n_zoom)
		ptz_stats_print(self, "zoom", self->n_zoom);
	if(self->n_lens)
		ptz_stats_print(self, "lens", self->n_lens);
	if(self->n_aux)
		ptz_stats_print(self, "aux", self->n_aux);
	if(self->n_preset)
		ptz_stats_print(self, "preset", self->n_preset);
}

/**
 * Count one packet in the packet stats.
 *
 * @param pkt		Packet to count
 */
void ptz_stats_count(struct ptz_stats *self, struct ccpacket *pkt) {
	self->n_packets++;
	if(pkt->status)
		self->n_status++;
	if(pkt->command & (CC_PAN_LEFT | CC_PAN_RIGHT) && pkt->pan)
		self->n_pan++;
	if(pkt->command & (CC_TILT_UP | CC_TILT_DOWN) && pkt->tilt)
		self->n_tilt++;
	if(pkt->zoom)
		self->n_zoom++;
	if(pkt->focus | pkt->iris)
		self->n_lens++;
	if(pkt->aux)
		self->n_aux++;
	if(pkt->command & (CC_RECALL | CC_STORE))
		self->n_preset++;
	if((self->n_packets % 100) == 0)
		ptz_stats_display(self);
}

/**
 * Drop one camera control packet.
 */
void ptz_stats_drop(struct ptz_stats *self) {
	self->n_dropped++;
}
