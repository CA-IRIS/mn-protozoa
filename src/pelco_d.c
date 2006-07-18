#include <stdbool.h>
#include <stdio.h>
#include <strings.h>
#include "ccreader.h"
#include "pelco_d.h"
#include "bitarray.h"

#define FLAG (0xff)
#define MSG_SIZE (7)
#define TURBO_SPEED (1 << 6)

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

static inline int decode_receiver(uint8_t *mess) {
	return mess[1];
}

static uint8_t calculate_checksum(uint8_t *mess) {
	int i;
	int checksum = 0;
	for(i = 1; i < 6; i++)
		checksum += mess[i];
	return checksum;
}

static inline bool is_extended(struct buffer *rxbuf) {
	uint8_t *mess = rxbuf->pout;
	return bit_is_set(mess, BIT_EXTENDED);
}

static inline void decode_pan(struct ccpacket *p, uint8_t *mess) {
	int pan = mess[4] << 5;
	if(pan > SPEED_MAX)
		pan = SPEED_MAX;
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

static inline void decode_tilt(struct ccpacket *p, uint8_t *mess) {
	int tilt = mess[5] << 5;
	if(tilt > SPEED_MAX)
		tilt = SPEED_MAX;
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

static inline int pelco_decode_command(struct ccreader *r,
	struct buffer *rxbuf)
{
	uint8_t *mess = rxbuf->pout;
	r->packet.receiver = decode_receiver(mess);
	decode_pan(&r->packet, mess);
	decode_tilt(&r->packet, mess);
	decode_lens(&r->packet, mess);
	decode_sense(&r->packet, mess);
	buffer_skip(rxbuf, MSG_SIZE);
	return ccreader_process_packet(r);
}

enum extended_t {
	EX_NONE,		/* 00000 */
	EX_STORE,		/* 00001 store preset */
	EX_CLEAR,		/* 00010 clear preset */
	EX_RECALL,		/* 00011 recall preset */
	EX_AUX_SET,		/* 00100 set auxilliary */
	EX_AUX_CLEAR,		/* 00101 clear auxilliary */
	EX_RESERVED,		/* 00110 */
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

static inline int pelco_decode_extended(struct ccreader *r,
	struct buffer *rxbuf)
{
	uint8_t *mess = rxbuf->pout;
	r->packet.receiver = decode_receiver(mess);
	int ex = mess[3] >> 1 & 0x1f;
	int p0 = mess[5];
	int p1 = mess[4];
	decode_extended(&r->packet, ex, p0, p1);
	buffer_skip(rxbuf, MSG_SIZE);
	return ccreader_process_packet(r);
}

static inline bool checksum_invalid(struct buffer *rxbuf) {
	uint8_t *mess = rxbuf->pout;
	return calculate_checksum(mess) != mess[6];
}

static inline int pelco_decode_message(struct ccreader *r,
	struct buffer *rxbuf)
{
	if(buffer_peek(rxbuf) != FLAG) {
		fprintf(stderr, "Pelco(D): unexpected byte %02X\n",
			buffer_get(rxbuf));
		return 0;
	}
	if(checksum_invalid(rxbuf)) {
		fprintf(stderr, "Pelco(D): invalid checksum\n");
		return 0;
	}
	if(is_extended(rxbuf))
		return pelco_decode_extended(r, rxbuf);
	else
		return pelco_decode_command(r, rxbuf);
}

int pelco_d_do_read(struct handler *h, struct buffer *rxbuf) {
	struct ccreader *r = (struct ccreader *)h;

	while(buffer_available(rxbuf) >= MSG_SIZE) {
		int m = pelco_decode_message(r, rxbuf);
		if(m < 0)
			return m;
		else if(m > 0)
			break;
	}
	return 0;
}

static inline void encode_receiver(uint8_t *mess, int receiver) {
	mess[0] = FLAG;
	mess[1] = receiver;
}

static void encode_pan(uint8_t *mess, struct ccpacket *p) {
	int pan = p->pan >> 5;
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

static void encode_tilt(uint8_t *mess, struct ccpacket *p) {
	int tilt = p->tilt >> 5;
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

static inline void encode_checksum(uint8_t *mess) {
	mess[6] = calculate_checksum(mess);
}

static void encode_command(struct ccwriter *w, struct ccpacket *p) {
	uint8_t mess[MSG_SIZE];
	bzero(mess, MSG_SIZE);
	encode_receiver(mess, p->receiver);
	encode_pan(mess, p);
	encode_tilt(mess, p);
	encode_lens(mess, p);
	encode_sense(mess, p);
	encode_checksum(mess);
	ccwriter_write(w, mess, MSG_SIZE);
}

static void encode_preset(struct ccwriter *w, struct ccpacket *p) {
	uint8_t mess[MSG_SIZE];
	bzero(mess, MSG_SIZE);
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
	ccwriter_write(w, mess, MSG_SIZE);
}

static void encode_aux(struct ccwriter *w, struct ccpacket *p) {
	uint8_t mess[MSG_SIZE];
	bzero(mess, MSG_SIZE);
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
	ccwriter_write(w, mess, MSG_SIZE);
}

static inline bool has_sense(struct ccpacket *p) {
	if(p->command & (CC_AUTO_PAN | CC_MANUAL_PAN))
		return true;
	if(p->command & (CC_CAMERA_ON | CC_CAMERA_OFF))
		return true;
	return false;
}

static inline bool has_command(struct ccpacket *p) {
	if(p->command & CC_PAN_TILT)
		return true;
	if(p->zoom || p->focus || p->iris)
		return true;
	return has_sense(p);
}

static inline bool has_preset(struct ccpacket *p) {
	return p->command & CC_PRESET;
}

static inline bool has_aux(struct ccpacket *p) {
	if(p->aux)
		return true;
	else
		return false;
}

int pelco_d_do_write(struct ccwriter *w, struct ccpacket *p) {
	int receiver = p->receiver + w->base;
	if(receiver < 1 || receiver > 254) {
		ccpacket_drop(p);
		return 0;
	}
	if(has_command(p))
		encode_command(w, p);
	if(has_preset(p))
		encode_preset(w, p);
	if(has_aux(p))
		encode_aux(w, p);
	return 1;
}
