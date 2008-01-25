/*
 * protozoa -- CCTV transcoder / mixer for PTZ
 * Copyright (C) 2008  Minnesota Department of Transportation
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
#include "timer.h"	/* for timer_init */
#include "timeval.h"
#include "defer.h"

static cl_compare_t compare_pkts(const void *value0, const void *value1) {
	const struct deferred_pkt *dpkt0 = value0;
	const struct deferred_pkt *dpkt1 = value1;

	if(dpkt0->tv.tv_sec < dpkt1->tv.tv_sec)
		return CL_LESS;
	if(dpkt0->tv.tv_sec > dpkt1->tv.tv_sec)
		return CL_GREATER;
	if(dpkt0->tv.tv_usec < dpkt1->tv.tv_usec)
		return CL_LESS;
	if(dpkt0->tv.tv_usec > dpkt1->tv.tv_usec)
		return CL_GREATER;
	return CL_EQUAL;
}

struct defer *defer_init(struct defer *dfr) {
	if(cl_pool_init(&dfr->pool, sizeof(struct deferred_pkt)) == NULL)
		return NULL;
	if(cl_rbtree_init(&dfr->tree, CL_DUP_ALLOW, compare_pkts) == NULL)
		goto fail;
	if(timer_init() == NULL)
		goto fail2;
	return dfr;
fail2:
	cl_rbtree_clear(&dfr->tree, NULL, NULL);
fail:
	cl_pool_destroy(&dfr->pool);
	return NULL;
}


static int defer_rearm(struct defer *dfr) {
	struct deferred_pkt *dpkt = cl_rbtree_peek(&dfr->tree);
	if(dpkt)
		return timer_arm(time_from_now(&dpkt->tv));
	else
		return timer_disarm();
}

/*
 * defer_packet		Defer one packet to be sent at a later time.
 */
int defer_packet(struct defer *dfr, struct ccpacket *pkt,
	struct ccwriter *wtr)
{
	struct deferred_pkt *dpkt = cl_pool_alloc(&dfr->pool);

	if(dpkt == NULL)
		return -1;
	timeval_set_timeout(&dpkt->tv, wtr->timeout);
	dpkt->packet = pkt;
	dpkt->writer = wtr;
	if(cl_rbtree_add(&dfr->tree, dpkt) == NULL)
		return -1;
	return defer_rearm(dfr);
}

static bool deferred_pkt_again(struct deferred_pkt *dpkt) {
	int timeout = dpkt->writer->timeout;

	return (time_elapsed(&dpkt->packet->sent, &dpkt->tv) >= timeout) &&
		(time_elapsed(&dpkt->tv, &dpkt->packet->expire) >= timeout);
}

static void defer_packet_now(struct defer *dfr, struct deferred_pkt *dpkt) {
	int timeout = dpkt->writer->timeout;

	cl_rbtree_remove(&dfr->tree, dpkt);
	if(deferred_pkt_again(dpkt)) {
		ccwriter_do_write(dpkt->writer, dpkt->packet);
		gettimeofday(&dpkt->packet->sent, NULL);
		timeval_set_timeout(&dpkt->tv, timeout);
	}
	if(deferred_pkt_again(dpkt))
		cl_rbtree_add(&dfr->tree, dpkt);
	else
		cl_pool_release(&dfr->pool, dpkt);
}

/*
 * defer_next		Process the next deferred packet
 */
int defer_next(struct defer *dfr) {
	struct deferred_pkt *dpkt;

	if(timer_read() < 0)
		return -1;

	dpkt = cl_rbtree_peek(&dfr->tree);
	if(dpkt)
		defer_packet_now(dfr, dpkt);
	return defer_rearm(dfr);
}

int defer_get_fd(struct defer *dfr) {
	return timer_get_fd();
}
