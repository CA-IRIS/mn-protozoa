#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "sport.h"
#include "combiner.h"
#include "manchester.h"
#include "vicon.h"

static int config_do_write(struct combiner *cmbnr, const char *protocol) {
	if(strcasecmp(protocol, "manchester") == 0)
		cmbnr->do_write = manchester_do_write;
	else if(strcasecmp(protocol, "vicon") == 0)
		cmbnr->do_write = vicon_do_write;
	else {
		printf("Unknown protocol: %s\n", protocol);
		return -1;
	}
	return 0;
}

static int config_do_read(struct combiner *cmbnr, const char *protocol) {
	if(strcasecmp(protocol, "manchester") == 0)
		cmbnr->handler.do_read = manchester_do_read;
	else if(strcasecmp(protocol, "vicon") == 0)
		cmbnr->handler.do_read = vicon_do_read;
	else {
		printf("Unknown protocol: %s\n", protocol);
		return -1;
	}
	return 0;
}

static struct combiner *config_outbound(struct sport *port,
	char *protocol)
{
	struct combiner *cmbnr = malloc(sizeof(struct combiner));
	if(cmbnr == NULL)
		return NULL;
	combiner_init(cmbnr, &port->txbuf);
	if(config_do_write(cmbnr, protocol) < 0)
		goto fail;
	port->handler = &cmbnr->handler;
	return cmbnr;
fail:
	free(cmbnr);
	return NULL;
}

static void config_inbound(struct sport *port, const char *protocol,
	struct combiner *out)
{
	struct combiner *cmbnr = malloc(sizeof(struct combiner));
	if(cmbnr == NULL)
		return;
	if(out == NULL) {
		printf("Missing OUT directive");
		goto fail;
	}
	if(config_do_read(cmbnr, protocol) < 0)
		goto fail;
	ccpacket_init(&cmbnr->packet);
	cmbnr->txbuf = out->txbuf;
	port->handler = &cmbnr->handler;
	return;
fail:
	free(cmbnr);
}

static void config_inbound2(struct sport *port, const char *protocol) {
	struct combiner *cmbnr = (struct combiner *)port->handler;
	if(cmbnr == NULL)
		return;
	config_do_read(cmbnr, protocol);
}

static struct sport *find_port(struct sport *ports[], int n_ports, char *port) {
	int i;
	for(i = 0; i < n_ports; i++) {
		if(strcmp(port, ports[i]->name) == 0)
			return ports[i];
	}
	return NULL;
}

int config_read(const char *filename, struct sport *ports[]) {
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
			if(strcasecmp(in_out, "OUT") == 0)
				cmbnr = config_outbound(prt, protocol);
			else if(strcasecmp(in_out, "IN") == 0)
				config_inbound(prt, protocol, cmbnr);
			else
				goto fail;
		} else {
			if(strcasecmp(in_out, "IN") == 0)
				config_inbound2(prt, protocol);
			else
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
