#ifndef __MANCHESTER_H__
#define __MANCHESTER_H__

#include "ccwriter.h"

void manchester_do_read(struct ccreader *r, struct buffer *rxbuf);
unsigned int manchester_do_write(struct ccwriter *w, struct ccpacket *p);

#endif
