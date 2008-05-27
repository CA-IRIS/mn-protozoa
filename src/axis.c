/*
 * protozoa -- CCTV transcoder / mixer for PTZ
 * Copyright (C) 2007  Traffic Technologies
 * Copyright (C) 2007-2008  Minnesota Department of Transportation
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
#include <stdbool.h>
#include <string.h>
#include "ccreader.h"
#include "axis.h"

#define AXIS_MAX_SPEED (100)

static const char *default_speed = "100";
static const char *axis_header = "GET /axis-cgi/com/ptz.cgi?";
static const char *axis_header_auth = "GET /axis-cgi/com/ptzconfig.cgi?";
static const char *axis_trailer = " HTTP/1.0";
static const char *axis_auth = "\r\nAuthorization: Basic ";
static const char *axis_ending = "\r\n\r\n";

/*
 * axis_add_to_buffer	Add a string to the transmit buffer.
 *
 * msg: char string to add to the buffer
 */
static void axis_add_to_buffer(struct ccwriter *wtr, const char *msg) {
	char *mess = ccwriter_append(wtr, strlen(msg));
	if(mess)
		memcpy(mess, msg, strlen(msg));
}

/*
 * axis_prepare_buffer	Prepare the transmit buffer for writing.
 *
 * somein: 1 -> some data in buffer; 2 -> authenticated request in buffer
 * auth: Flag to incidate an authenticated request
 * return: new value of somein
 */
static int axis_prepare_buffer(struct ccwriter *w, int somein, bool auth) {
	if(somein)
		axis_add_to_buffer(w, "&");
	else {
		if(auth)
			axis_add_to_buffer(w, axis_header_auth);
		else
			axis_add_to_buffer(w, axis_header);
		somein = 1 + auth;
	}
	return somein;
}

/*
 * axis_encode_speed	Encode pan/tilt speed.
 */
int axis_encode_speed(int speed) {
	return ((speed * AXIS_MAX_SPEED) / (SPEED_MAX + 1)) + 1;
}

/*
 * encode_pan		Encode the pan speed.
 *
 * p: Packet to encode pan speed from
 * mess: string to append to
 */
static void encode_pan(struct ccpacket *p, char *mess) {
	char speed_str[8];

	int speed = axis_encode_speed(p->pan);
	if(p->command & CC_PAN_LEFT)
		speed = -speed;
	if(snprintf(speed_str, 8, "%d,", speed) > 0)
		strcat(mess, speed_str);
	else
		strcat(mess, "0,");
}

/*
 * encode_tilt		Encode the tilt speed.
 *
 * p: Packet to encode tilt speed from
 * mess: string to append to
 */
static void encode_tilt(struct ccpacket *p, char *mess) {
	char speed_str[8];

	int speed = axis_encode_speed(p->tilt);
	if(p->command & CC_TILT_DOWN)
		speed = -speed;
	if(snprintf(speed_str, 8, "%d,", speed) > 0)
		strcat(mess, speed_str);
	else
		strcat(mess, "0,");
}

/*
 * encode_pan_tilt	Encode an axis pan/tilt request.
 *
 * p: Packet with pan/tilt values to encode.
 * somein: Flag to determine whether some data is already in the buffer
 * return: new somein value
 */
static int encode_pan_tilt(struct ccwriter *w, struct ccpacket *p, int somein) {
	char mess[64];
	mess[0] = '\0';
	if(p->command & CC_PAN_TILT) {
		somein = axis_prepare_buffer(w, somein, false);
		strcat(mess, "continuouspantiltmove=");
		if(p->pan)
			encode_pan(p, mess);
		else
			strcat(mess, "0,");
		if(p->tilt)
			encode_tilt(p, mess);
		else
			strcat(mess, "0");
		axis_add_to_buffer(w, mess);
	}
	return somein;
}

/*
 * encode_focus		Encode an axis focus request.
 *
 * p: Packet with focus value to encode.
 * somein: Flag to determine whether some data is already in the buffer
 * return: new somein value
 */
