#ifndef __MANCHESTER_H__
#define __MANCHESTER_H__

#include "sport.h"

int manchester_do_read(struct handler *h, struct buffer *rxbuf);
int manchester_do_write(struct combiner *c);

#endif
