/*
 * protozoa -- CCTV transcoder / mixer for PTZ
 * Copyright (C) 2006-2007  Minnesota Department of Transportation
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
#include <strings.h>
#include "ccreader.h"
#include "joystick.h"
#include "manchester.h"
#include "pelco_d.h"
#include "vicon.h"

/*
 * ccreader_set_protocol	Set protocol of the camera control reader.
 *
 * protocol: protocol name
 * return: 0 on success; -1 if protocol not found
 */
static int ccreader_set_protocol(struct ccreader *rdr, const char *protocol) {
	if(strcasecmp(protocol, "joystick") == 0)
		rdr->do_read = joystick_do_read;
	else if(strcasecmp(protocol, "manchester") == 0)
		rdr->do_read = manchester_do_read;
	else if(strcasecmp(protocol, "pelco_d") == 0)
		rdr->do_read = pelco_d_do_read;
	else if(strcasecmp(protocol, "vicon") == 0)
		rdr->do_read = vicon_do_read;
	else {
		log_println(rdr->log, "Unknown protocol: %s", protocol);
		return -1;
	}
	return 0;
}

static void ccreader_set_range(struct ccreader *rdr, const char *range) {
	int first, last;

	rdr->range_first = 1;
	rdr->range_last = 1024;

	if(sscanf(range, "%d%d", &first, &last) == 2) {
		rdr->range_first = first;
		rdr->range_last = -last;
	} else if(sscanf(range, "%d", &first) == 1) {
		rdr->range_first = first;
		rdr->range_last = first;
	}
	rdr->packet.receiver = rdr->range_first;
}

void ccreader_previous_camera(struct ccreader *rdr) {
	if(rdr->packet.receiver > rdr->range_first)
		rdr->packet.receiver--;
}

void ccreader_next_camera(struct ccreader *rdr) {
	if(rdr->packet.receiver < rdr->range_last)
		rdr->packet.receiver++;
}

/*
 * ccreader_init	Initialize a new camera control reader.
 *
 * log: message logger
 * protocol: protocol name
 * return: pointer to struct ccreader on success; NULL on failure
 */
static struct ccreader *ccreader_init(struct ccreader *rdr, struct log *log,
	const char *protocol, const char *range)
{
	ccpacket_init(&rdr->packet);
	rdr->writer = NULL;
	rdr->log = log;
	ccreader_set_range(rdr, range);
	if(ccreader_set_protocol(rdr, protocol) < 0)
		return NULL;
	else
		return rdr;
}

/*
 * ccreader_new		Construct a new camera control reader.
 *
 * log: message logger
 * protocol: protocol name
 * range: range of receiver addresses
 * return: pointer to struct ccreader on success; NULL on failure
 */
struct ccreader *ccreader_new(const char *name, struct log *log,
	const char *protocol, const char *range)
{
	struct ccreader *rdr = malloc(sizeof(struct ccreader));
	if(rdr == NULL)
		return NULL;
	if(ccreader_init(rdr, log, protocol, range) == NULL)
		goto fail;
	rdr->name = name;
	return rdr;
fail:
	free(rdr);
	return NULL;
}

/*
 * ccreader_add_writer		Add a writer to the camera control reader.
 *
 * wtr: camera control writer to link with the reader
 */
void ccreader_add_writer(struct ccreader *rdr, struct ccwriter *wtr) {
	wtr->next = rdr->writer;
	rdr->writer = wtr;
}

/*
 * ccreader_do_writers		Write a packet to all linked writers.
 *
 * return: number of writers that wrote the packet
 */
static inline unsigned int ccreader_do_writers(struct ccreader *rdr) {
	unsigned int res = 0;
	struct ccpacket *pkt = &rdr->packet;
	const int receiver = pkt->receiver;	/* save "true" receiver */
	struct ccwriter *wtr = rdr->writer;
	while(wtr) {
		pkt->receiver = ccwriter_get_receiver(wtr, receiver);
		res += wtr->do_write(wtr, pkt);
		wtr = wtr->next;
	}
	pkt->receiver = receiver;	/* restore "true" receiver */
	if(res && rdr->log->packet)
		ccpacket_log(pkt, rdr->log, rdr->name);
	return res;
}

/*
 * ccreader_process_packet_no_clear	Process a packet (but don't clear it)
 *					from the camera control reader.
 *
 * return: number of writers that wrote the packet
 */
unsigned int ccreader_process_packet_no_clear(struct ccreader *rdr) {
	struct ccpacket *pkt = &rdr->packet;
	unsigned int res = 0;
	if(pkt->receiver < rdr->range_first || pkt->receiver > rdr->range_last)
		return 0;	/* Ignore if receiver address is out of range */
	else if(pkt->status)
		ccpacket_drop(pkt);
	else {
		res = ccreader_do_writers(rdr);
		if(res)
			ccpacket_count(pkt);
		else
			ccpacket_drop(pkt);
	}
	return res;
}

/*
 * ccreader_process_packet	Process and clear a packet from the camera
 *				control reader.
 *
 * return: number of writers that wrote the packet
 */
unsigned int ccreader_process_packet(struct ccreader *rdr) {
	unsigned int res = ccreader_process_packet_no_clear(rdr);
	ccpacket_clear(&rdr->packet);
	return res;
}
