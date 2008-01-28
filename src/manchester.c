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
#include <stdbool.h>
#include <stdint.h>	/* for uint8_t */
#include "ccreader.h"
#include "manchester.h"

#define FLAG (0x80)
#define PT_COMMAND (0x02)
#define SIZE_MSG (3)

static inline bool pt_command(uint8_t *mess) {
	return (mess[2] & PT_COMMAND) != 0;
}

static inline int decode_receiver(uint8_t *mess) {
	return 1 + (((mess[0] & 0x0f) << 6) | ((mess[1] & 0x01) << 5) |
		((mess[2] >> 2) & 0x1f));
}

static inline int decode_command(uint8_t *mess) {
	return (mess[1] >> 4) & 0x03;
}

static inline int pt_extra(uint8_t *mess) {
	return (mess[1] >> 1) & 0x07;
}

/* Valid pan/tilt speeds are 0 - 6 (or 7), here's a lookup table */
static const int SPEED[] = {
	1 << 8,
	2 << 8,
	3 << 8,
	4 << 8,
	5 << 8,
	6 << 8,
	7 << 8,
	SPEED_MAX,
};

#define SPEED_FULL (0x07)

static inline int pt_speed(uint8_t *mess) {
	return SPEED[pt_extra(mess)];
}

int manchester_encode_speed(int speed) {
	int s;
	for(s = 0; s < SPEED_FULL; s++) {
		/* round up to the next higher speed level */
		if(SPEED[s] >= speed)
			return s;
	}
	return SPEED_FULL;
}

enum pt_command_t {
	TILT_DOWN,	/* 00 */
	TILT_UP,	/* 01 */
	PAN_LEFT,	/* 10 */
	PAN_RIGHT	/* 11 */
};

static inline void decode_pan_tilt(struct ccpacket *p, enum pt_command_t cmnd,
	int speed)
{
	switch(cmnd) {
		case PAN_LEFT:
			p->command |= CC_PAN_LEFT;
			p->pan = speed;
			break;
		case PAN_RIGHT:
			p->command |= CC_PAN_RIGHT;
			p->pan = speed;
			break;
		case TILT_DOWN:
			p->command |= CC_TILT_DOWN;
			p->tilt = speed;
			break;
		case TILT_UP:
			p->command |= CC_TILT_UP;
			p->tilt = speed;
			break;
	}
}

enum lens_t {
	XL_TILT_DOWN,	/* 000 (not really a lens function) */
	XL_IRIS_OPEN,	/* 001 */
	XL_FOCUS_FAR,	/* 010 */
	XL_ZOOM_IN,	/* 011 */
	XL_IRIS_CLOSE,	/* 100 */
	XL_FOCUS_NEAR,	/* 101 */
	XL_ZOOM_OUT,	/* 110 */
	XL_PAN_LEFT,	/* 111 (not really a lens function) */
};

static inline void decode_lens(struct ccpacket *p, enum lens_t extra) {
	switch(extra) {
		case XL_ZOOM_IN:
			p->zoom = ZOOM_IN;
			break;
		case XL_ZOOM_OUT:
			p->zoom = ZOOM_OUT;
			break;
		case XL_FOCUS_FAR:
			p->focus = FOCUS_FAR;
			break;
		case XL_FOCUS_NEAR:
			p->focus = FOCUS_NEAR;
			break;
		case XL_IRIS_OPEN:
			p->iris = IRIS_OPEN;
			break;
		case XL_IRIS_CLOSE:
			p->iris = IRIS_CLOSE;
			break;
		case XL_TILT_DOWN:
			/* Weird special case for full down */
			p->command |= CC_TILT_DOWN;
			p->tilt = SPEED_MAX;
			break;
		case XL_PAN_LEFT:
			/* Weird special case for full left */
			p->command |= CC_PAN_LEFT;
			p->pan = SPEED_MAX;
			break;
	}
}

static const enum aux_t AUX_LUT[] = {
	AUX_NONE,	/* 000 (full tilt up) */
	AUX_NONE,	/* 001 (full pan right) */
	AUX_1,		/* 010 */
	AUX_4,		/* 011 */
	AUX_2,		/* 100 */
	AUX_5,		/* 101 */
	AUX_3,		/* 110 */
	AUX_6		/* 111 */
};

