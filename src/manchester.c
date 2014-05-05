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
#include <stdbool.h>	/* for bool */
#include <stdint.h>	/* for uint8_t */
#include "ccreader.h"
#include "manchester.h"

#define FLAG (0x80)
#define PT_COMMAND (0x02)
#define SIZE_MSG (3)
#define SPEED_FULL (0x07)

/* Lookup table for pan/tilt speeds 0 - 6 (or 7) */
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

/* Pan/tilt command values */
enum pt_command_t {
	TILT_DOWN,	/* 00 */
	TILT_UP,	/* 01 */
	PAN_LEFT,	/* 10 */
	PAN_RIGHT	/* 11 */
};

/* Extended function values */
enum ex_function_t {
	EX_LENS,	/* 00 */
	EX_AUX,		/* 01 */
	EX_RECALL,	/* 10 */
	EX_STORE,	/* 11 */
};

/* Lens command values */
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

#define EX_AUX_FULL_UP (0)
#define EX_AUX_FULL_RIGHT (1)

/* Auxiliary command lookup table */
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

/* Masks for each aux function */
static const enum aux_t AUX_MASK[] = {
	AUX_1, AUX_2, AUX_3, AUX_4, AUX_5, AUX_6
};

/* Reverse AUX function lookup table */
static const int LUT_AUX[] = {
	2, 4, 6, 3, 5, 7
};

/*
 * is_pan_tilt_command	Test if a message is a pan/tilt command.
 */
static inline bool is_pan_tilt_command(uint8_t *mess) {
	return (mess[2] & PT_COMMAND) != 0;
}

/*
 * decode_receiver	Decode the receiver address.
 */
static inline int decode_receiver(uint8_t *mess) {
	return 1 + (((mess[0] & 0x0f) << 6) | ((mess[1] & 0x01) << 5) |
		((mess[2] >> 2) & 0x1f));
}

/*
 * decode_command	Decode the command code.
 */
static inline int decode_command(uint8_t *mess) {
	return (mess[1] >> 4) & 0x03;
}

/*
 * pt_extra		Decode the pan/tilt extra data.
 */
static inline int pt_extra(uint8_t *mess) {
	return (mess[1] >> 1) & 0x07;
}

/*
 * decode_speed		Decode pan/tilt speed.
 */
static inline int decode_speed(uint8_t *mess) {
	return SPEED[pt_extra(mess)];
}

/*
 * decode_pan_tilt	Decode pan/tilt command.
 */
static inline void decode_pan_tilt(struct ccpacket *pkt, enum pt_command_t cmnd,
	int speed)
{
	switch(cmnd) {
	case PAN_LEFT:
		pkt->command |= CC_PAN_LEFT;
		ccpacket_set_pan_speed(pkt, speed);
		break;
	case PAN_RIGHT:
		pkt->command |= CC_PAN_RIGHT;
		ccpacket_set_pan_speed(pkt, speed);
		break;
	case TILT_DOWN:
		pkt->command |= CC_TILT_DOWN;
		ccpacket_set_tilt_speed(pkt, speed);
		break;
	case TILT_UP:
		pkt->command |= CC_TILT_UP;
		ccpacket_set_tilt_speed(pkt, speed);
		break;
	}
}

/*
 * decode_lens		Decode a lens command.
 */
static inline void decode_lens(struct ccpacket *pkt, enum lens_t extra) {
	switch(extra) {
	case XL_ZOOM_IN:
		pkt->zoom = ZOOM_IN;
		break;
	case XL_ZOOM_OUT:
		pkt->zoom = ZOOM_OUT;
		break;
	case XL_FOCUS_FAR:
		pkt->focus = FOCUS_FAR;
		break;
	case XL_FOCUS_NEAR:
		pkt->focus = FOCUS_NEAR;
		break;
	case XL_IRIS_OPEN:
		pkt->iris = IRIS_OPEN;
		break;
	case XL_IRIS_CLOSE:
		pkt->iris = IRIS_CLOSE;
		break;
	case XL_TILT_DOWN:
		/* Weird special case for full down */
		pkt->command |= CC_TILT_DOWN;
		ccpacket_set_tilt_speed(pkt, SPEED_MAX);
		break;
	case XL_PAN_LEFT:
		/* Weird special case for full left */
		pkt->command |= CC_PAN_LEFT;
		ccpacket_set_pan_speed(pkt, SPEED_MAX);
		break;
	}
}

/*
 * decode_aux		Decode an auxiliary command.
 */
static inline void decode_aux(struct ccpacket *pkt, int extra) {
	if(extra == EX_AUX_FULL_UP) {
		/* Weird special case for full up */
		pkt->command |= CC_TILT_UP;
		ccpacket_set_tilt_speed(pkt, SPEED_MAX);
	} else if(extra == EX_AUX_FULL_RIGHT) {
		/** Weird special case for full right */
		pkt->command |= CC_PAN_RIGHT;
		ccpacket_set_pan_speed(pkt, SPEED_MAX);
	} else
		pkt->aux |= AUX_LUT[extra];
}

