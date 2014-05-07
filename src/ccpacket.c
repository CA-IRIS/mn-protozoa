/*
 * protozoa -- CCTV transcoder / mixer for PTZ
 * Copyright (C) 2006-2014  Minnesota Department of Transportation
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
#include <string.h>	/* for memcpy */
#include "ccpacket.h"
#include "stats.h"

/*
 * Special preset numbers for on-screen menu functions
 */
enum special_presets {
	MENU_OPEN_PRESET = 77,
	MENU_ENTER_PRESET = 78,
	MENU_CANCEL_PRESET = 79,
};

/** Initialize a camera control packet.
 */
void ccpacket_init(struct ccpacket *pkt) {
	timeval_set_now(&pkt->expire);
	ccpacket_clear(pkt);
}

/** Clear the camera control packet.
 */
void ccpacket_clear(struct ccpacket *pkt) {
	pkt->receiver = 0;
	pkt->status = STATUS_NONE;
	pkt->command = 0;
	pkt->pan = 0;
	pkt->tilt = 0;
	pkt->lens = 0;
	pkt->aux = 0;
	pkt->preset = 0;
}

/** Set receiver for packet.
 *
 * @param receiver	Receiver address.
 */
void ccpacket_set_receiver(struct ccpacket *self, int receiver) {
	self->receiver = receiver;
}

/** Get receiver for packet.
 *
 * @return Receiver address.
 */
int ccpacket_get_receiver(const struct ccpacket *self) {
	return self->receiver;
}

/** Set status request.
 *
 * @param s		Status request.
 */
void ccpacket_set_status(struct ccpacket *self, enum status_t s) {
	self->status = s;
}

/** Get status request.
 *
 * @return Status request.
 */
enum status_t ccpacket_get_status(const struct ccpacket *self) {
	return self->status;
}

/** Set pan speed.
 *
 * @param speed		Pan speed.
 */
void ccpacket_set_pan_speed(struct ccpacket *self, int speed) {
	self->pan = speed;
}

/** Get pan speed.
 */
int ccpacket_get_pan_speed(const struct ccpacket *self) {
	return self->pan;
}

/** Check if packet has pan.
 */
bool ccpacket_has_pan(const struct ccpacket *self) {
	return (self->command & CC_PAN) && (self->pan);
}

/** Set tilt speed.
 *
 * @param speed		Tilt speed.
 */
void ccpacket_set_tilt_speed(struct ccpacket *self, int speed) {
	self->tilt = speed;
}

/** Get tilt speed.
 */
int ccpacket_get_tilt_speed(const struct ccpacket *self) {
	return self->tilt;
}

/** Check if packet has tilt.
 */
bool ccpacket_has_tilt(const struct ccpacket *self) {
	return (self->command & CC_TILT) && (self->tilt);
}

/*
 * ccpacket_set_timeout	Set the timeout for a camera control packet.
 */
void ccpacket_set_timeout(struct ccpacket *pkt, unsigned int timeout) {
	timeval_set_now(&pkt->expire);
	timeval_adjust(&pkt->expire, timeout);
}

/** Check is packet is expired.
 *
 * @param timeout	Timeout in ms.
 * @return true if packet is expired.
 */
bool ccpacket_is_expired(struct ccpacket *self, unsigned int timeout) {
	return time_from_now(&self->expire) > timeout;
}

/** Test if the packet has a preset command.
 */
bool ccpacket_has_preset(const struct ccpacket *self) {
	return self->command & CC_PRESET;
}

/** Decode a store preset command.  Predefined presets are replaced with menu
 * commands.
 */
void ccpacket_store_preset(struct ccpacket *self, int p_num) {
	self->command &= ~CC_PRESET;
	switch(p_num) {
	case MENU_OPEN_PRESET:
		self->command |= CC_MENU_OPEN;
		break;
	case MENU_ENTER_PRESET:
		self->command |= CC_MENU_ENTER;
		break;
	case MENU_CANCEL_PRESET:
		self->command |= CC_MENU_CANCEL;
		break;
	default:
		if(p_num > 0)
			self->command |= CC_STORE;
		self->preset = p_num;
	}
}

/** Recall a preset.
 */
void ccpacket_recall_preset(struct ccpacket *self, int p_num) {
	self->command &= ~CC_PRESET;
	if(p_num > 0)
		self->command |= CC_RECALL;
	self->preset = p_num;
}

/** Clear a preset.
 */
void ccpacket_clear_preset(struct ccpacket *self, int p_num) {
	self->command &= ~CC_PRESET;
	if(p_num > 0)
		self->command |= CC_CLEAR;
	self->preset = p_num;
}

/** Get the preset number.
 */
int ccpacket_get_preset(const struct ccpacket *self) {
	return self->preset;
}

/*
 * ccpacket_is_stop	Test if the packet is a stop command.
 */
