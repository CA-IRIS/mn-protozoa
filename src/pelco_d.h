#ifndef __PELCO_D_H__
#define __PELCO_D_H__

#include "ccwriter.h"

#define PELCO_D_TIMEOUT (30000)
#define PELCO_D_MAX_ADDRESS (254)

void pelco_d_do_read(struct ccreader *r, struct buffer *rxbuf);
int pelco_d_encode_speed(int speed);
unsigned int pelco_d_do_write(struct ccwriter *w, struct ccpacket *p);

#endif