/*
 * decode_recall	Decode a preset recall command.
 */
static inline void decode_recall(struct ccpacket *pkt, int extra) {
	pkt->command |= CC_RECALL;
	pkt->preset = extra + 1;
}

/*
 * decode_extended	Decode an extended command.
 */
static inline void decode_extended(struct ccpacket *pkt,
	enum ex_function_t cmnd, int extra)
{
	switch(cmnd) {
	case EX_LENS:
		decode_lens(pkt, extra);
		break;
	case EX_AUX:
		decode_aux(pkt, extra);
		break;
	case EX_RECALL:
		decode_recall(pkt, extra);
		break;
	case EX_STORE:
		ccpacket_store_preset(pkt, extra + 1);
		break;
	}
}

/*
 * decode_packet	Decode a manchester packet.
 */
static inline void decode_packet(struct ccpacket *pkt, uint8_t *mess) {
	int cmnd = decode_command(mess);
	if(is_pan_tilt_command(mess))
		decode_pan_tilt(pkt, cmnd, decode_speed(mess));
	else
		decode_extended(pkt, cmnd, pt_extra(mess));
}

/*
 * manchester_decode_packet	Decode a manchester packet.
 */
static void manchester_decode_packet(struct ccreader *rdr, uint8_t *mess) {
	int receiver = decode_receiver(mess);
	if(ccpacket_get_receiver(&rdr->packet) != receiver)
		ccreader_process_packet(rdr);
	ccpacket_set_receiver(&rdr->packet, receiver);
	decode_packet(&rdr->packet, mess);
}

/*
 * manchester_read_message	Read one manchester packet.
 */
static inline enum decode_t manchester_read_message(struct ccreader *rdr,
	struct buffer *rxbuf)
{
	uint8_t *mess = buffer_output(rxbuf);
	if((mess[0] & FLAG) == 0) {
		log_println(rdr->log, "Manchester: unexpected byte %02X",
			mess[0]);
		buffer_consume(rxbuf, 1);
		return DECODE_MORE;
	}
	manchester_decode_packet(rdr, mess);
	buffer_consume(rxbuf, SIZE_MSG);
	return DECODE_MORE;
}

/*
 * manchester_do_read		Read packets in manchester protocol.
 */
void manchester_do_read(struct ccreader *rdr, struct buffer *rxbuf) {
	while(buffer_available(rxbuf) >= SIZE_MSG) {
		if(manchester_read_message(rdr, rxbuf) == DECODE_DONE)
			break;
	}
	/* If there's a partial packet in the buffer, don't process yet */
	if(!buffer_available(rxbuf))
		ccreader_process_packet(rdr);
}

/*
 * encode_receiver		Encode the receiver address.
 */
static inline void encode_receiver(uint8_t *mess, const struct ccpacket *pkt) {
	int rdr = ccpacket_get_receiver(pkt) - 1;
	mess[0] = FLAG | ((rdr >> 6) & 0x0f);
	mess[1] = (rdr >> 5) & 0x01;
	mess[2] = (rdr & 0x1f) << 2;
}

/*
 * encode_pan_tilt_command	Encode a pan/tilt command.
 */
static void encode_pan_tilt_command(struct ccwriter *wtr, struct ccpacket *pkt,
	enum pt_command_t cmnd, int speed)
{
	uint8_t *mess = ccwriter_append(wtr, SIZE_MSG);
	if(mess) {
		encode_receiver(mess, pkt);
		mess[1] |= (cmnd << 4) | (speed << 1);
		mess[2] |= PT_COMMAND;
	}
}

/*
 * encode_lens_function		Encode a lens function.
 */
static void encode_lens_function(struct ccwriter *wtr, struct ccpacket *pkt,
	enum lens_t func)
{
	uint8_t *mess = ccwriter_append(wtr, SIZE_MSG);
	if(mess) {
		encode_receiver(mess, pkt);
		mess[1] |= (func << 1) | (EX_LENS << 4);
	}
}

/*
 * encode_aux_function		Encode an auxiliary function.
 */
static void encode_aux_function(struct ccwriter *wtr, struct ccpacket *pkt,
	int aux)
{
	uint8_t *mess = ccwriter_append(wtr, SIZE_MSG);
	if(mess) {
		encode_receiver(mess, pkt);
		mess[1] |= (aux << 1) | (EX_AUX << 4);
	}
}

/*
 * manchester_encode_speed	Encode pan/tilt speed.
 */
static int manchester_encode_speed(int speed) {
	int s;
	for(s = 0; s < SPEED_FULL; s++) {
		/* round up to the next higher speed level */
		if(SPEED[s] >= speed)
			return s;
	}
	return SPEED_FULL;
}

/*
 * encode_pan		Encode a pan command.
 */
