/*
 * protozoa -- CCTV transcoder / mixer for PTZ
 * Copyright (C) 2006-2008  Minnesota Department of Transportation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <string.h>		/* for strcpy, strlen */
#include <strings.h>		/* for strcasecmp */
#include "ccwriter.h"
#include "axis.h"
#include "manchester.h"
#include "pelco_d.h"
#include "vicon.h"

/*
 * ccwriter_set_receivers	Set the number of receivers for the writer.
 */
static int ccwriter_set_receivers(struct ccwriter *wtr, const int n_rcv) {
	struct timeval now;
	int i;

	wtr->ptime = malloc(sizeof(struct timeval) * n_rcv);
	if(wtr->ptime == NULL)
		return -1;
	if(gettimeofday(&now, NULL) == 0) {
		for(i = 0; i < n_rcv; i++)
			memcpy(wtr->ptime + i, &now, sizeof(struct timeval));
	}
	return 0;
}

/*
 * ccwriter_set_protocol	Set protocol of the camera control writer.
 *
 * protocol: protocol name
 * return: 0 on success; -1 if protocol not found or allocation error
 */
static int ccwriter_set_protocol(struct ccwriter *wtr, const char *protocol) {
	if(strcasecmp(protocol, "manchester") == 0) {
		wtr->do_write = manchester_do_write;
		return ccwriter_set_receivers(wtr, MANCHESTER_MAX_ADDRESS);
	} else if(strcasecmp(protocol, "pelco_d") == 0) {
		wtr->do_write = pelco_d_do_write;
		return ccwriter_set_receivers(wtr, PELCO_D_MAX_ADDRESS);
	} else if(strcasecmp(protocol, "vicon") == 0) {
		wtr->do_write = vicon_do_write;
		return ccwriter_set_receivers(wtr, VICON_MAX_ADDRESS);
	} else if(strcasecmp(protocol, "axis") == 0) {
		wtr->do_write = axis_do_write;
		wtr->chn->response_required = true;
		return 0;
	} else {
		log_println(wtr->chn->log, "Unknown protocol: %s", protocol);
		return -1;
	}
}

/*
 * ccwriter_init	Initialize a new camera control writer.
 *
 * chn: channel to write camera control output
 * protocol: protocol name
 * auth: authentication token
 * return: pointer to struct ccwriter on success; NULL on error
 */
static struct ccwriter *ccwriter_init(struct ccwriter *wtr, struct channel *chn,
	const char *protocol, const char *auth)
{
	wtr->chn = chn;
	wtr->ptime = NULL;
	wtr->auth = NULL;
	if(auth) {
		wtr->auth = malloc(strlen(auth) + 1);
		if(wtr->auth == NULL)
			return NULL;
		else
			strcpy(wtr->auth, auth);
	}
	if(ccwriter_set_protocol(wtr, protocol) < 0)
		return NULL;
	else
		return wtr;
}

/*
 * ccwriter_new		Construct a new camera control writer.
 *
 * chn: channel to write camera control output
 * protocol: protocol name
 * auth: authentication token
 * return: pointer to camera control writer
 */
struct ccwriter *ccwriter_new(struct channel *chn, const char *protocol,
	const char *auth)
{
	struct ccwriter *wtr = malloc(sizeof(struct ccwriter));
	if(wtr == NULL)
		return NULL;
	if(ccwriter_init(wtr, chn, protocol, auth) == NULL)
		goto fail;
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
	void *mess = buffer_append(&wtr->chn->txbuf, n_bytes);
	if(mess)
		return mess;
	else {
		log_println(wtr->chn->log,
			"ccwriter_append: output buffer full");
		return NULL;
	}
}

/*
 * ccwriter_command_receiver	Update the command time for a receiver
 */
void ccwriter_command_receiver(struct ccwriter *wtr, const int receiver) {
	gettimeofday(wtr->ptime + receiver - 1, NULL);
}
