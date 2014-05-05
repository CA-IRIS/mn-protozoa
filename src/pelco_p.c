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
#include <string.h>	/* for strlen, strncat */
#include "ccreader.h"
#include "pelco_p.h"
#include "bitarray.h"

#define STX (0xA0)	/* start transmission */
#define ETX (0xAF)	/* end transmission */
#define SIZE_MSG (8)
#define TURBO_SPEED (1 << 6)

enum pelco_special_presets {
	PELCO_PRESET_MENU_OPEN = 95,
};

/*
 * Packet bit positions for PTZ functions.
 */
enum pelco_p_bit_t {
	BIT_FOCUS_FAR = 16,
	BIT_FOCUS_NEAR = 17,
	BIT_IRIS_OPEN = 18,
	BIT_IRIS_CLOSE = 19,
	BIT_CAMERA_ON_OFF = 20,
	BIT_AUTO_PAN = 21,
	BIT_CAMERA_ON = 22,
	BIT_EXTENDED = 24,
	BIT_PAN_RIGHT = 25,
	BIT_PAN_LEFT = 26,
	BIT_TILT_UP = 27,
	BIT_TILT_DOWN = 28,
	BIT_ZOOM_IN = 29,
	BIT_ZOOM_OUT = 30,
};

/*
 * Extended pelco_p functions
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
 * calculate_checksum	Calculate the checksum for a pelco_p packet.
 */
static uint8_t calculate_checksum(uint8_t *mess) {
	int i;
	int checksum = 0;
	for(i = 0; i < 7; i++)
		checksum ^= mess[i];
	return checksum;
}

/**
 * Decode the receiver address from a pelco_p packet.
 *
 * @param pkt		Packet.
 * @param mess		Message buffer.
 */
static inline void decode_receiver(struct ccpacket *pkt, uint8_t *mess) {
	ccpacket_set_receiver(pkt, mess[1]);
}

/*
 * decode_speed0	Decode pan/tilt speed with no deadzone.
 */
static inline int decode_speed0(uint8_t val) {
	return val << 5;
}

/*
 * decode_speed7	Decode pan/tilt speed with deadzone between 1 and 6.
 *			Valid speeds are 0, and 7 - 64.
 */
static inline int decode_speed7(uint8_t val) {
	return val >= 7 ? ((val - 6) * SPEED_MAX / (64 - 6)) : 0;
}

/*
 * decode_speed		Decode pan or tilt speed.
 */
static inline int decode_speed(uint8_t val, enum rdr_flags_t flags) {
	int speed = (flags & PT_DEADZONE) ?
		decode_speed7(val) : decode_speed0(val);
	if(speed > SPEED_MAX)
		return SPEED_MAX;
	else
		return speed;
}

/*
 * decode_pan		Decode the pan speed (and command).
 */
static inline void decode_pan(struct ccpacket *pkt, uint8_t *mess,
	enum rdr_flags_t flags)
{
	int pan = decode_speed(mess[4], flags);
	if(bit_is_set(mess, BIT_PAN_RIGHT)) {
		pkt->command |= CC_PAN_RIGHT;
		ccpacket_set_pan_speed(pkt, pan);
	} else if(bit_is_set(mess, BIT_PAN_LEFT)) {
		pkt->command |= CC_PAN_LEFT;
		ccpacket_set_pan_speed(pkt, pan);
	} else {
		pkt->command |= CC_PAN_LEFT;
		ccpacket_set_pan_speed(pkt, 0);
	}
}

/*
 * decode_tilt		Decode the tilt speed (and command).
 */
static inline void decode_tilt(struct ccpacket *pkt, uint8_t *mess,
	enum rdr_flags_t flags)
{
	int tilt = decode_speed(mess[5], flags);
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
	if(bit_is_set(mess, BIT_CAMERA_ON_OFF)) {
		if(bit_is_set(mess, BIT_CAMERA_ON))
			pkt->command |= CC_CAMERA_ON;
		else
			pkt->command |= CC_CAMERA_OFF;
	}
	if(bit_is_set(mess, BIT_AUTO_PAN))
		pkt->command |= CC_AUTO_PAN;
}

/*
 * pelco_decode_command	Decode a pelco_p command.
 */
static inline enum decode_t pelco_decode_command(struct ccreader *rdr,
	uint8_t *mess)
{
	decode_receiver(&rdr->packet, mess);
	decode_pan(&rdr->packet, mess, rdr->flags);
	decode_tilt(&rdr->packet, mess, rdr->flags);
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
	decode_receiver(&rdr->packet, mess);
	int ex = mess[3] >> 1 & 0x1f;
	int p0 = mess[5];
	int p1 = mess[4];
	decode_extended(&rdr->packet, ex, p0, p1);
	ccreader_process_packet(rdr);
	return DECODE_MORE;
}

/*
 * pelco_log_discard	Log discarded data.
 */
static void pelco_log_discard(struct ccreader *rdr, uint8_t *mess, int n_bytes,
	const char *msg)
{
	char lbuf[256];
	int i;
	snprintf(lbuf, 256, "Pelco(P) %s; discarding %d bytes: ", msg, n_bytes);
	for(i = 0; i < n_bytes && i < 24 && strlen(lbuf) <= 250; i++) {
		char hchar[4];
		snprintf(hchar, 4, "%02X ", mess[i]);
		strncat(lbuf, hchar, 256 - strlen(lbuf));
	}
	if(n_bytes > 8)
		strncat(lbuf, "...", 256 - strlen(lbuf));
	log_println(rdr->log, lbuf);
}

