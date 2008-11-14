#ifndef __MANCHESTER_H__
#define __MANCHESTER_H__

#include "ccwriter.h"

#define MANCHESTER_GAPTIME (10)
#define MANCHESTER_TIMEOUT (80)
#define MANCHESTER_MAX_ADDRESS (1024)

void manchester_do_read(struct ccreader *r, struct buffer *rxbuf);
unsigned int manchester_do_write(struct ccwriter *w, struct ccpacket *p);

#endif
