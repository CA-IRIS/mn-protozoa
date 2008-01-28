#ifndef __JOYSTICK_H__
#define __JOYSTICK_H__

#include "ccwriter.h"

#define JOYSTICK_TIMEOUT (30000)

void joystick_do_read(struct ccreader *r, struct buffer *rxbuf);

#endif