#define EX_AUX_FULL_UP 0
#define EX_AUX_FULL_RIGHT 1

static inline void decode_aux(struct ccpacket *p, int extra) {
	if(extra == EX_AUX_FULL_UP) {
		/* Weird special case for full up */
		p->command |= CC_TILT_UP;
		p->tilt = SPEED_MAX;
	} else if(extra == EX_AUX_FULL_RIGHT) {
		/** Weird special case for full right */
		p->command |= CC_PAN_RIGHT;
		p->pan = SPEED_MAX;
	} else
		p->aux |= AUX_LUT[extra];
}

static inline void decode_recall(struct ccpacket *p, int extra) {
	p->command |= CC_RECALL;
	p->preset = extra + 1;
}

static inline void decode_store(struct ccpacket *p, int extra) {
	p->command |= CC_STORE;
	p->preset = extra + 1;
}

enum ex_function_t {
	EX_LENS,	/* 00 */
	EX_AUX,		/* 01 */
	EX_RECALL,	/* 10 */
	EX_STORE,	/* 11 */
};

static inline void decode_extended(struct ccpacket *p, enum ex_function_t cmnd,
	int extra)
{
	switch(cmnd) {
		case EX_LENS:
			decode_lens(p, extra);
			break;
		case EX_AUX:
			decode_aux(p, extra);
			break;
		case EX_RECALL:
			decode_recall(p, extra);
			break;
		case EX_STORE:
			decode_store(p, extra);
			break;
	}
}

static inline void decode_packet(struct ccpacket *p, uint8_t *mess) {
	int cmnd = decode_command(mess);
	if(pt_command(mess))
		decode_pan_tilt(p, cmnd, pt_speed(mess));
	else
		decode_extended(p, cmnd, pt_extra(mess));
}

static inline void manchester_decode_packet(struct ccreader *r, uint8_t *mess) {
	int receiver = decode_receiver(mess);
	if(r->packet.receiver != receiver)
		ccreader_process_packet(r);
	r->packet.receiver = receiver;
	decode_packet(&r->packet, mess);
}

static inline enum decode_t manchester_read_message(struct ccreader *r,
	struct buffer *rxbuf)
{
	uint8_t *mess = buffer_output(rxbuf);
	if((mess[0] & FLAG) == 0) {
		log_println(r->log, "Manchester: unexpected byte %02X",
			mess[0]);
		buffer_consume(rxbuf, 1);
		return DECODE_MORE;
	}
	manchester_decode_packet(r, mess);
	buffer_consume(rxbuf, SIZE_MSG);
	return DECODE_MORE;
}

void manchester_do_read(struct ccreader *r, struct buffer *rxbuf) {
	while(buffer_available(rxbuf) >= SIZE_MSG) {
		if(manchester_read_message(r, rxbuf) == DECODE_DONE)
			break;
	}
	/* If there's a partial packet in the buffer, don't process yet */
	if(!buffer_available(rxbuf))
		ccreader_process_packet(r);
}

static inline void encode_receiver(uint8_t *mess, int receiver) {
	int r = receiver - 1;
	mess[0] = FLAG | ((r >> 6) & 0x0f);
	mess[1] = (r >> 5) & 0x01;
	mess[2] = (r & 0x1f) << 2;
}

static void encode_pan_tilt_command(struct ccwriter *w, struct ccpacket *p,
	enum pt_command_t cmnd, int speed)
{
	uint8_t *mess = ccwriter_append(w, SIZE_MSG);
	if(mess) {
		encode_receiver(mess, p->receiver);
		mess[1] |= (cmnd << 4) | (speed << 1);
		mess[2] |= PT_COMMAND;
	}
}

static void encode_lens_function(struct ccwriter *w, struct ccpacket *p,
	enum lens_t func)
{
	uint8_t *mess = ccwriter_append(w, SIZE_MSG);
	if(mess) {
		encode_receiver(mess, p->receiver);
		mess[1] |= (func << 1) | (EX_LENS << 4);
	}
}

static void encode_aux_function(struct ccwriter *w, struct ccpacket *p,
	int aux)
{
	uint8_t *mess = ccwriter_append(w, SIZE_MSG);
	if(mess) {
		encode_receiver(mess, p->receiver);
		mess[1] |= (aux << 1) | (EX_AUX << 4);
	}
}

