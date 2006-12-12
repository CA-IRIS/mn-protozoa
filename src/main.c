#include <stdio.h>	/* for printf */
#include <string.h>	/* for strerror */
#include <unistd.h>	/* for daemon */
#include <sys/errno.h>	/* for errno */

#include "config.h"
#include "poller.h"

#define VERSION "0.8"

static const char *CONF_FILE = "/etc/protozoa.conf";
static const char *LOG_FILE = "/var/log/protozoa";

extern int errno;

static void print_version() {
	printf("protozoa " VERSION "\n");
	printf("Copyright (C) 2006 Minnesota Department of Transportation\n");
}

int main(int argc, char* argv[]) {
	int n_channels, i;
	struct config conf;
	struct poller poll;
	struct log *log;
	bool daemonize = false;
	bool debug = false;
	bool quiet = false;
	bool stats = false;

	for(i = 0; i < argc; i++) {
		if(strcmp(argv[i], "--daemonize") == 0)
			daemonize = true;
		if(strcmp(argv[i], "--debug") == 0)
			debug = true;
		if(strcmp(argv[i], "--quiet") == 0)
			quiet = true;
		if(strcmp(argv[i], "--stats") == 0)
			stats = true;
		if(strcmp(argv[i], "--version") == 0) {
			print_version();
			exit(0);
		}
	}

	log = malloc(sizeof(struct log));
	if(daemonize)
		log = log_init_file(log, LOG_FILE);
	else
		log = log_init(log);

	log->quiet = quiet;
	log->debug = debug;
	log->stats = stats;

	config_init(&conf, CONF_FILE, log);
	n_channels = config_read(&conf);
	if(n_channels <= 0) {
		fprintf(stderr, "Check configuration file: %s\n", CONF_FILE);
		goto fail;
	}
	if(poller_init(&poll, n_channels, conf.chns) == NULL)
		goto fail;
	if(daemonize) {
		if(daemon(0, 0) < 0)
			goto fail;
	}

	poller_loop(&poll);
fail:
	if(errno)
		fprintf(stderr, "Error: %s\n", strerror(errno));
	return errno;
}