bool ccpacket_is_stop(struct ccpacket *pkt) {
	return (pkt->command | CC_PAN_TILT) == CC_PAN_TILT &&
	       pkt->pan == 0 &&
	       pkt->tilt == 0 &&
	       ccpacket_get_zoom(pkt) == CC_ZOOM_STOP &&
	       ccpacket_get_focus(pkt) == CC_FOCUS_STOP &&
	       ccpacket_get_iris(pkt) == CC_IRIS_STOP &&
	       pkt->aux == 0 &&
	       pkt->status == STATUS_NONE;
}

/** Get a valid zoom mode */
static enum lens_t ccpacket_zoom(enum lens_t zm) {
	enum lens_t z = zm & CC_ZOOM;
	switch(z) {
	case CC_ZOOM_IN:
	case CC_ZOOM_OUT:
		return z;
	default:
		return CC_ZOOM_STOP;
	}
}

/** Set the zoom mode.
 *
 * @param zm		Zoom mode
 */
void ccpacket_set_zoom(struct ccpacket *self, enum lens_t zm) {
	self->lens = ccpacket_zoom(zm) | (self->lens & ~CC_ZOOM);
}

/** Get the zoom mode.
 */
enum lens_t ccpacket_get_zoom(const struct ccpacket *self) {
	return ccpacket_zoom(self->lens);
}

/** Get a valid focus mode */
static enum lens_t ccpacket_focus(enum lens_t fm) {
	enum lens_t f = fm & CC_FOCUS;
	switch(f) {
	case CC_FOCUS_NEAR:
	case CC_FOCUS_FAR:
	case CC_FOCUS_AUTO:
		return f;
	default:
		return CC_FOCUS_STOP;
	}
}

/** Set the focus mode.
 *
 * @param fm		Focus mode
 */
void ccpacket_set_focus(struct ccpacket *self, enum lens_t fm) {
	self->lens = ccpacket_focus(fm) | (self->lens & ~CC_FOCUS);
}

/** Get the focus mode.
 */
enum lens_t ccpacket_get_focus(const struct ccpacket *self) {
	return ccpacket_focus(self->lens);
}

/** Get a valid iris mode */
static enum lens_t ccpacket_iris(enum lens_t im) {
	enum lens_t i = im & CC_IRIS;
	switch(i) {
	case CC_IRIS_CLOSE:
	case CC_IRIS_OPEN:
	case CC_IRIS_AUTO:
		return i;
	default:
		return CC_IRIS_STOP;
	}
}

/** Set the iris mode.
 *
 * @param im		Iris mode
 */
void ccpacket_set_iris(struct ccpacket *self, enum lens_t im) {
	self->lens = ccpacket_iris(im) | (self->lens & ~CC_IRIS);
}

/** Get the iris mode.
 */
enum lens_t ccpacket_get_iris(const struct ccpacket *self) {
	return ccpacket_iris(self->lens);
}

/** Get a valid lens mode */
static enum lens_t ccpacket_lens(enum lens_t lm) {
	enum lens_t l = lm & CC_LENS;
	switch(l) {
	case CC_LENS_SPEED:
		return l;
	default:
		return CC_LENS_NONE;
	}
}

/** Set the lens mode.
 *
 * @param lm		Lens mode
 */
void ccpacket_set_lens(struct ccpacket *self, enum lens_t lm) {
	self->lens = ccpacket_lens(lm) | (self->lens & ~CC_LENS);
}

/** Get the lens mode.
 */
enum lens_t ccpacket_get_lens(const struct ccpacket *self) {
	return ccpacket_lens(self->lens);
}

/*
 * ccpacket_has_command	Test if a packet has a command to encode.
 *
 * pkt: Packet to check for command
 * return: True if command is present; false otherwise
 */
bool ccpacket_has_command(const struct ccpacket *pkt) {
	return (pkt->command & CC_PAN_TILT) ||
	       (ccpacket_get_zoom(pkt) != CC_ZOOM_STOP) ||
	       (ccpacket_get_focus(pkt) != CC_FOCUS_STOP) ||
	       (ccpacket_get_iris(pkt) != CC_IRIS_STOP);
}

/*
 * ccpacket_has_aux	Test if the packet has an auxiliary function.
 */
bool ccpacket_has_aux(struct ccpacket *pkt) {
	if(pkt->aux)
		return true;
	else
		return false;
}

/*
 * ccpacket_has_autopan	Test if the packet has an autopan command.
 */
bool ccpacket_has_autopan(const struct ccpacket *pkt) {
	if(pkt->command & (CC_AUTO_PAN | CC_MANUAL_PAN))
		return true;
	else
		return false;
}

/*
 * ccpacket_has_power	Test if the packet has a power command.
 */
bool ccpacket_has_power(const struct ccpacket *pkt) {
	if(pkt->command & (CC_CAMERA_ON | CC_CAMERA_OFF))
		return true;
	else
		return false;
}

/*
 * ccpacket_log_pan	Log any pan command in the camera control packet.
 *
 * log: message logger
 */
