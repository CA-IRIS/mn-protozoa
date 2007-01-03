#include <strings.h>
#include "ccreader.h"
#include "manchester.h"
#include "pelco_d.h"
#include "vicon.h"

static void ccreader_do_read(struct ccreader *rdr, struct buffer *rxbuf) {
	/* Do nothing */
}

void ccreader_init(struct ccreader *rdr, struct log *log) {
	rdr->do_read = ccreader_do_read;
	ccpacket_init(&rdr->packet);
	rdr->writer = NULL;
	rdr->log = log;
}

void ccreader_add_writer(struct ccreader *rdr, struct ccwriter *wtr) {
	wtr->next = rdr->writer;
	rdr->writer = wtr;
}

static int ccreader_set_protocol(struct ccreader *rdr, const char *protocol) {
	if(strcasecmp(protocol, "manchester") == 0)
		rdr->do_read = manchester_do_read;
	else if(strcasecmp(protocol, "pelco_d") == 0)
		rdr->do_read = pelco_d_do_read;
	else if(strcasecmp(protocol, "vicon") == 0)
		rdr->do_read = vicon_do_read;
	else {
		log_println(rdr->log, "Unknown protocol: %s", protocol);
		return -1;
	}
	return 0;
}

static inline unsigned int ccreader_do_writers(struct ccreader *rdr) {
	unsigned int res = 0;
	struct ccpacket *pkt = &rdr->packet;
	const int receiver = pkt->receiver;	/* save "true" receiver */
	struct ccwriter *wtr = rdr->writer;
	while(wtr) {
		pkt->receiver = ccwriter_get_receiver(wtr, receiver);
		res += wtr->do_write(wtr, pkt);
		wtr = wtr->next;
	}
	pkt->receiver = receiver;	/* restore "true" receiver */
	if(res && rdr->log->packet)
		ccpacket_log(pkt, rdr->log, rdr->name);
	return res;
}

unsigned int ccreader_process_packet(struct ccreader *rdr) {
	struct ccpacket *pkt = &rdr->packet;
	unsigned int res = 0;
	if(pkt->receiver == 0)
		return 0;	/* Ignore if receiver is zero */
	else if(pkt->status)
		ccpacket_drop(pkt);
	else {
		res = ccreader_do_writers(rdr);
		if(res)
			ccpacket_count(pkt);
		else
			ccpacket_drop(pkt);
	}
	ccpacket_clear(pkt);
	return res;
}

struct ccreader *ccreader_create(const char *name, const char *protocol,
	struct log *log)
{
	struct ccreader *rdr = malloc(sizeof(struct ccreader));
	if(rdr == NULL)
		return NULL;
	ccreader_init(rdr, log);
	if(ccreader_set_protocol(rdr, protocol) < 0)
		goto fail;
	rdr->name = name;
	return rdr;
fail:
	free(rdr);
	return NULL;
}
