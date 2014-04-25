#ifndef __STATS_H__
#define __STATS_H__

#include "log.h"
#include "ccpacket.h"

struct ptz_stats *ptz_stats_new(struct log *log);
void ptz_stats_count(struct ptz_stats *self, struct ccpacket *pkt);
void ptz_stats_drop(struct ptz_stats *self);

#endif
