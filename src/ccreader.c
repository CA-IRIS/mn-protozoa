#include <strings.h>
#include "ccreader.h"
#include "manchester.h"
#include "pelco_d.h"
#include "vicon.h"

static void ccreader_do_read(struct ccreader *r, struct buffer *rxbuf) {
	/* Do nothing */
}

void ccreader_init(struct ccreader *r, struct log *log) {
	r->do_read = ccreader_do_read;
	ccpacket_init(&r->packet);
	r->writer = NULL;
	r->log = log;
}

void ccreader_add_writer(struct ccreader *r, struct ccwriter *w) {
	w->next = r->writer;
	r->writer = w;
}

static int ccreader_set_protocol(struct ccreader *r, const char *protocol) {
	if(strcasecmp(protocol, "manchester") == 0)
		r->do_read = manchester_do_read;
	else if(strcasecmp(protocol, "pelco_d") == 0)
		r->do_read = pelco_d_do_read;
	else if(strcasecmp(protocol, "vicon") == 0)
		r->do_read = vicon_do_read;
	else {
		log_println(r->log, "Unknown protocol: %s", protocol);
		return -1;
	}
	return 0;
}

static inline unsigned int ccreader_do_writers(struct ccreader *r) {
	struct ccwriter *w = r->writer;
	unsigned int res = 0;
	const int receiver = r->packet.receiver; /* save "true" receiver */
	while(w) {
		ccwriter_set_receiver(w, &r->packet);
		res += w->do_write(w, &r->packet);
		r->packet.receiver = receiver;  /* restore "true" receiver */
		w = w->next;
	}
	if(res && !r->log->quiet)
		ccpacket_log(&r->packet, r->log, r->name);
	return res;
}

unsigned int ccreader_process_packet(struct ccreader *r) {
	unsigned int res = 0;
	if(r->packet.receiver == 0)
		return 0;	/* Ignore if receiver is zero */
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

struct ccreader *ccreader_create(const char *name, const char *protocol,
	struct log *log)
{
	struct ccreader *r = malloc(sizeof(struct ccreader));
	if(r == NULL)
		return NULL;
	ccreader_init(r, log);
	if(ccreader_set_protocol(r, protocol) < 0)
		goto fail;
	r->name = name;
	return r;
fail:
	free(r);
	return NULL;
}
