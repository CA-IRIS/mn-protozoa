/*
 * protozoa -- CCTV transcoder / mixer for PTZ
 * Copyright (C) 2007  Traffic Technologies
 * Copyright (C) 2007  Minnesota Department of Transportation
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

static const char *axis_header[] = {
	"GET /axis-cgi/com/ptz.cgi?",
	"GET /axis-cgi/com/ptzconfig.cgi?"
};
static const char *axis_trailer = " HTTP/1.0";
static const char *axis_auth = "\r\nAuthorization: Basic ";

static void axis_add_to_buffer(struct ccwriter *w, const char *msg) {
	char* mess = buffer_append(w->txbuf, strlen(msg));
	if(mess)
		memcpy(mess, msg, strlen(msg));
}

static int axis_check_buffer(struct ccwriter *w, int somein, int which) {
	if(somein)
		axis_add_to_buffer(w, "&");
	else {
		axis_add_to_buffer(w, axis_header[which]);
		somein = which + 1;
	}
	return somein;
}

static int encode_pan_tilt(struct ccwriter *w, struct ccpacket *p, int somein) {
	int speed;
	char speed_str[8];
	char mess[64];
	mess[0] = '\0';
	if(p->command & CC_PAN_TILT) {
		somein = axis_check_buffer(w, somein, 0);
		strcat(mess, "continuouspantiltmove=");
		if(p->pan) {
			speed = ((p->pan * AXIS_MAX_SPEED) / (SPEED_MAX + 1)) + 1;
			sprintf(speed_str, "%d,", speed);
			if(p->command & CC_PAN_LEFT) {
				strcat(mess, "-");
				strcat(mess, speed_str);
			} else if(p->command & CC_PAN_RIGHT)
				strcat(mess, speed_str);
			else
				strcat(mess, "0,");
		} else
			strcat(mess, "0,");
		if(p->tilt) {
			speed = ((p->tilt * AXIS_MAX_SPEED) / (SPEED_MAX + 1)) + 1;
			sprintf(speed_str, "%d", speed);
			if(p->command & CC_TILT_DOWN) {
				strcat(mess, "-");
				strcat(mess, speed_str);
			} else if(p->command & CC_TILT_UP)
				strcat(mess, speed_str);
			else
				strcat(mess, "0");
		} else
			strcat(mess, "0");
		axis_add_to_buffer(w, mess);
	}
	return somein;
}

static int encode_focus(struct ccwriter *w, struct ccpacket *p, int somein) {
	char mess[32];
	strcpy(mess, "continuousfocusmove=");
	if(p->focus == FOCUS_NEAR) {
		somein = axis_check_buffer(w, somein, 0);
		strcat(mess, default_speed);
	} else if(p->focus == FOCUS_FAR) {
		somein = axis_check_buffer(w, somein, 0);
		strcat(mess, "-");
		strcat(mess, default_speed);
	} else {
		somein = axis_check_buffer(w, somein, 0);
		strcat(mess, "0");
	}
	axis_add_to_buffer(w, mess);
	return somein;
}

static int encode_zoom(struct ccwriter *w, struct ccpacket *p, int somein) {
	char mess[32];
	strcpy(mess, "continuouszoommove=");
	if(p->zoom == ZOOM_IN) {
		somein = axis_check_buffer(w, somein, 0);
		strcat(mess, default_speed);
	} else if(p->zoom == ZOOM_OUT) {
		somein = axis_check_buffer(w, somein, 0);
		strcat(mess, "-");
		strcat(mess, default_speed);
	} else {
		somein = axis_check_buffer(w, somein, 0);
		strcat(mess, "0");
	}
	axis_add_to_buffer(w, mess);
	return somein;
}

static int encode_command(struct ccwriter *w, struct ccpacket *p, int somein) {
	somein = encode_pan_tilt(w, p, somein);
	somein = encode_focus(w, p, somein);
	return encode_zoom(w, p, somein);
}

static inline bool has_command(struct ccpacket *p) {
	if(p->command & CC_PAN_TILT)
		return true;
	if(p->zoom || p->focus || p->iris)
		return true;
	return false;
}

static int encode_preset(struct ccwriter *w, struct ccpacket *p, int somein) {
	char num[16];
	char mess[32];

	if(p->command & CC_RECALL) {
		somein = axis_check_buffer(w, somein, 0);
		strcpy(mess, "goto");
	} else if(p->command & CC_STORE) {
		somein = axis_check_buffer(w, somein, 1);
		strcpy(mess, "set");
	} else if(p->command & CC_CLEAR) {
		somein = axis_check_buffer(w, somein, 1);
		strcpy(mess, "remove");
	}
	strcat(mess, "serverpresetname=");
	sprintf(num, "Pos%d", p->preset);
	strcat(mess, num);
	axis_add_to_buffer(w, mess);
	return somein;
}

static inline bool has_preset(struct ccpacket *p) {
	return p->command & CC_PRESET;
}

unsigned int axis_do_write(struct ccwriter *w, struct ccpacket *p) {
	int somein = 0;
	if(has_preset(p))
		somein = encode_preset(w, p, somein);
	else if(has_command(p))
		somein = encode_command(w, p, somein);
	if(somein) {
		axis_add_to_buffer(w, axis_trailer);
		if(somein == 2) {
			axis_add_to_buffer(w, axis_auth);
			axis_add_to_buffer(w, w->auth);
		}
	}
	return 1;
}
