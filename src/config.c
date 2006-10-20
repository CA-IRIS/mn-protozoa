#include <stdio.h>
#include <string.h>
#include "config.h"
#include "ccreader.h"
#include "ccwriter.h"

void config_init(struct config *c, const char *filename, bool verbose,
	bool debug, bool stats)
{
	c->filename = filename;
	c->line = malloc(LINE_LENGTH);
	c->ports = NULL;
	c->n_ports = 0;
	c->verbose = verbose;
	c->debug = debug;
	c->out = NULL;
	if(stats) {
		c->counter = malloc(sizeof(struct packet_counter));
		counter_init(c->counter);
	} else
		c->counter = NULL;
}

static struct sport *config_find_port(struct config *c, const char *port) {
	int i;
	for(i = 0; i < c->n_ports; i++) {
		if(strcmp(port, c->ports[i].name) == 0)
			return c->ports + i;
	}
	return NULL;
}

static struct sport *config_new_port(struct config *c, const char *port,
	int baud)
{
	struct sport *prt;
	c->n_ports++;
	c->ports = realloc(c->ports, sizeof(struct sport) * c->n_ports);
	if(c->ports == NULL)
		return NULL;
	prt = c->ports + (c->n_ports - 1);
	if(sport_init(prt, port, baud) == NULL) {
		fprintf(stderr, "Error initializing serial port: %s\n",
			port);
		return NULL;
	}
	prt->rxbuf->debug = c->debug;
	prt->txbuf->debug = c->debug;
	return prt;
}

static struct sport *config_get_port(struct config *c, const char *port,
	int baud)
{
	struct sport *prt;

	prt = config_find_port(c, port);
	if(prt) {
		if(prt->baud != baud) {
			fprintf(stderr, "Baud rate redefined for %s\n", port);
			return NULL;
		} else
			return prt;
	} else
		return config_new_port(c, port, baud);
}

static int config_directive(struct config *c, const char *protocol_in,
	const char *port_in, int baud_in, const char *protocol_out,
	const char *port_out, int baud_out, int base)
{
	struct sport *prt_in, *prt_out;
	struct ccreader *reader;
	struct ccwriter *writer;

	if(c->verbose) {
		printf("protozoa: %s %s %d %s %s %d %d\n", protocol_in, port_in,
			baud_in, protocol_out, port_out, baud_out, base);
	}
	prt_in = config_get_port(c, port_in, baud_in);
	if(prt_in == NULL)
		goto fail;
	if(prt_in->handler == NULL) {
		reader = ccreader_create(prt_in, protocol_in, c->verbose);
		reader->packet.counter = c->counter;
	} else {
		// FIXME: check for redefined protocol
		reader = (struct ccreader *)prt_in->handler;
	}
	prt_out = config_get_port(c, port_out, baud_out);
	if(prt_out == NULL)
		goto fail;
	writer = ccwriter_create(prt_out, protocol_out, base);
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
	if(c->n_ports == 0) {
		fprintf(stderr, "Error reading configuration file: %s\n",
			c->filename);
	}
	fclose(f);
	return c->n_ports;
fail:
	fclose(f);
	return -1;
}
