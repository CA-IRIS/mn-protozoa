#ifndef __VICON_H__
#define __VICON_H__

#include "ccwriter.h"

#define VICON_TIMEOUT (500)
#define VICON_MAX_ADDRESS (255)

void vicon_do_read(struct ccreader *r, struct buffer *rxbuf);
int vicon_encode_speed(int speed);
unsigned int vicon_do_write(struct ccwriter *w, struct ccpacket *p);

#endif
