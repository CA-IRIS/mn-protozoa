/*
 * protozoa -- CCTV transcoder / mixer for PTZ
 * Copyright (C) 2006-2012  Minnesota Department of Transportation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <string.h>	/* for strerror */
#include <unistd.h>	/* for daemon, sleep */
#include <sys/errno.h>	/* for errno */

#include "timer.h"
#include "config.h"
#include "poller.h"

#define VERSION "0.53"
#define BANNER "protozoa: v" VERSION "  Copyright (C) 2006-2012  Mn/DOT"

static const char *LOG_FILE = "/var/log/protozoa";

/** Run the main protozoa loop.
 *
 * Return: errno value on error, or 0 if config file has changed.
 */
static int run_protozoa(struct log *log, bool dryrun) {
	struct packet_counter	*counter;
	struct config		cfg;
	struct poller		poll;
	int			n_channels;
	int			rc = 0;

	log_println(log, BANNER);
	if(log->stats)
		counter = packet_counter_new(log);
	else
		counter = NULL;
	if(config_init(&cfg, log, counter) == NULL) {
		rc = (errno ? errno : -1);
		goto out_0;
	}
	if(config_read(&cfg, CONF_FILE) <= 0) {
		log_println(log, "Check configuration file: %s", CONF_FILE);
		rc = (errno ? errno : -1);
		goto out_1;
	}
	if(dryrun)
		goto out_1;
	if(timer_init() == NULL) {
		rc = (errno ? errno : -1);
		goto out_1;
	}
	n_channels = cfg.n_channels;
	if(poller_init(&poll, n_channels, config_cede_channels(&cfg),
		cfg.defer) == NULL)
	{
		rc = (errno ? errno : -1);
		goto out_2;
	}
	rc = poller_loop(&poll);
	poller_destroy(&poll);
out_2:
	timer_destroy();
out_1:
	config_destroy(&cfg);
out_0:
	free(counter);
	return rc;
}

int main(int argc, char* argv[]) {
	int i;
	int rc = 0;
	struct log log;
	bool daemonize = false;
	bool dryrun = false;

	log_init(&log);
	log_println(&log, "================== protozoa init ===============");
	for(i = 0; i < argc; i++) {
		if(strcmp(argv[i], "--daemonize") == 0)
			daemonize = true;
		if(strcmp(argv[i], "--debug") == 0)
			log.debug = true;
		if(strcmp(argv[i], "--dryrun") == 0)
			dryrun = true;
		if(strcmp(argv[i], "--packet") == 0)
			log.packet = true;
		if(strcmp(argv[i], "--stats") == 0)
			log.stats = true;
	}
	if(daemonize) {
		if(log_open_file(&log, LOG_FILE) == NULL) {
			log_println(&log, "Cannot open: %s", LOG_FILE);
			rc = (errno ? errno : -1);
			goto out;
		}
		if(daemon(0, 0) < 0) {
			rc = (errno ? errno : -1);
			goto out;
		}
	}
	while(true) {
		rc = run_protozoa(&log, dryrun);
		if(dryrun)
			break;
		if(rc > 0)
			log_println(&log, "Error: %s", strerror(rc));
		else if(rc < 0)
			log_println(&log, "Unknown error");
		else
			log_println(&log, CONF_FILE " modified");
		log_println(&log, "** reloading **");
		/* don't chew through CPU */
		sleep(1);
	}
out:
	if(rc > 0)
		log_println(&log, "Error: %s", strerror(rc));
	log_destroy(&log);
	return rc;
}
