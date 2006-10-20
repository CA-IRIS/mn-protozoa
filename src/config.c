#include <stdio.h>
#include <string.h>
#include "config.h"
#include "ccreader.h"
#include "ccwriter.h"
#include "sport.h"

void config_init(struct config *c, const char *filename, bool verbose,
	bool debug, bool stats)
{
	c->filename = filename;
	c->line = malloc(LINE_LENGTH);
	c->chns = NULL;
	c->n_channels = 0;
	c->verbose = verbose;
	c->debug = debug;
	c->out = NULL;
	if(stats) {
		c->counter = malloc(sizeof(struct packet_counter));
		counter_init(c->counter);
	} else
		c->counter = NULL;
}

static struct channel *config_find_channel(struct config *c, const char *name) {
	int i;
	for(i = 0; i < c->n_channels; i++) {
		if(strcmp(name, c->chns[i].name) == 0)
			return c->chns + i;
	}
	return NULL;
}

static struct channel *config_new_channel(struct config *c, const char *name,
	int baud)
{
	struct channel *chn;
	c->n_channels++;
	c->chns = realloc(c->chns, sizeof(struct channel) * c->n_channels);
	if(c->chns == NULL)
		return NULL;
	chn = c->chns + (c->n_channels - 1);
	if(sport_init(chn, name, baud) == NULL) {
		fprintf(stderr, "Error initializing serial port: %s\n",
			name);
		return NULL;
	}
	chn->rxbuf->debug = c->debug;
	chn->txbuf->debug = c->debug;
	return chn;
}

static struct channel *config_get_channel(struct config *c, const char *name,
	int baud)
{
	struct channel *chn;

	chn = config_find_channel(c, name);
	if(chn)
		return chn;
	else
		return config_new_channel(c, name, baud);
}

static int config_directive(struct config *c, const char *protocol_in,
	const char *port_in, int baud_in, const char *protocol_out,
	const char *port_out, int baud_out, int base)
{
	struct channel *chn_in, *chn_out;
	struct ccreader *reader;
	struct ccwriter *writer;

	if(c->verbose) {
		printf("protozoa: %s %s %d %s %s %d %d\n", protocol_in, port_in,
			baud_in, protocol_out, port_out, baud_out, base);
	}
	chn_in = config_get_channel(c, port_in, baud_in);
	if(chn_in == NULL)
		goto fail;
	if(chn_in->reader == NULL) {
		reader = ccreader_create(chn_in->name, protocol_in, c->verbose);
		chn_in->reader = reader;
		reader->packet.counter = c->counter;
	} else {
		// FIXME: check for redefined protocol
		reader = chn_in->reader;
	}
	chn_out = config_get_channel(c, port_out, baud_out);
	if(chn_out == NULL)
		goto fail;
	writer = ccwriter_create(chn_out, protocol_out, base);
	if(writer == NULL)
		goto fail;
	ccreader_add_writer(reader, writer);
	return 0;
fail:
	return -1;
}

static void config_skip_comments(struct config *c) {
	int i;
	for(i = 0; i < LINE_LENGTH; i++) {
		if(c->line[i] == '#')
			c->line[i] = '\0';
	}
}

static int config_scan_directive(struct config *c) {
	int i;
	char protocol_in[16], protocol_out[16];
	char port_in[16], port_out[16];
	int baud_in = 9600;
	int baud_out = 9600;
	int base = 0;
	i = sscanf(c->line, "%15s %15s %d %15s %15s %d %d", protocol_in,
		port_in, &baud_in, protocol_out, port_out, &baud_out, &base);
	if(i == 5)
		baud_out = baud_in;
	if(i > 4)
		return config_directive(c, protocol_in, port_in, baud_in,
			protocol_out, port_out, baud_out, base);
	else if(i <= 0)
		return 0;
	else {
		fprintf(stderr, "Invalid directive: %s\n", c->line);
		return -1;
	}
}

int config_read(struct config *c) {
	FILE *f = fopen(c->filename, "r");
	if(f == NULL)
		return -1;

	while(fgets(c->line, LINE_LENGTH, f) == c->line) {
		config_skip_comments(c);
		if(config_scan_directive(c))
			goto fail;
	}
	if(c->n_channels == 0) {
		fprintf(stderr, "Error reading configuration file: %s\n",
			c->filename);
	}
	fclose(f);
	return c->n_channels;
fail:
	fclose(f);
	return -1;
}
