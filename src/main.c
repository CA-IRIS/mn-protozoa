#include <string.h>	/* for strerror */
#include <unistd.h>	/* for daemon */
#include <sys/errno.h>	/* for errno */

#include "config.h"
#include "poller.h"

#define VERSION "0.10"
#define BANNER "protozoa: v" VERSION "  Copyright (C) 2006,2007  Mn/DOT"

static const char *CONF_FILE = "/etc/protozoa.conf";
static const char *LOG_FILE = "/var/log/protozoa";

extern int errno;

struct poller *create_poller(struct log *log) {
	int n_channels;
	struct config cfg;
	struct poller *poll;

	poll = malloc(sizeof(struct poller));
	if(poll == NULL)
		goto fail;
	if(config_init(&cfg, log) == NULL)
		goto fail;
	n_channels = config_read(&cfg, CONF_FILE);
	if(n_channels <= 0) {
		log_println(log, "Check configuration file: %s", CONF_FILE);
		goto fail_0;
	}
	if(poller_init(poll, n_channels, config_take_channels(&cfg)) == NULL)
		goto fail_0;
	config_destroy(&cfg);
	return poll;
fail_0:
	config_destroy(&cfg);
fail:
	free(poll);
	return NULL;
}

int main(int argc, char* argv[]) {
	int i;
	struct poller *poll;
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
	poll = create_poller(&log);
	if(poll == NULL)
		goto fail;
	if(daemonize) {
		if(daemon(0, 0) < 0)
			goto fail;
	}
	poller_loop(poll);
fail:
	if(errno)
		log_println(&log, "Error: %s", strerror(errno));
	log_destroy(&log);
	return errno;
}
