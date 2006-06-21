#include <stdio.h>
#include <string.h>
#include "config.h"

void config_init(struct config *c, const char *filename, bool verbose,
	bool debug)
{
	c->filename = filename;
	c->line = malloc(LINE_LENGTH);
	c->ports = NULL;
	c->n_ports = 0;
	c->verbose = verbose;
	c->debug = debug;
	c->out = NULL;
}

static struct sport *config_find_port(struct config *c, const char *port) {
	int i;
	for(i = 0; i < c->n_ports; i++) {
		if(strcmp(port, c->ports[i].name) == 0)
			return c->ports + i;
	}
	return NULL;
}

static int config_set_inbound(struct sport *prt, const char *protocol) {
	struct combiner *cmbnr = (struct combiner *)prt->handler;
	if(cmbnr == NULL)
		return -1;
	return combiner_set_input_protocol(cmbnr, protocol);
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

static struct combiner *config_new_combiner(struct config *c, struct sport *prt,
	const char *in_out, const char *protocol)
{
	if(strcasecmp(in_out, "OUT") == 0) {
		c->out = combiner_create_outbound(prt, protocol, c->verbose);
		return c->out;
	} else if(strcasecmp(in_out, "IN") == 0)
		return combiner_create_inbound(prt, protocol, c->out);
	else
		return NULL;
}

static int config_directive(struct config *c, const char *in_out,
	const char *protocol, const char *port, int baud)
{
	struct sport *prt;

	printf("protozoa: %s %s %s %d\n", in_out, protocol, port, baud);
	prt = config_find_port(c, port);
	if(prt) {
		if(strcasecmp(in_out, "IN") == 0) {
			if(config_set_inbound(prt, protocol) < 0)
				goto fail;
		} else {
			fprintf(stderr, "Invalid directive: %s\n", in_out);
			goto fail;
		}
	} else {
		prt = config_new_port(c, port, baud);
		if(prt == NULL)
			goto fail;
		if(config_new_combiner(c, prt, in_out, protocol) == NULL)
			goto fail;
	}
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
	char in_out[4];
	char protocol[16];
	char port[16];
	int baud = 9600;
	i = sscanf(c->line, "%3s %15s %15s %6d", in_out, protocol, port, &baud);
	if(i > 2)
		return config_directive(c, in_out, protocol, port, baud);
	else if(i <= 0)
		return 0;
	else {
		fprintf(stderr, "Invalid directive: %s\n", c->line);
		return -1;
	}
}

int config_read(struct config *c) {
	FILE *f = fopen(c->filename, "r");

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
