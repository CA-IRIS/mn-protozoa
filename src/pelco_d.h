#ifndef __PELCO_D_H__
#define __PELCO_D_H__

#include "sport.h"

int pelco_d_do_read(struct handler *h, struct buffer *rxbuf);
int pelco_d_do_write(struct combiner *c);

#endif
