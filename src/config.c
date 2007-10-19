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
	struct channel *chn = cfg->chns;
	while(chn) {
		struct channel *nchn = chn->next;
		channel_destroy(chn);
		free(chn);
		chn = nchn;
	}
	free(cfg->line);
	memset(cfg, 0, sizeof(struct config));
}

/*
 * config_find_channel	Find a configured channel by name.
 *
 * name: name of channel to find
 * extra: extra channel parameter
 * listen: flag to indicate tcp listening channel
 * return: pointer to channel; or NULL if not found
 */
static struct channel *config_find_channel(struct config *cfg,
	const char *name, int extra, bool listen)
{
	struct channel *chn = cfg->chns;
	while(chn) {
		if(channel_matches(chn, name, extra, listen))
			return chn;
		chn = chn->next;
	}
	return NULL;
}

/*
 * config_new_channel	Create a new channel in the configuration.
 *
 * name: name of the channel
 * extra: extra channel parameter (baud rate or tcp port)
 * listen: flag to indicate whether the channel is a listen channel
 * return: pointer to channel; or NULL on error
 */
static struct channel *config_new_channel(struct config *cfg, const char *name,
	int extra, bool listen)
{
	struct channel *chn = malloc(sizeof(struct channel));
	if(chn == NULL)
		goto fail;
	if(channel_init(chn, name, extra, listen, cfg->log) == NULL)
		goto fail;
	chn->next = cfg->chns;
	cfg->chns = chn;
	cfg->n_channels++;
	return chn;
fail:
	log_println(cfg->log, "config: channel %s init error", name);
	return NULL;
}

/*
 * parse_extra		Parse the extra value after a colon.
 *
 * name: channel name (port:baud or host:port pair)
 * return: parsed value of the suffix after the colon
 */
static int parse_extra(const char *name) {
	int extra = 0;
	char *c = strrchr(name, (int)':');
	if(c)
		sscanf(c + 1, "%d", &extra);
	return extra;
}

/*
 * find_colon		Find a colon in a string.
 */
static inline size_t find_colon(const char *name) {
	char *c = strrchr(name, (int)':');
	if(c)
		return c - name;
	else
		return strlen(name);
}

/*
 * parse_name		Parse the channel name.
 *
 * name: channel name (port:baud or host:port pair)
 * pname: parsed value of the channel name.
 */
static void parse_name(const char *name, char *pname) {
	size_t len = find_colon(name);
	strncpy(pname, name, len);
	pname[len] = '\0';
}

/*
 * name_is_tcp		Test if a channel name is for a tcp address.
 *
 * return: true if channel is a serial port; otherwise false
 */
static inline bool name_is_tcp(const char *name) {
	return name[0] != '/';
}

/*
 * _config_get_channel	Find an existing channel or create a new one.
 *
 * name: name of the channel (device node or hostname)
 * extra: extra parameter (baud rate or tcp port)
 * listen: flag to indicate a tcp listening channel
 * return: pointer to channel; or NULL on error
 */
static struct channel *_config_get_channel(struct config *cfg, const char *name,
	int extra, bool listen)
{
	struct channel *chn = config_find_channel(cfg, name, extra, listen);
	if(chn)
		return chn;
	else
		return config_new_channel(cfg, name, extra, listen);
}

/*
 * config_get_channel	Find an existing channel or create a new one.
 *
 * name: name of the channel (device node or hostname)
 * listen: flag to incidate tcp listening channels
 * return: pointer to channel; or NULL on error
 */
static struct channel *config_get_channel(struct config *cfg, const char *name,
	bool listen)
{
	char pname[32];
	int extra = parse_extra(name);

	parse_name(name, pname);

	if(name_is_tcp(pname))
		return _config_get_channel(cfg, pname, extra, listen);
	else
		return _config_get_channel(cfg, pname, extra, false);
}

/*
 * config_directive	Process one configuration directive.
 *
 * protocol_in: input protocol
 * port_in: input port:baud pair or TCP host:port
 * range: input receiver address range
 * protocol_out: output protocol
 * port_out: output port:baud pair or TCP host:port
 * shift: output receiver address shift offset
 * return: 0 on success; -1 on error
 */
static int config_directive(struct config *cfg, const char *protocol_in,
	const char *port_in, const char *range, const char *protocol_out,
	const char *port_out, const char *shift, const char *auth_out)
{
	struct channel *chn_in, *chn_out;
	struct ccreader *reader;
	struct ccwriter *writer;

	log_println(cfg->log, "config: %s %s %s -> %s %s %s", protocol_in,
		port_in, range, protocol_out, port_out, shift);
	chn_in = config_get_channel(cfg, port_in, true);
	if(chn_in == NULL)
		goto fail;
	if(chn_in->reader == NULL) {
		reader = ccreader_new(chn_in->name, cfg->log, protocol_in);
		chn_in->reader = reader;
		reader->packet.counter = cfg->counter;
	} else {
		/* FIXME: check for redefined protocol */
		reader = chn_in->reader;
	}
	chn_out = config_get_channel(cfg, port_out, false);
	if(chn_out == NULL)
		goto fail;
	writer = ccwriter_new(chn_out, protocol_out, auth_out);
	if(writer == NULL)
		goto fail;
	ccreader_add_writer(reader, writer, range, shift);
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
	char range[8], shift[8];
	char auth_out[32];

	shift[0] = '\0';
	auth_out[0] = '\0';

	i = sscanf(cfg->line, "%15s %31s %7s %15s %31s %7s %31s", protocol_in,
		port_in, range, protocol_out, port_out, shift, auth_out);
	if(i == 5)
		strcpy(shift, "0");
	if(i >= 5)
		return config_directive(cfg, protocol_in, port_in, range,
			protocol_out, port_out, shift, auth_out);
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
 * return: ceded pointer to channel list
 */
struct channel *config_cede_channels(struct config *cfg) {
	struct channel *chn = cfg->chns;
	cfg->n_channels = 0;
	cfg->chns = NULL;
	return chn;
}
