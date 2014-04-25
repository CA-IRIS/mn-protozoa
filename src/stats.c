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

/** Packet stats structure */
static struct ptz_stats stats;

/**
 * Initialize packet stats.
 *
 * @param log		Message logger
 */
void ptz_stats_init(struct log *log) {
	memset(&stats, 0, sizeof(struct ptz_stats));
	stats.log = log;
}

/**
 * Print packet statistics.
 *
 * @param stat		Name of statistic to print
 * @param count		Count of the statistic
 */
static void ptz_stats_print(const char *stat, long long count) {
	float percent = 100 * (float)count / stats.n_packets;
	log_println(stats.log, "%10s: %10lld  %6.2f%%", stat, count, percent);
}

/**
 * Display all packet statistics.
 */
static void ptz_stats_display(void) {
	log_println(stats.log, "protozoa statistics: %lld packets",
		stats.n_packets);
	if (stats.n_dropped)
		ptz_stats_print("dropped", stats.n_dropped);
	if (stats.n_status)
		ptz_stats_print("status", stats.n_status);
	if (stats.n_pan)
		ptz_stats_print("pan", stats.n_pan);
	if (stats.n_tilt)
		ptz_stats_print("tilt", stats.n_tilt);
	if (stats.n_zoom)
		ptz_stats_print("zoom", stats.n_zoom);
	if (stats.n_lens)
		ptz_stats_print("lens", stats.n_lens);
	if (stats.n_aux)
		ptz_stats_print("aux", stats.n_aux);
	if (stats.n_preset)
		ptz_stats_print("preset", stats.n_preset);
}

/**
 * Count one packet in the packet stats.
 *
 * @param pkt		Packet to count
 */
void ptz_stats_count(struct ccpacket *pkt) {
	if (stats.log) {
		stats.n_packets++;
		if (pkt->status)
			stats.n_status++;
		if (pkt->command & (CC_PAN_LEFT | CC_PAN_RIGHT) && pkt->pan)
			stats.n_pan++;
		if (pkt->command & (CC_TILT_UP | CC_TILT_DOWN) && pkt->tilt)
			stats.n_tilt++;
		if (pkt->zoom)
			stats.n_zoom++;
		if (pkt->focus | pkt->iris)
			stats.n_lens++;
		if (pkt->aux)
			stats.n_aux++;
		if (pkt->command & (CC_RECALL | CC_STORE))
			stats.n_preset++;
		if ((stats.n_packets % 100) == 0)
			ptz_stats_display();
	}
}

/**
 * Drop one camera control packet.
 */
void ptz_stats_drop(void) {
	stats.n_dropped++;
}
