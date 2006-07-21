#include <stdio.h>
#include <strings.h>
#include "ccwriter.h"
#include "manchester.h"
#include "pelco_d.h"
#include "vicon.h"

static int ccwriter_do_write(struct ccwriter *w, struct ccpacket *p) {
	/* Do nothing */
	return 0;
}

void ccwriter_init(struct ccwriter *w) {
	w->do_write = ccwriter_do_write;
	w->txbuf = NULL;
	w->base = 0;
	w->next = NULL;
}

int ccwriter_set_protocol(struct ccwriter *w, const char *protocol) {
	if(strcasecmp(protocol, "manchester") == 0)
		w->do_write = manchester_do_write;
	else if(strcasecmp(protocol, "pelco_d") == 0)
		w->do_write = pelco_d_do_write;
	else if(strcasecmp(protocol, "vicon") == 0)
		w->do_write = vicon_do_write;
	else {
		fprintf(stderr, "Unknown protocol: %s\n", protocol);
		return -1;
	}
	return 0;
}

uint8_t *ccwriter_append(struct ccwriter *w, size_t count) {
	uint8_t *mess = buffer_append(w->txbuf, count);
	if(mess)
		return mess;
	else {
		fprintf(stderr, "protozoa: output buffer full\n");
		return NULL;
	}
}

struct ccwriter *ccwriter_create(struct sport *port, const char *protocol,
	int base)
{
	struct ccwriter *w = malloc(sizeof(struct ccwriter));
	if(w == NULL)
		return NULL;
	ccwriter_init(w);
	if(ccwriter_set_protocol(w, protocol) < 0)
		goto fail;
	w->txbuf = port->txbuf;
	w->base = base;
	return w;
fail:
	free(w);
	return NULL;
}
