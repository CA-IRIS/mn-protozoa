#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "channel.h"
#include "ccpacket.h"

#define LINE_LENGTH (80)

struct config {
	const char	*filename;
	char		*line;
	struct channel	*chns;
	int		n_channels;
	struct log	*log;
	struct combiner *out;		/* most recent OUT combiner */
	struct packet_counter *counter;
};

void config_init(struct config *cfg, const char *filename, struct log *log);
void config_destroy(struct config *cfg);
int config_read(struct config *cfg);

#endif