static inline void ccpacket_log_pan(struct ccpacket *pkt, struct log *log) {
	if(pkt->pan == 0)
		log_printf(log, " pan: 0");
	else if(pkt->command & CC_PAN_LEFT)
		log_printf(log, " pan left: %d", pkt->pan);
	else if(pkt->command & CC_PAN_RIGHT)
		log_printf(log, " pan right: %d", pkt->pan);
}

/*
 * ccpacket_log_tilt	Log any tilt command in the camera control packet.
 *
 * log: message logger
 */
static inline void ccpacket_log_tilt(struct ccpacket *pkt, struct log *log) {
	if(pkt->tilt == 0)
		log_printf(log, " tilt: 0");
	else if(pkt->command & CC_TILT_UP)
		log_printf(log, " tilt up: %d", pkt->tilt);
	else if(pkt->command & CC_TILT_DOWN)
		log_printf(log, " tilt down: %d", pkt->tilt);
}

/*
 * ccpacket_log_lens	Log any lens commands in the camera control packet.
 *
 * log: message logger
 */
static inline void ccpacket_log_lens(struct ccpacket *pkt, struct log *log) {
	enum lens_t zm = ccpacket_get_zoom(pkt);
	enum lens_t fm = ccpacket_get_focus(pkt);
	enum lens_t im = ccpacket_get_iris(pkt);
	enum lens_t lm = ccpacket_get_lens(pkt);
	if (zm == CC_ZOOM_IN)
		log_printf(log, " zoom IN");
	if (zm == CC_ZOOM_OUT)
		log_printf(log, " zoom OUT");
	if (fm == CC_FOCUS_NEAR)
		log_printf(log, " focus NEAR");
	if (fm == CC_FOCUS_FAR)
		log_printf(log, " focus FAR");
	if (fm == CC_FOCUS_AUTO)
		log_printf(log, " focus AUTO");
	if (im == CC_IRIS_CLOSE)
		log_printf(log, " iris CLOSE");
	if (im == CC_IRIS_OPEN)
		log_printf(log, " iris OPEN");
	if (im == CC_IRIS_AUTO)
		log_printf(log, " iris AUTO");
	if (lm == CC_LENS_SPEED)
		log_printf(log, " lens SPEED");
}

/*
 * ccpacket_log_preset	Log any preset commands in the camera control packet.
 *
 * log: message logger
 */
static inline void ccpacket_log_preset(struct ccpacket *pkt, struct log *log) {
	if(pkt->command & CC_RECALL)
		log_printf(log, " recall");
	else if(pkt->command & CC_STORE)
		log_printf(log, " store");
	else if(pkt->command & CC_CLEAR)
		log_printf(log, " clear");
	log_printf(log, " preset: %d", pkt->preset);
}

/*
 * ccpacket_log_special	Log any special commands in the camera control packet.
 *
 * log: message logger
 */
static inline void ccpacket_log_special(struct ccpacket *pkt, struct log *log) {
	if(pkt->command & CC_AUTO_PAN)
		log_printf(log, " auto-pan");
	if(pkt->command & CC_MANUAL_PAN)
		log_printf(log, " manual-pan");
	if(pkt->command & CC_ACK_ALARM)
		log_printf(log, " ack-alarm");
	if(pkt->command & CC_MENU_OPEN)
		log_printf(log, " menu-open");
	if(pkt->command & CC_MENU_ENTER)
		log_printf(log, " menu-enter");
	if(pkt->command & CC_MENU_CANCEL)
		log_printf(log, " menu-cancel");
}

/*
 * ccpacket_log		Log the camera control packet.
 *
 * log: message logger
 */
void ccpacket_log(struct ccpacket *pkt, struct log *log, const char *dir,
	const char *name)
{
	log_line_start(log);
	log_printf(log, "packet: %s %s rcv: %d", dir, name, pkt->receiver);
	if(pkt->status)
		log_printf(log, " status: %d", pkt->status);
	ccpacket_log_pan(pkt, log);
	ccpacket_log_tilt(pkt, log);
	ccpacket_log_lens(pkt, log);
	if(pkt->aux)
		log_printf(log, " aux: %d", pkt->aux);
	if(pkt->preset)
		ccpacket_log_preset(pkt, log);
	ccpacket_log_special(pkt, log);
	log_line_end(log);
}

/*
 * ccpacket_count	Count the camera control packet statistics.
 */
void ccpacket_count(struct ccpacket *pkt) {
	ptz_stats_count(pkt);
}

/*
 * ccpacket_drop	Drop the camera control packet.
 */
void ccpacket_drop(struct ccpacket *pkt) {
	ptz_stats_drop();
	ccpacket_count(pkt);
}

/*
 * ccpacket_copy	Copy a camera control packet.
 */
void ccpacket_copy(struct ccpacket *dest, struct ccpacket *src) {
	memcpy(dest, src, sizeof(struct ccpacket));
}
