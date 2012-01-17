/*
 * protozoa -- CCTV transcoder / mixer for PTZ
 * Copyright (C) 2006-2012  Minnesota Department of Transportation
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
#include "pelco_d.h"
#include "bitarray.h"

#define FLAG (0xff)
#define TURBO_SPEED (1 << 6)

enum pelco_special_presets {
	PELCO_PRESET_MENU_OPEN = 95,
};

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
	EX_AUX_SET,		/* 00100 set auxiliary */
	EX_AUX_CLEAR,		/* 00101 clear auxiliary */
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
 * calculate_checksum	Calculate the checksum for a pelco_d packet.
 */
static uint8_t calculate_checksum(uint8_t *mess) {
	int i;
	int checksum = 0;
	for(i = 1; i < 6; i++)
		checksum += mess[i];
	return checksum;
}

/*
 * decode_receiver	Decode the receiver address from a pelco_d packet.
 */
static inline int decode_receiver(uint8_t *mess) {
	return mess[1];
}

/*
 * decode_speed		Decode pan or tilt speed.
 */
static inline int decode_speed(uint8_t val) {
	int speed = val << 5;
	if(speed > SPEED_MAX)
		return SPEED_MAX;
	else
		return speed;
}

/*
 * decode_pan		Decode the pan speed (and command).
 */
static inline void decode_pan(struct ccpacket *pkt, uint8_t *mess) {
	int pan = decode_speed(mess[4]);
	if(bit_is_set(mess, BIT_PAN_RIGHT)) {
		pkt->command |= CC_PAN_RIGHT;
		pkt->pan = pan;
	} else if(bit_is_set(mess, BIT_PAN_LEFT)) {
		pkt->command |= CC_PAN_LEFT;
		pkt->pan = pan;
	} else {
		pkt->command |= CC_PAN_LEFT;
		pkt->pan = 0;
	}
}

/*
 * decode_tilt		Decode the tilt speed (and command).
 */
static inline void decode_tilt(struct ccpacket *pkt, uint8_t *mess) {
	int tilt = decode_speed(mess[5]);
	if(bit_is_set(mess, BIT_TILT_UP)) {
		pkt->command |= CC_TILT_UP;
		pkt->tilt = tilt;
	} else if(bit_is_set(mess, BIT_TILT_DOWN)) {
		pkt->command |= CC_TILT_DOWN;
		pkt->tilt = tilt;
	} else {
		pkt->command |= CC_TILT_DOWN;
		pkt->tilt = 0;
	}
}

/*
 * decode_lens		Decode a lens command.
 */
static inline void decode_lens(struct ccpacket *pkt, uint8_t *mess) {
	if(bit_is_set(mess, BIT_IRIS_OPEN))
		pkt->iris = IRIS_OPEN;
	else if(bit_is_set(mess, BIT_IRIS_CLOSE))
		pkt->iris = IRIS_CLOSE;
	if(bit_is_set(mess, BIT_FOCUS_NEAR))
		pkt->focus = FOCUS_NEAR;
	else if(bit_is_set(mess, BIT_FOCUS_FAR))
		pkt->focus = FOCUS_FAR;
	if(bit_is_set(mess, BIT_ZOOM_IN))
		pkt->zoom = ZOOM_IN;
	else if(bit_is_set(mess, BIT_ZOOM_OUT))
		pkt->zoom = ZOOM_OUT;
}

/*
 * decode_sense		Decode a sense command.
 */
static inline void decode_sense(struct ccpacket *pkt, uint8_t *mess) {
	if(bit_is_set(mess, BIT_SENSE)) {
		if(bit_is_set(mess, BIT_CAMERA_ON_OFF))
			pkt->command |= CC_CAMERA_ON;
		if(bit_is_set(mess, BIT_AUTO_PAN))
			pkt->command |= CC_AUTO_PAN;
	} else {
		if(bit_is_set(mess, BIT_CAMERA_ON_OFF))
			pkt->command |= CC_CAMERA_OFF;
		if(bit_is_set(mess, BIT_AUTO_PAN))
			pkt->command |= CC_MANUAL_PAN;
	}
}

/*
 * pelco_decode_command	Decode a pelco_d command.
 */
static inline enum decode_t pelco_decode_command(struct ccreader *rdr,
	uint8_t *mess)
{
	rdr->packet.receiver = decode_receiver(mess);
	decode_pan(&rdr->packet, mess);
	decode_tilt(&rdr->packet, mess);
	decode_lens(&rdr->packet, mess);
	decode_sense(&rdr->packet, mess);
	ccreader_process_packet(rdr);
	return DECODE_MORE;
}

/*
 * decode_aux		Decode an auxiliary command.
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
 * decode_extended	Decode an extended command.
 */
static inline void decode_extended(struct ccpacket *pkt, enum extended_t ex,
	int p0, int p1)
{
	switch(ex) {
	case EX_STORE:
		ccpacket_store_preset(pkt, p0);
		break;
	case EX_RECALL:
		pkt->command |= CC_RECALL;
		pkt->preset = p0;
		break;
	case EX_CLEAR:
		pkt->command |= CC_CLEAR;
		pkt->preset = p0;
		break;
	case EX_AUX_SET:
		pkt->aux = decode_aux(p0);
		break;
	case EX_AUX_CLEAR:
		pkt->aux = decode_aux(p0) | AUX_CLEAR;
		break;
	/* FIXME: add other extended functions */
	default:
		break;
	}
}

