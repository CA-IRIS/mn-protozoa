#ifndef __VICON_H__
#define __VICON_H__

#include "ccwriter.h"

void vicon_do_read(struct handler *h, struct buffer *rxbuf);
unsigned int vicon_do_write(struct ccwriter *w, struct ccpacket *p);

#endif
