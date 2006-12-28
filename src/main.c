#include <string.h>	/* for strerror */
#include <unistd.h>	/* for daemon */
#include <sys/errno.h>	/* for errno */

#include "config.h"
#include "poller.h"

#define VERSION "0.10"
#define BANNER "protozoa: v" VERSION "  Copyright (C) 2006  Mn/DOT"

static const char *CONF_FILE = "/etc/protozoa.conf";
static const char *LOG_FILE = "/var/log/protozoa";

extern int errno;

int main(int argc, char* argv[]) {
	int n_channels, i;
	struct config conf;
	struct poller poll;
	struct log log;
	bool daemonize = false;

	log_init(&log);
	for(i = 0; i < argc; i++) {
		if(strcmp(argv[i], "--daemonize") == 0)
			daemonize = true;
		if(strcmp(argv[i], "--debug") == 0)
			log.debug = true;
		if(strcmp(argv[i], "--packet") == 0)
			log.packet = true;
		if(strcmp(argv[i], "--stats") == 0)
			log.stats = true;
	}
	if(daemonize) {
		if(log_open_file(&log, LOG_FILE) == NULL) {
			log_println(&log, "Cannot open: %s", LOG_FILE);
			goto fail;
		}
	}
	log_println(&log, BANNER);
	config_init(&conf, CONF_FILE, &log);
	n_channels = config_read(&conf);
	if(n_channels <= 0) {
		log_println(&log, "Check configuration file: %s", CONF_FILE);
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
		log_println(&log, "Error: %s", strerror(errno));
	log_destroy(&log);
	return errno;
}
