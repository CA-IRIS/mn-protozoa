#include <strings.h>
#include "ccwriter.h"
#include "manchester.h"
#include "pelco_d.h"
#include "vicon.h"

static unsigned int ccwriter_do_write(struct ccwriter *w, struct ccpacket *p) {
	/* Do nothing */
	return 0;
}

static void ccwriter_init(struct ccwriter *w, struct channel *chn) {
	w->do_write = ccwriter_do_write;
	w->log = chn->log;
	w->txbuf = chn->txbuf;
	w->base = 0;
	w->range = 0;
	w->next = NULL;
}

static int ccwriter_set_protocol(struct ccwriter *w, const char *protocol) {
	if(strcasecmp(protocol, "manchester") == 0)
		w->do_write = manchester_do_write;
	else if(strcasecmp(protocol, "pelco_d") == 0)
		w->do_write = pelco_d_do_write;
	else if(strcasecmp(protocol, "vicon") == 0)
		w->do_write = vicon_do_write;
	else {
		log_println(w->log, "Unknown protocol: %s", protocol);
		return -1;
	}
	return 0;
}

uint8_t *ccwriter_append(struct ccwriter *w, size_t n_bytes) {
	uint8_t *mess = buffer_append(w->txbuf, n_bytes);
	if(mess)
		return mess;
	else {
		log_println(w->log, "ccwriter_append: output buffer full");
		return NULL;
	}
}

struct ccwriter *ccwriter_create(struct channel *chn, const char *protocol,
	int base, int range)
{
	struct ccwriter *w = malloc(sizeof(struct ccwriter));
	if(w == NULL)
		return NULL;
	ccwriter_init(w, chn);
	if(ccwriter_set_protocol(w, protocol) < 0)
		goto fail;
	w->base = base;
	w->range = range;
	return w;
fail:
	free(w);
	return NULL;
}

void ccwriter_set_receiver(const struct ccwriter *w, struct ccpacket *p) {
	p->receiver += w->base;
	if(p->receiver < 0)
		p->receiver = 0;
	if((w->range > 0) && (p->receiver > w->range))
		p->receiver = 0;
}
