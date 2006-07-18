#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "sport.h"
#include "ccpacket.h"

#define LINE_LENGTH (80)

struct config {
	const char	*filename;
	char		*line;
	struct sport	*ports;
	int		n_ports;
	bool		verbose;
	bool		debug;
	struct combiner *out;		/* most recent OUT combiner */
	struct packet_counter *counter;
};

void config_init(struct config *c, const char *filename, bool verbose,
	bool debug, bool stats);
int config_read(struct config *c);

#endif
