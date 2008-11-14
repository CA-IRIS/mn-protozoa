#ifndef __AXIS_H__
#define __AXIS_H__

#include "ccwriter.h"

#define AXIS_GAPTIME (100)
#define AXIS_TIMEOUT (30000)
#define AXIS_MAX_ADDRESS (1)

/* There is no reader for axis protocol (http output only) */
int axis_encode_speed(int speed);
unsigned int axis_do_write(struct ccwriter *w, struct ccpacket *p);

#endif
