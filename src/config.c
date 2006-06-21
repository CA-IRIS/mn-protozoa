#include <stdio.h>
#include <string.h>
#include "sport.h"
#include "combiner.h"
#include "config.h"

void config_init(struct config *c, const char *filename, bool verbose) {
	c->filename = filename;
	c->ports = NULL;
	c->n_ports = 0;
	c->verbose = verbose;
}

static int config_inbound2(struct sport *port, const char *protocol) {
	struct combiner *cmbnr = (struct combiner *)port->handler;
	if(cmbnr == NULL)
		return -1;
	return combiner_set_input_protocol(cmbnr, protocol);
}

static struct sport *find_port(struct sport *ports[], int n_ports, char *port) {
	int i;
	for(i = 0; i < n_ports; i++) {
		if(strcmp(port, ports[i]->name) == 0)
			return ports[i];
	}
	return NULL;
}

int config_read(const char *filename, struct sport *ports[], bool verbose) {
	char in_out[4];
	char protocol[16];
	char port[16];
	int baud;
	int n_ports = 0;
	struct sport *prt;
	struct combiner *cmbnr = NULL;
	FILE *f = fopen(filename, "r");
	*ports = NULL;

	while(fscanf(f, "%3s %15s %15s %6d", in_out, protocol, port, &baud)==4)
	{
		printf("protozoa: %s %s %s %d\n", in_out, protocol, port, baud);
		prt = find_port(ports, n_ports, port);
		if(prt == NULL) {
			n_ports++;
			*ports = realloc(*ports, sizeof(struct sport)* n_ports);
			if(*ports == NULL)
				goto fail;
			prt = *ports + (n_ports - 1);
			if(sport_init(prt, port, baud) == NULL) {
				printf("Error initializing serial port: %s\n",
					port);
				goto fail;
			}
			if(strcasecmp(in_out, "OUT") == 0) {
				cmbnr = combiner_create_outbound(prt, protocol,
					verbose);
				if(cmbnr == NULL)
					goto fail;
			} else if(strcasecmp(in_out, "IN") == 0) {
				if(combiner_create_inbound(prt, protocol,
					cmbnr) == NULL)
					goto fail;
			} else
				goto fail;
		} else {
			if(strcasecmp(in_out, "IN") == 0) {
				if(config_inbound2(prt, protocol) < 0)
					goto fail;
			} else
				goto fail;
		}
	}
	if(n_ports == 0)
		printf("Error reading configuration file: %s\n", filename);
	fclose(f);
	return n_ports;
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
