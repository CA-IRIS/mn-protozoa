#ifndef __COMBINER_H__
#define __COMBINER_H__

#include "sport.h"

struct combiner {
	struct handler	handler;
	struct buffer	*txbuf;
};

#endif
