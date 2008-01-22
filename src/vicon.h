#ifndef __VICON_H__
#define __VICON_H__

#include "ccwriter.h"

#define VICON_TIMEOUT (250)

void vicon_do_read(struct ccreader *r, struct buffer *rxbuf);
unsigned int vicon_do_write(struct ccwriter *w, struct ccpacket *p);

#endif
