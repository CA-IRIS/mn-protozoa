/*
 * protozoa -- CCTV transcoder / mixer for PTZ
 * Copyright (C) 2006-2008  Minnesota Department of Transportation
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

static void ccreader_set_timeout(struct ccreader *rdr, unsigned int timeout) {
	rdr->timeout = timeout;
}

/*
 * ccreader_set_protocol	Set protocol of the camera control reader.
 *
 * protocol: protocol name
 * return: 0 on success; -1 if protocol not found
 */
static int ccreader_set_protocol(struct ccreader *rdr, const char *protocol) {
	if(strcasecmp(protocol, "joystick") == 0) {
		rdr->do_read = joystick_do_read;
		ccreader_set_timeout(rdr, JOYSTICK_TIMEOUT);
	} else if(strcasecmp(protocol, "manchester") == 0) {
		rdr->do_read = manchester_do_read;
		ccreader_set_timeout(rdr, MANCHESTER_TIMEOUT);
	} else if(strcasecmp(protocol, "pelco_d") == 0) {
		rdr->do_read = pelco_d_do_read;
		ccreader_set_timeout(rdr, PELCO_D_TIMEOUT);
	} else if(strcasecmp(protocol, "vicon") == 0) {
		rdr->do_read = vicon_do_read;
		ccreader_set_timeout(rdr, VICON_TIMEOUT);
	} else {
		log_println(rdr->log, "Unknown protocol: %s", protocol);
		return -1;
	}
	return 0;
}

static struct ccnode *ccnode_new() {
	return malloc(sizeof(struct ccnode));
}

static struct ccnode *ccnode_init(struct ccnode *node) {
	node->writer = NULL;
	node->range_first = 1;
	node->range_last = 1024;
	node->shift = 0;
	node->next = NULL;
	return node;
}

static void ccnode_set_range(struct ccnode *node, const char *range) {
	int first, last;

	if(sscanf(range, "%d%d", &first, &last) == 2) {
		node->range_first = first;
		node->range_last = -last;
	} else if(sscanf(range, "%d", &first) == 1) {
		node->range_first = first;
		node->range_last = first;
	}
}  

static void ccnode_set_shift(struct ccnode *node, const char *shift) {
	sscanf(shift, "%d", &node->shift);
}

void ccreader_previous_camera(struct ccreader *rdr) {
	if(rdr->packet.receiver > 0)
		rdr->packet.receiver--;
}

void ccreader_next_camera(struct ccreader *rdr) {
	if(rdr->packet.receiver < 1024)
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
	const char *protocol)
{
	ccpacket_init(&rdr->packet);
	rdr->timeout = DEFAULT_TIMEOUT;
	rdr->head = NULL;
	rdr->log = log;
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
 * return: pointer to struct ccreader on success; NULL on failure
 */
struct ccreader *ccreader_new(const char *name, struct log *log,
	const char *protocol)
{
	struct ccreader *rdr = malloc(sizeof(struct ccreader));
	if(rdr == NULL)
		return NULL;
	if(ccreader_init(rdr, log, protocol) == NULL)
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
 * range: range of receiver addresses
 * shift: receiver address shift offset
 */
void ccreader_add_writer(struct ccreader *rdr, struct ccwriter *wtr,
	const char *range, const char *shift)
{
	struct ccnode *node = ccnode_new();
	if(node == NULL)
		return;
	if(ccnode_init(node) == NULL)
		return;
	node->writer = wtr;
	ccnode_set_range(node, range);
	ccnode_set_shift(node, shift);
	node->next = rdr->head;
	rdr->head = node;
	rdr->packet.receiver = node->range_first;
}

/*
 * ccnode_get_receiver	Get receiver address adjusted for the node.
 *
 * receiver: input receiver address
 * return: output receiver address; 0 indicates drop packet
 */
static int ccnode_get_receiver(const struct ccnode *node, int receiver) {
	if(receiver < node->range_first || receiver > node->range_last)
		return 0;	/* Ignore if receiver address is out of range */
	receiver += node->shift;
	if(receiver < 0)
		return 0;
	else
		return receiver;
}

/*
 * ccreader_do_writers		Write a packet to all linked writers.
 *
 * return: number of writers that wrote the packet
 */
static unsigned int ccreader_do_writers(struct ccreader *rdr) {
	unsigned int res = 0;
	struct ccpacket *pkt = &rdr->packet;
	const int receiver = pkt->receiver;	/* save "true" receiver */
	struct ccnode *node = rdr->head;
	while(node) {
		pkt->receiver = ccnode_get_receiver(node, receiver);
		if(pkt->receiver) {
			struct ccwriter *wtr = node->writer;
			res += wtr->do_write(wtr, pkt);
		}
		node = node->next;
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
	if(pkt->status)
		ccpacket_drop(pkt);
	else {
		ccpacket_set_timeout(pkt, rdr->timeout);
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
