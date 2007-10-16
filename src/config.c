/*
 * protozoa -- CCTV transcoder / mixer for PTZ
 * Copyright (C) 2006-2007  Minnesota Department of Transportation
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
#include <string.h>		/* for memset */
#include "config.h"
#include "ccreader.h"
#include "ccwriter.h"

/*
 * config_init		Initialize a new configuration reader.
 *
 * log: message logger (borrowed pointer)
 * cnt: packet counter (borrowed pointer)
 * return: pointer to struct config or NULL on error
 */
struct config *config_init(struct config *cfg, struct log *log,
	struct packet_counter *cnt)
{
	memset(cfg, 0, sizeof(struct config));
	cfg->line = malloc(LINE_LENGTH);
	if(cfg->line == NULL)
		return NULL;
	cfg->log = log;
	cfg->counter = cnt;
	return cfg;
}

/*
 * config_destroy	Destroy a previously initialized config.
 */
void config_destroy(struct config *cfg) {
	int i;
	for(i = 0; i < cfg->n_channels; i++)
		channel_destroy(cfg->chns + i);
	free(cfg->chns);
	free(cfg->line);
	memset(cfg, 0, sizeof(struct config));
}

/*
 * config_find_channel	Find a configured channel by name.
 *
 * name: name of channel to find
 * return: pointer to channel; or NULL if not found
 */
static struct channel *config_find_channel(struct config *cfg,
	const char *name)
{
	int i;
	for(i = 0; i < cfg->n_channels; i++) {
		if(strcmp(name, cfg->chns[i].name) == 0)
			return cfg->chns + i;
	}
	return NULL;
}

/*
 * config_new_channel	Create a new channel in the configuration.
 *
 * name: name of the channel
 * extra: extra channel parameter (baud rate or tcp port)
 * return: pointer to channel; or NULL on error
 */
static struct channel *config_new_channel(struct config *cfg, const char *name,
	int extra)
{
	struct channel *chn, *chns;

	chns = realloc(cfg->chns, sizeof(struct channel) * (cfg->n_channels+1));
	if(chns == NULL)
		goto fail;
	chn = chns + cfg->n_channels;
	if(channel_init(chn, name, extra, cfg->log) == NULL)
		goto fail;
	cfg->n_channels++;
	cfg->chns = chns;
	return chn;
fail:
	log_println(cfg->log, "config: channel %s init error", name);
	return NULL;
}

/*
 * split_colon		Chop off trailing colon suffix and return parsed value
 *
 * name: channel name (port:baud or host:port pair)
 * return: parsed value of the suffix after the colon
 */
static int split_colon(char *name) {
	char *c = rindex(name, (int)':');
	if(c != NULL) {
		int extra = 0;
		*c = '\0';
		sscanf(c + 1, "%d", &extra);
		return extra;
	} else
		return 0;	
}

/*
 * config_get_channel	Find an existing channel or create a new one.
 *
 * name: name of the channel (port:baud or host:port)
 * return: pointer to channel; or NULL on error
 */
static struct channel *config_get_channel(struct config *cfg, char *name) {
	struct channel *chn;
	int extra = split_colon(name);

	chn = config_find_channel(cfg, name);
	if(chn)
		return chn;
	else
		return config_new_channel(cfg, name, extra);
}

/*
 * config_directive	Process one configuration directive.
 *
 * protocol_in: input protocol
 * port_in: input port:baud pair or TCP host:port
 * range_in: input receiver address range
 * protocol_out: output protocol
 * port_out: output port:baud pair or TCP host:port
 * shift_out: output receiver address shift offset
 * return: 0 on success; -1 on error
 */
static int config_directive(struct config *cfg, const char *protocol_in,
	char *port_in, const char *range_in, const char *protocol_out,
	char *port_out, const char *shift_out)
{
	struct channel *chn_in, *chn_out;
	struct ccreader *reader;
	struct ccwriter *writer;

	log_println(cfg->log, "config: %s %s %s -> %s %s %s", protocol_in,
		port_in, range_in, protocol_out, port_out, shift_out);
	chn_in = config_get_channel(cfg, port_in);
	if(chn_in == NULL)
		goto fail;
	if(chn_in->reader == NULL) {
		reader = ccreader_new(chn_in->name, cfg->log, protocol_in,
			range_in);
		chn_in->reader = reader;
		reader->packet.counter = cfg->counter;
	} else {
		/* FIXME: check for redefined protocol */
		reader = chn_in->reader;
	}
	chn_out = config_get_channel(cfg, port_out);
	if(chn_out == NULL)
		goto fail;
	writer = ccwriter_new(chn_out, protocol_out, shift_out);
	if(writer == NULL)
		goto fail;
	ccreader_add_writer(reader, writer);
	return 0;
fail:
	return -1;
}

/*
 * config_skip_comments		Remove comments from the line being parsed.
 */
static void config_skip_comments(struct config *cfg) {
	int i;
	for(i = 0; i < LINE_LENGTH; i++) {
		if(cfg->line[i] == '#')
			cfg->line[i] = '\0';
	}
}

/*
 * config_scan_directive	Parse one directive in the configuration.
 *
 * return: 0 on success; -1 on error
 */
static int config_scan_directive(struct config *cfg) {
	int i;
	char protocol_in[16], protocol_out[16];
	char port_in[32], port_out[32];
	char range[8], shift[32];

	i = sscanf(cfg->line, "%15s %31s %8s %15s %31s %31s", protocol_in,
		port_in, range, protocol_out, port_out, shift);
	if(i == 5)
		strcpy(shift, "0");
	if(i >= 5)
		return config_directive(cfg, protocol_in, port_in, range,
			protocol_out, port_out, shift);
	else if(i <= 0)
		return 0;
	else {
		log_println(cfg->log, "Invalid directive: %s", cfg->line);
		return -1;
	}
}

/*
 * config_read		Read the configuration file.
 *
 * filename: name of the configuration file
 * return: number of channels created by the configuration
 */
int config_read(struct config *cfg, const char *filename) {
	FILE *f = fopen(filename, "r");
	if(f == NULL)
		return -1;

	while(fgets(cfg->line, LINE_LENGTH, f) == cfg->line) {
		config_skip_comments(cfg);
		if(config_scan_directive(cfg))
			goto fail;
	}
	if(cfg->n_channels == 0) {
		log_println(cfg->log, "Error reading configuration file: %s",
			filename);
	}
	fclose(f);
	return cfg->n_channels;
fail:
	fclose(f);
	return -1;
}

/*
 * config_cede_channels		Cede ownership of channel array memory.
 *
 * return: ceded pointer to channel array
 */
struct channel *config_cede_channels(struct config *cfg) {
	struct channel *chns = cfg->chns;
	cfg->n_channels = 0;
	cfg->chns = NULL;
	return chns;
}
