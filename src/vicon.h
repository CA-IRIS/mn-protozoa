#ifndef __VICON_H__
#define __VICON_H__

#include "ccwriter.h"

int vicon_do_read(struct handler *h, struct buffer *rxbuf);
int vicon_do_write(struct ccwriter *w, struct ccpacket *p);

#endif
