#ifndef __PELCO_D_H__
#define __PELCO_D_H__

#include "ccwriter.h"

int pelco_d_do_read(struct handler *h, struct buffer *rxbuf);
int pelco_d_do_write(struct ccwriter *w, struct ccpacket *p);

#endif
