#ifndef __STATS_H__
#define __STATS_H__

#include "log.h"
#include "ccpacket.h"

void ptz_stats_init(struct log *log);
void ptz_stats_count(struct ccpacket *pkt);
void ptz_stats_drop(void);

#endif