/*
 * pelco_decode_extended	Decode an extended message.
 */
static inline enum decode_t pelco_decode_extended(struct ccreader *rdr,
	uint8_t *mess)
{
	rdr->packet.receiver = decode_receiver(mess);
	int ex = mess[3] >> 1 & 0x1f;
	int p0 = mess[5];
	int p1 = mess[4];
	decode_extended(&rdr->packet, ex, p0, p1);
	ccreader_process_packet(rdr);
	return DECODE_MORE;
}

/*
 * checksum_is_valid	Test if a message checksum is valid.
 */
static inline bool checksum_is_valid(uint8_t *mess) {
	return calculate_checksum(mess) == mess[6];
}

/*
 * pelco_decode_message	Decode a pelco_d message.
 */
static inline enum decode_t pelco_decode_message(struct ccreader *rdr,
	struct buffer *rxbuf)
{
	uint8_t *mess = buffer_output(rxbuf);
	if(mess[0] != FLAG) {
		log_println(rdr->log, "Pelco(D): unexpected byte %02X",
			mess[0]);
		buffer_consume(rxbuf, 1);
		return DECODE_MORE;
	}
	buffer_consume(rxbuf, PELCO_D_SZ);
	if(!checksum_is_valid(mess)) {
		log_println(rdr->log, "Pelco(D): invalid checksum");
		return DECODE_MORE;
	}
	if(bit_is_set(mess, BIT_EXTENDED))
		return pelco_decode_extended(rdr, mess);
	else
		return pelco_decode_command(rdr, mess);
}

/*
 * pelco_d_do_read	Read messages in pelco_d protocol.
 */
void pelco_d_do_read(struct ccreader *rdr, struct buffer *rxbuf) {
	while(buffer_available(rxbuf) >= PELCO_D_SZ) {
		if(pelco_decode_message(rdr, rxbuf) == DECODE_DONE)
			break;
	}
}

/*
 * encode_receiver	Encode the receiver address.
 */
static inline void encode_receiver(uint8_t *mess, int receiver) {
	mess[0] = FLAG;
	mess[1] = receiver;
}

/*
 * pelco_d_encode_speed	Encode pan or tilt speed.
 */
static int pelco_d_encode_speed(int speed) {
	/* round to the nearest speed level */
	int s = (speed >> 5) + ((speed % 32) >> 4);
	if(s < TURBO_SPEED)
		return s;
	else
		return TURBO_SPEED - 1;
}

/*
 * encode_pan		Encode the pan speed and command.
 */
static void encode_pan(uint8_t *mess, struct ccpacket *pkt) {
	int pan = pelco_d_encode_speed(pkt->pan);
	if(pkt->pan > SPEED_MAX - 8)
		pan = TURBO_SPEED;
	mess[4] = pan;
	if(pan) {
		if(pkt->command & CC_PAN_LEFT)
			bit_set(mess, BIT_PAN_LEFT);
		else if(pkt->command & CC_PAN_RIGHT)
			bit_set(mess, BIT_PAN_RIGHT);
		else
			mess[4] = 0;
	}
}

/*
 * encode_tilt		Encode the tilt speed and command.
 */
static void encode_tilt(uint8_t *mess, struct ccpacket *pkt) {
	int tilt = pelco_d_encode_speed(pkt->tilt);
	mess[5] = tilt;
	if(tilt) {
		if(pkt->command & CC_TILT_UP)
			bit_set(mess, BIT_TILT_UP);
		else if(pkt->command & CC_TILT_DOWN)
			bit_set(mess, BIT_TILT_DOWN);
		else
			mess[5] = 0;
	}
}

/*
 * encode_lens		Encode the lens commands.
 */
static void encode_lens(uint8_t *mess, struct ccpacket *pkt) {
	if(pkt->iris == IRIS_OPEN)
		bit_set(mess, BIT_IRIS_OPEN);
	else if(pkt->iris == IRIS_CLOSE)
		bit_set(mess, BIT_IRIS_CLOSE);
	if(pkt->focus == FOCUS_NEAR)
		bit_set(mess, BIT_FOCUS_NEAR);
	else if(pkt->focus == FOCUS_FAR)
		bit_set(mess, BIT_FOCUS_FAR);
	if(pkt->zoom == ZOOM_IN)
		bit_set(mess, BIT_ZOOM_IN);
	else if(pkt->zoom == ZOOM_OUT)
		bit_set(mess, BIT_ZOOM_OUT);
}

/*
 * encode_sense		Encode a sense command.
 */
