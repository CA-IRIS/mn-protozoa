#ifndef __AXIS_H__
#define __AXIS_H__

#include "ccwriter.h"

/* There is no reader for axis protocol (http output only) */
unsigned int axis_do_write(struct ccwriter *w, struct ccpacket *p);

#endif