static int encode_focus(struct ccwriter *w, struct ccpacket *p, int somein) {
	char mess[32];
	strcpy(mess, "continuousfocusmove=");
	if(p->focus == FOCUS_NEAR) {
		somein = axis_prepare_buffer(w, somein, false);
		strcat(mess, default_speed);
	} else if(p->focus == FOCUS_FAR) {
		somein = axis_prepare_buffer(w, somein, false);
		strcat(mess, "-");
		strcat(mess, default_speed);
	} else {
		somein = axis_prepare_buffer(w, somein, false);
		strcat(mess, "0");
	}
	axis_add_to_buffer(w, mess);
	return somein;
}

/*
 * encode_zoom		Encode an axis zoom request.
 *
 * p: Packet with zoom value to encode.
 * somein: Flag to determine whether some data is already in the buffer
 * return: new somein value
 */
static int encode_zoom(struct ccwriter *w, struct ccpacket *p, int somein) {
	char mess[32];
	strcpy(mess, "continuouszoommove=");
	if(p->zoom == ZOOM_IN) {
		somein = axis_prepare_buffer(w, somein, false);
		strcat(mess, default_speed);
	} else if(p->zoom == ZOOM_OUT) {
		somein = axis_prepare_buffer(w, somein, false);
		strcat(mess, "-");
		strcat(mess, default_speed);
	} else {
		somein = axis_prepare_buffer(w, somein, false);
		strcat(mess, "0");
	}
	axis_add_to_buffer(w, mess);
	return somein;
}

/*
 * encode_command	Encode an axis pan/tilt/zoom or focus request.
 *
 * p: Packet with zoom value to encode.
 * somein: Flag to determine whether some data is already in the buffer
 * return: new somein value
 */
static int encode_command(struct ccwriter *w, struct ccpacket *p, int somein) {
	somein = encode_pan_tilt(w, p, somein);
	somein = encode_focus(w, p, somein);
	return encode_zoom(w, p, somein);
}

/*
 * has_command		Test if a packet has a command to encode.
 *
 * p: Packet to check for command
 * return: True is command is present; false otherwise
 */
static inline bool has_command(struct ccpacket *p) {
	if(p->command & CC_PAN_TILT)
		return true;
	if(p->zoom || p->focus || p->iris)
		return true;
	return false;
}

/*
 * encode_preset	Encode an axis preset request.
 *
 * p: Packet with preset value to encode.
 * somein: Flag to determine whether some data is already in the buffer
 * return: new somein value
 */
static int encode_preset(struct ccwriter *w, struct ccpacket *p, int somein) {
	char num[16];
	char mess[32];

	if(p->command & CC_RECALL) {
		somein = axis_prepare_buffer(w, somein, false);
		strcpy(mess, "goto");
	} else if(p->command & CC_STORE) {
		somein = axis_prepare_buffer(w, somein, true);
		strcpy(mess, "set");
	} else if(p->command & CC_CLEAR) {
		somein = axis_prepare_buffer(w, somein, true);
		strcpy(mess, "remove");
	}
	strcat(mess, "serverpresetname=");
	sprintf(num, "Pos%d", p->preset);
	strcat(mess, num);
	axis_add_to_buffer(w, mess);
	return somein;
}

/*
 * axis_do_write	Encode a packet to the axis protocol.
 *
 * p: Packet to encode.
 * return: count of encoded packets
 */
unsigned int axis_do_write(struct ccwriter *w, struct ccpacket *p) {
	int somein = 0;
	if(!buffer_is_empty(&w->chn->txbuf)) {
		log_println(w->chn->log, "axis: dropping packet(s)");
		buffer_clear(&w->chn->txbuf);
	}
	if(ccpacket_has_preset(p))
		somein = encode_preset(w, p, somein);
	else if(has_command(p))
		somein = encode_command(w, p, somein);
	if(somein) {
		axis_add_to_buffer(w, axis_trailer);
		if(somein == 2) {
			axis_add_to_buffer(w, axis_auth);
			axis_add_to_buffer(w, w->auth);
		}
		axis_add_to_buffer(w, axis_ending);
		return 1;
	} else
		return 0;
}
