#ifndef __SPORT_H__
#define __SPORT_H__

#include "channel.h"

struct channel* sport_init(struct channel *chn, const char *name, int baud); 

#endif
