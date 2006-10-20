#ifndef __PELCO_D_H__
#define __PELCO_D_H__

#include "ccwriter.h"

void pelco_d_do_read(struct ccreader *r, struct buffer *rxbuf);
unsigned int pelco_d_do_write(struct ccwriter *w, struct ccpacket *p);

#endif
