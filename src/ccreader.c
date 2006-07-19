#include <stdio.h>
#include <strings.h>
#include "ccreader.h"
#include "manchester.h"
#include "pelco_d.h"
#include "vicon.h"

static int ccreader_do_read(struct handler *h, struct buffer *rxbuf) {
	/* Do nothing */
	return 0;
}

void ccreader_init(struct ccreader *r) {
	r->handler.do_read = ccreader_do_read;
	ccpacket_init(&r->packet);
	r->writer = NULL;
	r->verbose = false;
}

void ccreader_add_writer(struct ccreader *r, struct ccwriter *w) {
	w->next = r->writer;
	r->writer = w;
}

static int ccreader_set_protocol(struct ccreader *r, const char *protocol) {
	if(strcasecmp(protocol, "manchester") == 0)
		r->handler.do_read = manchester_do_read;
	else if(strcasecmp(protocol, "pelco_d") == 0)
		r->handler.do_read = pelco_d_do_read;
	else if(strcasecmp(protocol, "vicon") == 0)
		r->handler.do_read = vicon_do_read;
	else {
		fprintf(stderr, "Unknown protocol: %s\n", protocol);
		return -1;
	}
	return 0;
}

static inline int ccreader_do_writers(struct ccreader *r) {
	struct ccwriter *w = r->writer;
	int res = 0;
	while(w) {
		int i = w->do_write(w, &r->packet);
		if(i < 0)
			return i;
		else if(i > 0)
			res = i;
		w = w->next;
	}
	if(res && r->verbose)
		ccpacket_debug(&r->packet);
	return res;
}

int ccreader_process_packet(struct ccreader *r) {
	int res = 0;
	if(r->packet.receiver == 0)
		return 0;	// Ignore if receiver is zero
	else if(r->packet.status)
		ccpacket_drop(&r->packet);
	else {
		res = ccreader_do_writers(r);
		if(res)
			ccpacket_count(&r->packet);
		else
			ccpacket_drop(&r->packet);
	}
	ccpacket_clear(&r->packet);
	return res;
}

struct ccreader *ccreader_create(struct sport *port, const char *protocol,
	bool verbose)
{
	struct ccreader *r = malloc(sizeof(struct ccreader));
	if(r == NULL)
		return NULL;
	ccreader_init(r);
	if(ccreader_set_protocol(r, protocol) < 0)
		goto fail;
	port->handler = &r->handler;
	return r;
fail:
	free(r);
	return NULL;
}