/*
 * pelco_discard_garbage	Scan receive buffer for garbage data.
 */
static void pelco_discard_garbage(struct ccreader *rdr, struct buffer *rxbuf,
	const char *msg)
{
	uint8_t *mess = buffer_output(rxbuf);
	int n_bytes = 1;
	while(n_bytes < buffer_available(rxbuf)) {
		if(mess[n_bytes] == STX)
			break;
		n_bytes++;
	}
	buffer_consume(rxbuf, n_bytes);
	pelco_log_discard(rdr, mess, n_bytes, msg);
}

/*
 * checksum_is_valid	Test if a message checksum is valid.
 */
static inline bool checksum_is_valid(uint8_t *mess) {
	return calculate_checksum(mess) == mess[7];
}

/*
 * pelco_decode_message	Decode a pelco_p message.
 */
static inline enum decode_t pelco_decode_message(struct ccreader *rdr,
	struct buffer *rxbuf)
{
	uint8_t *mess = buffer_output(rxbuf);
	if(mess[0] != STX) {
		pelco_discard_garbage(rdr, rxbuf, "Invalid STX");
		return DECODE_MORE;
	}
	if(mess[6] != ETX) {
		pelco_discard_garbage(rdr, rxbuf, "Invalid ETX");
		return DECODE_MORE;
	}
	if(!checksum_is_valid(mess)) {
		pelco_discard_garbage(rdr, rxbuf, "Invalid checksum");
		return DECODE_MORE;
	}
	buffer_consume(rxbuf, SIZE_MSG);
	if(bit_is_set(mess, BIT_EXTENDED))
		return pelco_decode_extended(rdr, mess);
	else
		return pelco_decode_command(rdr, mess);
}

/*
 * pelco_p_do_read	Read messages in pelco_p protocol.
 */
void pelco_p_do_read(struct ccreader *rdr, struct buffer *rxbuf) {
	while(buffer_available(rxbuf) >= SIZE_MSG) {
		if(pelco_decode_message(rdr, rxbuf) == DECODE_DONE)
			break;
	}
}

/*
 * encode_receiver	Encode the receiver address.
 */
static inline void encode_receiver(uint8_t *mess, const struct ccpacket *pkt) {
	mess[0] = STX;
	mess[1] = ccpacket_get_receiver(pkt);
	mess[6] = ETX;
}

/*
 * pelco_p_encode_speed	Encode pan or tilt speed.
 */
static int pelco_p_encode_speed(int speed) {
	/* round to the nearest speed level */
	int s = (speed >> 5) + ((speed % 32) >> 4);
	if(s < TURBO_SPEED)
		return s;
	else
		return TURBO_SPEED - 1;
}

/** Encode pan speed.
 *
 * @param speed		Protocol independent speed (0 - SPEED_MAX).
 * @return Pan speed for Pelco-P protocol.
 */
static int pelco_p_encode_pan_speed(int speed) {
	if(speed > SPEED_MAX - 8)
		return TURBO_SPEED;
	else
		return pelco_p_encode_speed(speed);
}

/*
 * encode_pan		Encode the pan speed and command.
 */
static void encode_pan(uint8_t *mess, struct ccpacket *pkt) {
	int pan = pelco_p_encode_pan_speed(ccpacket_get_pan_speed(pkt));
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
	int tilt = pelco_p_encode_speed(pkt->tilt);
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
	if(pkt->command & CC_CAMERA_ON) {
		bit_set(mess, BIT_CAMERA_ON_OFF);
		bit_set(mess, BIT_CAMERA_ON);
	} else if(pkt->command & CC_CAMERA_OFF)
		bit_set(mess, BIT_CAMERA_ON_OFF);
	if(pkt->command & CC_AUTO_PAN)
		bit_set(mess, BIT_AUTO_PAN);
}

/*
 * encode_checksum	Encode the message checksum.
 */
static inline void encode_checksum(uint8_t *mess) {
	mess[7] = calculate_checksum(mess);
}

/*
 * encode_command	Encode a command message.
 */
static void encode_command(struct ccwriter *wtr, struct ccpacket *pkt) {
	uint8_t *mess = ccwriter_append(wtr, SIZE_MSG);
	if(mess) {
		encode_receiver(mess, pkt);
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
	uint8_t *mess = ccwriter_append(wtr, SIZE_MSG);
	if(mess) {
		encode_receiver(mess, pkt);
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
	uint8_t *mess = ccwriter_append(wtr, SIZE_MSG);
	if(mess) {
		encode_receiver(mess, pkt);
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
 * pelco_p_do_write	Write a packet in the pelco_p protocol.
 */
unsigned int pelco_p_do_write(struct ccwriter *wtr, struct ccpacket *pkt) {
	int receiver = ccpacket_get_receiver(pkt);
	if(receiver < 1 || receiver > PELCO_P_MAX_ADDRESS)
		return 0;
	adjust_menu_commands(pkt);
	if(ccpacket_has_command(pkt) || ccpacket_has_autopan(pkt) ||
	   ccpacket_has_power(pkt))
	{
		encode_command(wtr, pkt);
	}
	if(ccpacket_has_preset(pkt))
		encode_preset(wtr, pkt);
	if(ccpacket_has_aux(pkt))
		encode_aux(wtr, pkt);
	return 1;
}
