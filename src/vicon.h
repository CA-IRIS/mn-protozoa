#ifndef __VICON_H__
#define __VICON_H__

#include "sport.h"

int vicon_do_read(struct handler *h, struct buffer *rxbuf);
int vicon_do_write(struct combiner *c);

#endif
