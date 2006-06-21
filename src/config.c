#include <stdio.h>
#include <string.h>
#include "config.h"

void config_init(struct config *c, const char *filename, bool verbose,
	bool debug)
{
	c->filename = filename;
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

static int config_inbound2(struct sport *port, const char *protocol) {
	struct combiner *cmbnr = (struct combiner *)port->handler;
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
			if(config_inbound2(prt, protocol) < 0)
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

int config_read(struct config *c) {
	char in_out[4];
	char protocol[16];
	char port[16];
	int baud;
	FILE *f = fopen(c->filename, "r");

	while(fscanf(f, "%3s %15s %15s %6d", in_out, protocol, port, &baud)==4)
	{
		if(config_directive(c, in_out, protocol, port, baud) < 0)
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

void config_debug(int n_ports, struct sport *ports) {
	int i;
	for(i = 0; i < n_ports; i++) {
		ports[i].rxbuf->debug = true;
		ports[i].txbuf->debug = true;
	}
}
