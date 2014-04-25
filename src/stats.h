#ifndef __STATS_H__
#define __STATS_H__

#include "log.h"
#include "ccpacket.h"

struct packet_counter *packet_counter_init(struct packet_counter *cnt,
	struct log *log);
struct packet_counter *packet_counter_new(struct log *log);
void packet_counter_count(struct packet_counter *cnt, struct ccpacket *pkt);
void packet_counter_drop(struct packet_counter *cnt);

#endif
