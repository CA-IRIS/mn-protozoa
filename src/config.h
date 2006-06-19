#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "sport.h"

int config_read(const char *filename, struct sport *ports[]);
void config_debug(int n_ports, struct sport *ports);

#endif
