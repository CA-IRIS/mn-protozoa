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
#include "pelco_d.h"
#include "bitarray.h"

#define FLAG (0xff)
#define SIZE_MSG (7)
#define TURBO_SPEED (1 << 6)

/*
 * Packet bit positions for PTZ functions.
 */
enum pelco_bit_t {
	BIT_FOCUS_NEAR = 16,
	BIT_IRIS_OPEN = 17,
	BIT_IRIS_CLOSE = 18,
	BIT_CAMERA_ON_OFF = 19,
	BIT_AUTO_PAN = 20,
	BIT_SENSE = 23,
	BIT_EXTENDED = 24,
	BIT_PAN_RIGHT = 25,
	BIT_PAN_LEFT = 26,
	BIT_TILT_UP = 27,
	BIT_TILT_DOWN = 28,
	BIT_ZOOM_IN = 29,
	BIT_ZOOM_OUT = 30,
	BIT_FOCUS_FAR = 31,
};

/*
 * Extended pelco_d functions
 */
enum extended_t {
	EX_NONE,		/* 00000 no function */
	EX_STORE,		/* 00001 store preset */
	EX_CLEAR,		/* 00010 clear preset */
	EX_RECALL,		/* 00011 recall preset */
	EX_AUX_SET,		/* 00100 set auxilliary */
	EX_AUX_CLEAR,		/* 00101 clear auxilliary */
	EX_RESERVED,		/* 00110 reserved */
	EX_RESET,		/* 00111 remote reset */
	EX_ZONE_START,		/* 01000 set zone start */
	EX_ZONE_END,		/* 01001 set zone end */
	EX_CHAR_WRITE,		/* 01010 write character */
	EX_CHAR_CLEAR,		/* 01011 clear all characters */
	EX_ACK_ALARM,		/* 01100 acknowledge alarm */
	EX_ZONE_SCAN_ON,	/* 01101 zone scan on */
	EX_ZONE_SCAN_OFF,	/* 01110 zone scan off */
	EX_PATTERN_START,	/* 01111 set pattern start */
	EX_PATTERN_STOP,	/* 10000 set pattern stop */
	EX_PATTERN_RUN,		/* 10001 run pattern */
	EX_ZOOM_SPEED,		/* 10010 set zoom speed */
	EX_FOCUS_SPEED,		/* 10011 set focus speed */
};

/*
 * Test if the packet is an extended function
 */
static inline bool is_extended(uint8_t *mess) {
	return bit_is_set(mess, BIT_EXTENDED);
}

/*
 * Test if the packet has a sense function
 */
static inline bool has_sense(struct ccpacket *p) {
	if(p->command & (CC_AUTO_PAN | CC_MANUAL_PAN))
		return true;
	if(p->command & (CC_CAMERA_ON | CC_CAMERA_OFF))
		return true;
	return false;
}

/*
 * Calculate the checksum for a pelco_d packet
 */
static uint8_t calculate_checksum(uint8_t *mess) {
	int i;
	int checksum = 0;
	for(i = 1; i < 6; i++)
		checksum += mess[i];
	return checksum;
}

/*
 * Decode the receiver address from a pelco_d packet
 */
static inline int decode_receiver(uint8_t *mess) {
	return mess[1];
}

/*
 * Decode a pan or tilt speed
 */
static inline int decode_speed(uint8_t val) {
	int speed = val << 5;
	if(speed > SPEED_MAX)
		return SPEED_MAX;
	else
		return speed;
}

/*
 * Decode the pan speed (and command)
 */
static inline void decode_pan(struct ccpacket *p, uint8_t *mess) {
	int pan = decode_speed(mess[4]);
	if(bit_is_set(mess, BIT_PAN_RIGHT)) {
		p->command |= CC_PAN_RIGHT;
		p->pan = pan;
	} else if(bit_is_set(mess, BIT_PAN_LEFT)) {
		p->command |= CC_PAN_LEFT;
		p->pan = pan;
	} else {
		p->command |= CC_PAN_LEFT;
		p->pan = 0;
	}
}