static inline void encode_sense(uint8_t *mess, struct ccpacket *pkt) {
	if(pkt->command & (CC_CAMERA_ON | CC_AUTO_PAN)) {
		bit_set(mess, BIT_SENSE);
		if(pkt->command & CC_CAMERA_ON)
			bit_set(mess, BIT_CAMERA_ON_OFF);
		if(pkt->command & CC_AUTO_PAN)
			bit_set(mess, BIT_AUTO_PAN);
	} else if(pkt->command & (CC_CAMERA_OFF | CC_MANUAL_PAN)) {
		if(pkt->command & CC_CAMERA_OFF)
			bit_set(mess, BIT_CAMERA_ON_OFF);
		if(pkt->command & CC_MANUAL_PAN)
			bit_set(mess, BIT_AUTO_PAN);
	}
}

/*
 * encode_checksum	Encode the message checksum.
 */
static inline void encode_checksum(uint8_t *mess) {
	mess[6] = calculate_checksum(mess);
}

/*
 * encode_command	Encode a command message.
 */
static void encode_command(struct ccwriter *wtr, struct ccpacket *pkt) {
	uint8_t *mess = ccwriter_append(wtr, PELCO_D_SZ);
	if(mess) {
		encode_receiver(mess, pkt->receiver);
		encode_pan(mess, pkt);
		encode_tilt(mess, pkt);
		encode_lens(mess, pkt);
		encode_sense(mess, pkt);
		encode_checksum(mess);
	}
}

/*
 * encode_preset	Encode a preset message.
 */
static void encode_preset(struct ccwriter *wtr, struct ccpacket *pkt) {
	uint8_t *mess = ccwriter_append(wtr, PELCO_D_SZ);
	if(mess) {
		encode_receiver(mess, pkt->receiver);
		bit_set(mess, BIT_EXTENDED);
		if(pkt->command & CC_RECALL)
			mess[3] |= EX_RECALL << 1;
		else if(pkt->command & CC_STORE)
			mess[3] |= EX_STORE << 1;
		else if(pkt->command & CC_CLEAR)
			mess[3] |= EX_CLEAR << 1;
		mess[5] = pkt->preset;
		encode_checksum(mess);
	}
}

/*
 * encode_aux		Encode an auxiliary command.
 */
static void encode_aux(struct ccwriter *wtr, struct ccpacket *pkt) {
	uint8_t *mess = ccwriter_append(wtr, PELCO_D_SZ);
	if(mess) {
		encode_receiver(mess, pkt->receiver);
		bit_set(mess, BIT_EXTENDED);
		/* FIXME: other protocols don't send an AUX_CLEAR ... */
		if(pkt->aux & AUX_CLEAR)
			mess[3] |= EX_AUX_CLEAR << 1;
		else
			mess[3] |= EX_AUX_SET << 1;
		/* FIXME: use a lookup table; loop through bits */
		if(pkt->aux & AUX_1)
			mess[5] = 1;
		else if(pkt->aux & AUX_2)
			mess[5] = 2;
		else if(pkt->aux & AUX_3)
			mess[5] = 3;
		else if(pkt->aux & AUX_4)
			mess[5] = 4;
		else if(pkt->aux & AUX_5)
			mess[5] = 5;
		else if(pkt->aux & AUX_6)
			mess[5] = 6;
		else if(pkt->aux & AUX_7)
			mess[5] = 7;
		else if(pkt->aux & AUX_8)
			mess[5] = 8;
		encode_checksum(mess);
	}
}

/*
 * adjust_menu_commands	Adjust menu commands for pelco d protocol.
 */
static inline void adjust_menu_commands(struct ccpacket *pkt) {
	if(pkt->command & CC_MENU_OPEN) {
		pkt->command |= CC_STORE;
		pkt->preset = PELCO_PRESET_MENU_OPEN;
	} else if(pkt->command & CC_MENU_ENTER)
		pkt->iris = IRIS_OPEN;
	else if(pkt->command & CC_MENU_CANCEL)
		pkt->iris = IRIS_CLOSE;
}

/*
 * pelco_d_do_write_cb	Write a packet in the pelco_d protocol.
 */
unsigned int pelco_d_do_write_cb(struct ccwriter *wtr, struct ccpacket *pkt,
	ccwriter_cb *prepare_writer)
{
	if(pkt->receiver < 1 || pkt->receiver > PELCO_D_MAX_ADDRESS)
		return 0;
	adjust_menu_commands(pkt);
	if(ccpacket_has_command(pkt) || ccpacket_has_autopan(pkt) ||
	   ccpacket_has_power(pkt))
	{
		if(prepare_writer(wtr))
			encode_command(wtr, pkt);
	}
	if(ccpacket_has_preset(pkt)) {
		if(prepare_writer(wtr))
			encode_preset(wtr, pkt);
	}
	if(ccpacket_has_aux(pkt)) {
		if(prepare_writer(wtr))
			encode_aux(wtr, pkt);
	}
	return 1;
}

/*
 * pelco_d_prepare_true	Prepare a write for pelco_d protocol.
 */
static int pelco_d_prepare_true(struct ccwriter *wtr) {
	return 1;
}

/*
 * pelco_d_do_write	Write a packet in the pelco_d protocol.
 */
unsigned int pelco_d_do_write(struct ccwriter *wtr, struct ccpacket *pkt) {
	return pelco_d_do_write_cb(wtr, pkt, pelco_d_prepare_true);
}