static void encode_pan(struct ccwriter *w, struct ccpacket *p) {
	int speed = manchester_encode_speed(p->pan);
	if(p->command & CC_PAN_LEFT) {
		if(speed == SPEED_FULL)
			encode_lens_function(w, p, XL_PAN_LEFT);
		else
			encode_pan_tilt_command(w, p, PAN_LEFT, speed);
	} else if(p->command & CC_PAN_RIGHT) {
		if(speed == SPEED_FULL)
			encode_aux_function(w, p, EX_AUX_FULL_RIGHT);
		else
			encode_pan_tilt_command(w, p, PAN_RIGHT, speed);
	}
}

static void encode_tilt(struct ccwriter *w, struct ccpacket *p) {
	int speed = manchester_encode_speed(p->tilt);
	if(p->command & CC_TILT_DOWN) {
		if(speed == SPEED_FULL)
			encode_lens_function(w, p, XL_TILT_DOWN);
		else
			encode_pan_tilt_command(w, p, TILT_DOWN, speed);
	} else if(p->command & CC_TILT_UP) {
		if(speed == SPEED_FULL)
			encode_aux_function(w, p, EX_AUX_FULL_UP);
		else
			encode_pan_tilt_command(w, p, TILT_UP, speed);
	}
}

static inline void encode_zoom(struct ccwriter *w, struct ccpacket *p) {
	if(p->zoom < 0)
		encode_lens_function(w, p, XL_ZOOM_OUT);
	else if(p->zoom > 0)
		encode_lens_function(w, p, XL_ZOOM_IN);
}

static inline void encode_focus(struct ccwriter *w, struct ccpacket *p) {
	if(p->focus < 0)
		encode_lens_function(w, p, XL_FOCUS_NEAR);
	else if(p->focus > 0)
		encode_lens_function(w, p, XL_FOCUS_FAR);
}

static inline void encode_iris(struct ccwriter *w, struct ccpacket *p) {
	if(p->iris < 0)
		encode_lens_function(w, p, XL_IRIS_CLOSE);
	else if(p->iris > 0)
		encode_lens_function(w, p, XL_IRIS_OPEN);
}

/* Masks for each aux function */
static const enum aux_t AUX_MASK[] = {
	AUX_1, AUX_2, AUX_3, AUX_4, AUX_5, AUX_6
};

/* Reverse AUX function lookup table */
static const int LUT_AUX[] = {
	2, 4, 6, 3, 5, 7
};

static inline void encode_aux(struct ccwriter *w, struct ccpacket *p) {
	int i;
	if(p->aux & AUX_CLEAR)
		return;
	for(i = 0; i < 6; i++) {
		if(p->aux & AUX_MASK[i])
			encode_aux_function(w, p, LUT_AUX[i]);
	}
}

static void encode_recall_function(struct ccwriter *w, struct ccpacket *p,
	int preset)
{
	uint8_t *mess = ccwriter_append(w, SIZE_MSG);
	if(mess) {
		encode_receiver(mess, p->receiver);
		mess[1] |= (preset << 1) | (EX_RECALL << 4);
	}
}

static void encode_store_function(struct ccwriter *w, struct ccpacket *p,
	int preset)
{
	uint8_t *mess = ccwriter_append(w, SIZE_MSG);
	if(mess) {
		encode_receiver(mess, p->receiver);
		mess[1] |= (preset << 1) | (EX_STORE << 4);
	}
}

static void encode_preset(struct ccwriter *w, struct ccpacket *p) {
	int preset = p->preset;
	if(preset < 1 || preset > 8)
		return;
	if(p->command & CC_RECALL)
		encode_recall_function(w, p, preset - 1);
	else if(p->command & CC_STORE)
		encode_store_function(w, p, preset - 1);
}

unsigned int manchester_do_write(struct ccwriter *w, struct ccpacket *p) {
	if(p->receiver < 1 || p->receiver > MANCHESTER_MAX_ADDRESS)
		return 0;
	if(p->pan)
		encode_pan(w, p);
	if(p->tilt)
		encode_tilt(w, p);
	encode_zoom(w, p);
	encode_focus(w, p);
	encode_iris(w, p);
	encode_aux(w, p);
	encode_preset(w, p);
	return 1;
}