/*
 * Decode the tilt speed (and command)
 */
static inline void decode_tilt(struct ccpacket *p, uint8_t *mess) {
	int tilt = decode_speed(mess[5]);
	if(bit_is_set(mess, BIT_TILT_UP)) {
		p->command |= CC_TILT_UP;
		p->tilt = tilt;
	} else if(bit_is_set(mess, BIT_TILT_DOWN)) {
		p->command |= CC_TILT_DOWN;
		p->tilt = tilt;
	} else {
		p->command |= CC_TILT_DOWN;
		p->tilt = 0;
	}
}

/*
 * Decode a lens command
 */
static inline void decode_lens(struct ccpacket *p, uint8_t *mess) {
	if(bit_is_set(mess, BIT_IRIS_OPEN))
		p->iris = IRIS_OPEN;
	else if(bit_is_set(mess, BIT_IRIS_CLOSE))
		p->iris = IRIS_CLOSE;
	if(bit_is_set(mess, BIT_FOCUS_NEAR))
		p->focus = FOCUS_NEAR;
	else if(bit_is_set(mess, BIT_FOCUS_FAR))
		p->focus = FOCUS_FAR;
	if(bit_is_set(mess, BIT_ZOOM_IN))
		p->zoom = ZOOM_IN;
	else if(bit_is_set(mess, BIT_ZOOM_OUT))
		p->zoom = ZOOM_OUT;
}

/*
 * Decode a sense command
 */
static inline void decode_sense(struct ccpacket *p, uint8_t *mess) {
	if(bit_is_set(mess, BIT_SENSE)) {
		if(bit_is_set(mess, BIT_CAMERA_ON_OFF))
			p->command |= CC_CAMERA_ON;
		if(bit_is_set(mess, BIT_AUTO_PAN))
			p->command |= CC_AUTO_PAN;
	} else {
		if(bit_is_set(mess, BIT_CAMERA_ON_OFF))
			p->command |= CC_CAMERA_OFF;
		if(bit_is_set(mess, BIT_AUTO_PAN))
			p->command |= CC_MANUAL_PAN;
	}
}

/*
 * Decode a pelco_d command
 */
static inline enum decode_t pelco_decode_command(struct ccreader *r,
	uint8_t *mess)
{
	r->packet.receiver = decode_receiver(mess);
	decode_pan(&r->packet, mess);
	decode_tilt(&r->packet, mess);
	decode_lens(&r->packet, mess);
	decode_sense(&r->packet, mess);
	ccreader_process_packet(r);
	return DECODE_MORE;
}

/*
 * Decode an auxiliary command
 */
static enum aux_t decode_aux(int a) {
	switch(a) {
		case 1:
			return AUX_1;
		case 2:
			return AUX_2;
		case 3:
			return AUX_3;
		case 4:
			return AUX_4;
		case 5:
			return AUX_5;
		case 6:
			return AUX_6;
		case 7:
			return AUX_7;
		case 8:
			return AUX_8;
	}
	return AUX_NONE;
}

/*
 * Decode an extended command
 */
static inline void decode_extended(struct ccpacket *p, enum extended_t ex,
	int p0, int p1)
{
	switch(ex) {
		case EX_STORE:
			p->command |= CC_STORE;
			p->preset = p0;
			break;
		case EX_RECALL:
			p->command |= CC_RECALL;
			p->preset = p0;
			break;
		case EX_CLEAR:
			p->command |= CC_CLEAR;
			p->preset = p0;
			break;
		case EX_AUX_SET:
			p->aux = decode_aux(p0);
			break;
		case EX_AUX_CLEAR:
			p->aux = decode_aux(p0) | AUX_CLEAR;
			break;
		/* FIXME: add other extended functions */
		default:
			break;
	}
}

