#include <stdio.h>	/* for printf */
#include <string.h>	/* for strerror */
#include <unistd.h>	/* for daemon */
#include <sys/errno.h>	/* for errno */

#include "config.h"
#include "poller.h"

#define VERSION "0.7"

static const char *CONF_FILE = "/etc/protozoa.conf";

extern int errno;

static void print_version() {
	printf("protozoa " VERSION "\n");
	printf("Copyright (C) 2006 Minnesota Department of Transportation\n");
}

int main(int argc, char* argv[])
{
	int n_channels, i;
	struct config conf;
	struct poller poll;
	bool debug = false;
	bool verbose = false;
	bool stats = false;

	for(i = 0; i < argc; i++) {
		if(strcmp(argv[i], "--debug") == 0)
			debug = true;
		if(strcmp(argv[i], "--verbose") == 0)
			verbose = true;
		if(strcmp(argv[i], "--stats") == 0)
			stats = true;
		if(strcmp(argv[i], "--version") == 0) {
			print_version();
			exit(0);
		}
	}

	config_init(&conf, CONF_FILE, verbose, debug, stats);
	n_channels = config_read(&conf);
	if(n_channels <= 0) {
		fprintf(stderr, "Check configuration file: %s\n", CONF_FILE);
		goto fail;
	}
	if(poller_init(&poll, n_channels, conf.chns) == NULL)
		goto fail;
	if(!(debug || verbose || stats)) {
		if(daemon(0, 0) < 0)
			goto fail;
	}

	poller_loop(&poll);
fail:
	if(errno)
		fprintf(stderr, "Error: %s\n", strerror(errno));
	return errno;
}
