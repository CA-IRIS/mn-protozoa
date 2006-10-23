#ifndef __TCP_H__
#define __TCP_H__

#include "channel.h"

struct channel* tcp_init(struct channel *chn, const char *name, int port);

#endif