/*
 * Decode an extended message
 */
static inline enum decode_t pelco_decode_extended(struct ccreader *r,
	uint8_t *mess)
{
	r->packet.receiver = decode_receiver(mess);
	int ex = mess[3] >> 1 & 0x1f;
	int p0 = mess[5];
	int p1 = mess[4];
	decode_extended(&r->packet, ex, p0, p1);
	ccreader_process_packet(r);
	return DECODE_MORE;
}

/*
 * Test if a message checksum is invalid
 */
static inline bool checksum_invalid(uint8_t *mess) {
	return calculate_checksum(mess) != mess[6];
}

/*
 * Decode a pelco_d message
 */
static inline enum decode_t pelco_decode_message(struct ccreader *r,
	struct buffer *rxbuf)
{
	uint8_t *mess = buffer_output(rxbuf);
	if(mess[0] != FLAG) {
		log_println(r->log, "Pelco(D): unexpected byte %02X", mess[0]);
		buffer_consume(rxbuf, 1);
		return DECODE_MORE;
	}
	buffer_consume(rxbuf, SIZE_MSG);
	if(checksum_invalid(mess)) {
		log_println(r->log, "Pelco(D): invalid checksum");
		return DECODE_MORE;
	}
	if(is_extended(mess))
		return pelco_decode_extended(r, mess);
	else
		return pelco_decode_command(r, mess);
}

/*
 * Read messages in pelco_d protocol
 */
void pelco_d_do_read(struct ccreader *r, struct buffer *rxbuf) {
	while(buffer_available(rxbuf) >= SIZE_MSG) {
		if(pelco_decode_message(r, rxbuf) == DECODE_DONE)
			break;
	}
}

/*
 * Encode the receiver address
 */
static inline void encode_receiver(uint8_t *mess, int receiver) {
	mess[0] = FLAG;
	mess[1] = receiver;
}

/*
 * Encode pan or tilt speed
 */
static int pelco_d_encode_speed(int speed) {
	int s = speed >> 5;
	/* round up to the next speed level */
	if(speed % 32)
		s++;
	if(s < TURBO_SPEED)
		return s;
	else
		return TURBO_SPEED - 1;
}

/*
 * Encode the pan speed and command
 */
static void encode_pan(uint8_t *mess, struct ccpacket *p) {
	int pan = pelco_d_encode_speed(p->pan);
	if(p->pan > SPEED_MAX - 8)
		pan = TURBO_SPEED;
	mess[4] = pan;
	if(pan) {
		if(p->command & CC_PAN_LEFT)
			bit_set(mess, BIT_PAN_LEFT);
		else if(p->command & CC_PAN_RIGHT)
			bit_set(mess, BIT_PAN_RIGHT);
		else
			mess[4] = 0;
	}
}

/*
 * Encode the tilt speed and command
 */
static void encode_tilt(uint8_t *mess, struct ccpacket *p) {
	int tilt = pelco_d_encode_speed(p->tilt);
	mess[5] = tilt;
	if(tilt) {
		if(p->command & CC_TILT_UP)
			bit_set(mess, BIT_TILT_UP);
		else if(p->command & CC_TILT_DOWN)
			bit_set(mess, BIT_TILT_DOWN);
		else
			mess[5] = 0;
	}
}

/*
 * Encode the lens commands
 */
static void encode_lens(uint8_t *mess, struct ccpacket *p) {
	if(p->iris == IRIS_OPEN)
		bit_set(mess, BIT_IRIS_OPEN);
	else if(p->iris == IRIS_CLOSE)
		bit_set(mess, BIT_IRIS_CLOSE);
	if(p->focus == FOCUS_NEAR)
		bit_set(mess, BIT_FOCUS_NEAR);
	else if(p->focus == FOCUS_FAR)
		bit_set(mess, BIT_FOCUS_FAR);
	if(p->zoom == ZOOM_IN)
		bit_set(mess, BIT_ZOOM_IN);
	else if(p->zoom == ZOOM_OUT)
		bit_set(mess, BIT_ZOOM_OUT);
}

