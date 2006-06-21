#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "sport.h"

struct config {
	const char	*filename;
	struct sport	*ports;
	int		n_ports;
	bool		verbose;
};

void config_init(struct config *c, const char *filename, bool verbose);
int config_read(const char *filename, struct sport *ports[], bool verbose);
void config_debug(int n_ports, struct sport *ports);

#endif
