#include <strings.h>
#include "ccwriter.h"
#include "manchester.h"
#include "pelco_d.h"
#include "vicon.h"

/*
 * ccwriter_do_write	Stub function for camera control writers.
 *
 * pkt: camera control packet to write
 */
static unsigned int ccwriter_do_write(struct ccwriter *wtr,
	struct ccpacket *pkt)
{
	/* Do nothing */
	return 0;
}

/*
 * ccwriter_init	Initialize a new camera control writer.
 *
 * chn: channel to write camera control output
 */
static void ccwriter_init(struct ccwriter *wtr, struct channel *chn) {
	wtr->do_write = ccwriter_do_write;
	wtr->log = chn->log;
	wtr->txbuf = chn->txbuf;
	wtr->base = 0;
	wtr->range = 0;
	wtr->next = NULL;
}

/*
 * ccwriter_set_protocol	Set protocol of the camera control writer.
 *
 * protocol: protocol name
 * return: 0 on success; -1 if protocol not found
 */
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

/*
 * ccwriter_new		Construct a new camera control writer.
 *
 * chn: channel to write camera control output
 * protocol: protocol name
 * base: base receiver address for output
 * range: range of receiver addresses for output
 * return: pointer to camera control writer
 */
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

/*
 * ccwriter_append	Append data to the camera control writer.
 *
 * n_bytes: number of bytes to append
 * return: borrowed pointer to appended data
 */
void *ccwriter_append(struct ccwriter *wtr, size_t n_bytes) {
	void *mess = buffer_append(wtr->txbuf, n_bytes);
	if(mess)
		return mess;
	else {
		log_println(wtr->log, "ccwriter_append: output buffer full");
		return NULL;
	}
}

/*
 * ccwriter_get_receiver	Get receiver address adjusted for the writer.
 *
 * receiver: input receiver address
 * return: output receiver address; 0 indicates drop packet
 */
int ccwriter_get_receiver(const struct ccwriter *wtr, int receiver) {
	receiver += wtr->base;
	if((receiver < 0) || ((wtr->range > 0) && (receiver > wtr->range)))
		return 0;
	else
		return receiver;
}