static void encode_pan(struct ccwriter *wtr, struct ccpacket *pkt) {
	int speed = manchester_encode_speed(ccpacket_get_pan_speed(pkt));
	if(pkt->command & CC_PAN_LEFT) {
		if(speed == SPEED_FULL)
			encode_lens_function(wtr, pkt, XL_PAN_LEFT);
		else
			encode_pan_tilt_command(wtr, pkt, PAN_LEFT, speed);
	} else if(pkt->command & CC_PAN_RIGHT) {
		if(speed == SPEED_FULL)
			encode_aux_function(wtr, pkt, EX_AUX_FULL_RIGHT);
		else
			encode_pan_tilt_command(wtr, pkt, PAN_RIGHT, speed);
	}
}

/*
 * encode_tilt		Encode a tilt command.
 */
static void encode_tilt(struct ccwriter *wtr, struct ccpacket *pkt) {
	int speed = manchester_encode_speed(ccpacket_get_tilt_speed(pkt));
	if(pkt->command & CC_TILT_DOWN) {
		if(speed == SPEED_FULL)
			encode_lens_function(wtr, pkt, XL_TILT_DOWN);
		else
			encode_pan_tilt_command(wtr, pkt, TILT_DOWN, speed);
	} else if(pkt->command & CC_TILT_UP) {
		if(speed == SPEED_FULL)
			encode_aux_function(wtr, pkt, EX_AUX_FULL_UP);
		else
			encode_pan_tilt_command(wtr, pkt, TILT_UP, speed);
	}
}

/*
 * encode_zoom		Encode a zoom command.
 */
static inline void encode_zoom(struct ccwriter *wtr, struct ccpacket *pkt) {
	if(pkt->zoom < 0)
		encode_lens_function(wtr, pkt, XL_ZOOM_OUT);
	else if(pkt->zoom > 0)
		encode_lens_function(wtr, pkt, XL_ZOOM_IN);
}

/*
 * encode_focus		Encode a focus command.
 */
static inline void encode_focus(struct ccwriter *wtr, struct ccpacket *pkt) {
	if(pkt->focus < 0)
		encode_lens_function(wtr, pkt, XL_FOCUS_NEAR);
	else if(pkt->focus > 0)
		encode_lens_function(wtr, pkt, XL_FOCUS_FAR);
}

/*
 * encode_iris		Encode an iris command.
 */
static inline void encode_iris(struct ccwriter *wtr, struct ccpacket *pkt) {
	if(pkt->iris < 0)
		encode_lens_function(wtr, pkt, XL_IRIS_CLOSE);
	else if(pkt->iris > 0)
		encode_lens_function(wtr, pkt, XL_IRIS_OPEN);
}

/*
 * encode_aux		Encode an auxiliary command.
 */
static inline void encode_aux(struct ccwriter *wtr, struct ccpacket *pkt) {
	int i;
	if(pkt->aux & AUX_CLEAR)
		return;
	for(i = 0; i < 6; i++) {
		if(pkt->aux & AUX_MASK[i])
			encode_aux_function(wtr, pkt, LUT_AUX[i]);
	}
}

/*
 * encode_recall_function	Encode a recall preset function.
 */
static void encode_recall_function(struct ccwriter *wtr, struct ccpacket *pkt,
	int preset)
{
	uint8_t *mess = ccwriter_append(wtr, SIZE_MSG);
	if(mess) {
		encode_receiver(mess, pkt);
		mess[1] |= (preset << 1) | (EX_RECALL << 4);
	}
}

/*
 * encode_store_function	Encode a store preset function.
 */
static void encode_store_function(struct ccwriter *wtr, struct ccpacket *pkt,
	int preset)
{
	uint8_t *mess = ccwriter_append(wtr, SIZE_MSG);
	if(mess) {
		encode_receiver(mess, pkt);
		mess[1] |= (preset << 1) | (EX_STORE << 4);
	}
}

/*
 * encode_preset	Encode a preset command.
 */
static void encode_preset(struct ccwriter *wtr, struct ccpacket *pkt) {
	int preset = pkt->preset;
	if(preset < 1 || preset > 8)
		return;
	if(pkt->command & CC_RECALL)
		encode_recall_function(wtr, pkt, preset - 1);
	else if(pkt->command & CC_STORE)
		encode_store_function(wtr, pkt, preset - 1);
}

/*
 * manchester_do_write	Write a packet in manchester protocol.
 */
unsigned int manchester_do_write(struct ccwriter *wtr, struct ccpacket *pkt) {
	int receiver = ccpacket_get_receiver(pkt);
	if(receiver < 1 || receiver > MANCHESTER_MAX_ADDRESS)
		return 0;
	if(ccpacket_get_pan_speed(pkt))
		encode_pan(wtr, pkt);
	if(ccpacket_get_tilt_speed(pkt))
		encode_tilt(wtr, pkt);
	encode_zoom(wtr, pkt);
	encode_focus(wtr, pkt);
	encode_iris(wtr, pkt);
	encode_aux(wtr, pkt);
	encode_preset(wtr, pkt);
	return 1;
}