/*
 * Encode a sense command
 */
static inline void encode_sense(uint8_t *mess, struct ccpacket *p) {
	if(p->command & (CC_CAMERA_ON | CC_AUTO_PAN)) {
		bit_set(mess, BIT_SENSE);
		if(p->command & CC_CAMERA_ON)
			bit_set(mess, BIT_CAMERA_ON_OFF);
		if(p->command & CC_AUTO_PAN)
			bit_set(mess, BIT_AUTO_PAN);
	} else if(p->command & (CC_CAMERA_OFF | CC_MANUAL_PAN)) {
		if(p->command & CC_CAMERA_OFF)
			bit_set(mess, BIT_CAMERA_ON_OFF);
		if(p->command & CC_MANUAL_PAN)
			bit_set(mess, BIT_AUTO_PAN);
	}
}

/*
 * Encode the message checksum
 */
static inline void encode_checksum(uint8_t *mess) {
	mess[6] = calculate_checksum(mess);
}

/*
 * Encode a command message
 */
static void encode_command(struct ccwriter *w, struct ccpacket *p) {
	uint8_t *mess = ccwriter_append(w, SIZE_MSG);
	if(mess) {
		encode_receiver(mess, p->receiver);
		encode_pan(mess, p);
		encode_tilt(mess, p);
		encode_lens(mess, p);
		encode_sense(mess, p);
		encode_checksum(mess);
	}
}

/*
 * Encode a preset message
 */
static void encode_preset(struct ccwriter *w, struct ccpacket *p) {
	uint8_t *mess = ccwriter_append(w, SIZE_MSG);
	if(mess) {
		encode_receiver(mess, p->receiver);
		bit_set(mess, BIT_EXTENDED);
		if(p->command & CC_RECALL)
			mess[3] |= EX_RECALL << 1;
		else if(p->command & CC_STORE)
			mess[3] |= EX_STORE << 1;
		else if(p->command & CC_CLEAR)
			mess[3] |= EX_CLEAR << 1;
		mess[5] = p->preset;
		encode_checksum(mess);
	}
}

/*
 * Encode an auxiliary command
 */
static void encode_aux(struct ccwriter *w, struct ccpacket *p) {
	uint8_t *mess = ccwriter_append(w, SIZE_MSG);
	if(mess) {
		encode_receiver(mess, p->receiver);
		bit_set(mess, BIT_EXTENDED);
		if(p->aux & AUX_CLEAR)
			mess[3] |= EX_AUX_CLEAR << 1;
		else
			mess[3] |= EX_AUX_SET << 1;
		/* FIXME: use a lookup table; loop through bits */
		if(p->aux & AUX_1)
			mess[5] = 1;
		else if(p->aux & AUX_2)
			mess[5] = 2;
		else if(p->aux & AUX_3)
			mess[5] = 3;
		else if(p->aux & AUX_4)
			mess[5] = 4;
		else if(p->aux & AUX_5)
			mess[5] = 5;
		else if(p->aux & AUX_6)
			mess[5] = 6;
		else if(p->aux & AUX_7)
			mess[5] = 7;
		else if(p->aux & AUX_8)
			mess[5] = 8;
		encode_checksum(mess);
	}
}

/*
 * Write a packet in the pelco_d protocol
 */
unsigned int pelco_d_do_write(struct ccwriter *w, struct ccpacket *p) {
	if(p->receiver < 1 || p->receiver > PELCO_D_MAX_ADDRESS)
		return 0;
	if(ccpacket_has_command(p) || has_sense(p))
		encode_command(w, p);
	if(ccpacket_has_preset(p))
		encode_preset(w, p);
	if(ccpacket_has_aux(p))
		encode_aux(w, p);
	return 1;
}
