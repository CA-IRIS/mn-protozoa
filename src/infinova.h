#ifndef __INFINOVA_D_H__
#define __INFINOVA_D_H__

#include "ccwriter.h"

void infinova_authenticate(struct ccwriter *wtr);
unsigned int infinova_d_do_write(struct ccwriter *wtr, struct ccpacket *pkt);

#endif
