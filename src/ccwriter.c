#include <strings.h>
#include "ccwriter.h"
#include "manchester.h"
#include "pelco_d.h"
#include "vicon.h"

static unsigned int ccwriter_do_write(struct ccwriter *wtr,
	struct ccpacket *pkt)
{
	/* Do nothing */
	return 0;
}

static void ccwriter_init(struct ccwriter *wtr, struct channel *chn) {
	wtr->do_write = ccwriter_do_write;
	wtr->log = chn->log;
	wtr->txbuf = chn->txbuf;
	wtr->base = 0;
	wtr->range = 0;
	wtr->next = NULL;
}

static int ccwriter_set_protocol(struct ccwriter *wtr, const char *protocol) {
	if(strcasecmp(protocol, "manchester") == 0)
		wtr->do_write = manchester_do_write;
	else if(strcasecmp(protocol, "pelco_d") == 0)
		wtr->do_write = pelco_d_do_write;
	else if(strcasecmp(protocol, "vicon") == 0)
		wtr->do_write = vicon_do_write;
	else {
		log_println(wtr->log, "Unknown protocol: %s", protocol);
		return -1;
	}
	return 0;
}

struct ccwriter *ccwriter_new(struct channel *chn, const char *protocol,
	int base, int range)
{
	struct ccwriter *wtr = malloc(sizeof(struct ccwriter));
	if(wtr == NULL)
		return NULL;
	ccwriter_init(wtr, chn);
	if(ccwriter_set_protocol(wtr, protocol) < 0)
		goto fail;
	wtr->base = base;
	wtr->range = range;
	return wtr;
fail:
	free(wtr);
	return NULL;
}

uint8_t *ccwriter_append(struct ccwriter *wtr, size_t n_bytes) {
	uint8_t *mess = buffer_append(wtr->txbuf, n_bytes);
	if(mess)
		return mess;
	else {
		log_println(wtr->log, "ccwriter_append: output buffer full");
		return NULL;
	}
}

int ccwriter_get_receiver(const struct ccwriter *wtr, int receiver) {
	receiver += wtr->base;
	if((receiver < 0) || ((wtr->range > 0) && (receiver > wtr->range)))
		return 0;
	else
		return receiver;
}
