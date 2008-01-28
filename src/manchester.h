#ifndef __MANCHESTER_H__
#define __MANCHESTER_H__

#include "ccwriter.h"

#define MANCHESTER_TIMEOUT (80)
#define MANCHESTER_MAX_ADDRESS (1024)

void manchester_do_read(struct ccreader *r, struct buffer *rxbuf);
int manchester_encode_speed(int speed);
unsigned int manchester_do_write(struct ccwriter *w, struct ccpacket *p);

#endif
