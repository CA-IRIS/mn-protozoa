#include <stdio.h>	/* for printf */
#include <string.h>	/* for strerror */
#include <sys/errno.h>	/* for errno */

#include "sport.h"
#include "config.h"
#include "poller.h"

static const char *CONF_FILE = "protozoa.conf";

extern int errno;

int main(int argc, char* argv[])
{
	int n_ports, i;
	struct sport *port;
	struct poller poll;
	bool debug = false;
	bool verbose = false;

	for(i = 0; i < argc; i++) {
		if(strcmp(argv[i], "--debug") == 0)
			debug = true;
		if(strcmp(argv[i], "--verbose") == 0)
			verbose = true;
	}

	n_ports = config_read(CONF_FILE, &port, verbose);
	if(n_ports <= 0)
		goto fail;
	if(debug)
		config_debug(n_ports, port);
	if(poller_init(&poll, n_ports, port) == NULL)
		goto fail;

	poller_loop(&poll);
fail:
	if(errno)
		printf("Error: %s\n", strerror(errno));
	return errno;
}
