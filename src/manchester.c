#include <stdbool.h>
#include <stdio.h>
#include "sport.h"
#include "ccpacket.h"
#include "combiner.h"

#define FLAG 0x80
#define PT_COMMAND 0x02

static inline bool pt_command(uint8_t *mess) {
	return (mess[2] & PT_COMMAND) != 0;
}

static inline int decode_receiver(uint8_t *mess) {
	return 1 + (((mess[0] & 0x03) << 6) | ((mess[1] & 0x01) << 5) |
		((mess[2] >> 2) & 0x1f));
}

static inline int decode_command(uint8_t *mess) {
	return (mess[1] >> 4) & 0x03;
}

static inline int pt_extra(uint8_t *mess) {
	return (mess[1] >> 1) & 0x07;
}

/* Valid pan/tilt speeds are 0 - 6, here's a lookup table */
static const int SPEED[] = { 0, 170, 341, 512, 682, 852, 1023, 1023 };

static inline int pt_speed(uint8_t *mess) {
	return SPEED[pt_extra(mess)];
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
			p->pan = -speed;
			break;
		case PAN_RIGHT:
			p->command |= CC_PAN_RIGHT;
			p->pan = speed;
			break;
		case TILT_DOWN:
			p->command |= CC_TILT_DOWN;
			p->tilt = -speed;
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
			/* Weird special case for hard down */
			p->command |= CC_TILT_DOWN;
			p->tilt = -1023;
			break;
		case XL_PAN_LEFT:
			/* Weird special case for hard left */
			p->command |= CC_PAN_LEFT;
			p->pan = -1023;
			break;
	}
}

static const enum aux_t AUX_LUT[] = {
	AUX_NONE,	/* 000 */
	AUX_NONE,	/* 001 */
	AUX_1,		/* 010 */
	AUX_4,		/* 011 */
	AUX_2,		/* 100 */
	AUX_5,		/* 101 */
	AUX_3,		/* 110 */
	AUX_6		/* 111 */
};
/* Reverse AUX function lookup table */
static const int LUT_AUX[] = {
	0, 2, 4, 6, 3, 5, 7, 0
};

static inline void decode_aux(struct ccpacket *p, int extra) {
	p->aux = AUX_LUT[extra];
	/* Weird special case for hard up */
	if(extra == 0) {
		p->command |= CC_TILT_UP;
		p->tilt = 1023;
	}
}

enum ex_function_t {
	EX_LENS,	/* 00 */
	EX_AUX,		/* 01 */
	EX_RCL_PRESET,	/* 10 */
	EX_STO_PRESET,	/* 11 */
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
		case EX_RCL_PRESET:
			/* FIXME */
			break;
		case EX_STO_PRESET:
			/* FIXME */
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

static inline void manchester_decode_packet(struct combiner *c, uint8_t *mess) {
	int receiver = decode_receiver(mess);
	if(c->packet.receiver != receiver)
		c->do_write(c);
	c->packet.receiver = receiver;
	decode_packet(&c->packet, mess);

printf(" in: %02x %02x %02x\n", mess[0], mess[1], mess[2]);
}

static inline int manchester_read_message(struct combiner *c,
	struct buffer *rxbuf)
{
	if((buffer_peek(rxbuf) & FLAG) == 0) {
		printf("Manchester: unexpected byte %02X\n", buffer_get(rxbuf));
		return 0;
	}
	manchester_decode_packet(c, rxbuf->pout);
	buffer_skip(rxbuf, 3);
	return 0;
}

int manchester_do_read(struct handler *h, struct buffer *rxbuf) {
	struct combiner *c = (struct combiner *)h;

	while(buffer_available(rxbuf) >= 3) {
		if(manchester_read_message(c, rxbuf) < 0)
			return -1;
	}
	return c->do_write(c);
}

static void combiner_write(struct combiner *c, uint8_t *mess, size_t count) {
	printf("out: %02x %02x %02x\n", mess[0], mess[1], mess[2]);
}

static inline void format_receiver(uint8_t *mess, int receiver) {
	int r = receiver - 1;
	mess[0] = FLAG | ((r >> 6) & 0x03);
	mess[1] = (r >> 5) & 0x01;
	mess[2] = (r & 0x1f) << 2;
}

static void encode_pan_tilt_command(struct combiner *c, enum pt_command_t cmnd,
	int speed)
{
	int s = (abs(speed) / 170) & 0x07;
	uint8_t mess[3];
	format_receiver(mess, c->packet.receiver);
	mess[1] |= (cmnd << 4) | (s << 1);
	mess[2] |= PT_COMMAND;
	combiner_write(c, mess, 3);
}

static inline void encode_pan(struct combiner *c) {
	if(c->packet.command & CC_PAN_LEFT)
		encode_pan_tilt_command(c, PAN_LEFT, c->packet.pan);
	else if(c->packet.command & CC_PAN_RIGHT)
		encode_pan_tilt_command(c, PAN_RIGHT, c->packet.pan);
}

static inline void encode_tilt(struct combiner *c) {
	if(c->packet.command & CC_TILT_DOWN)
		encode_pan_tilt_command(c, TILT_DOWN, c->packet.tilt);
	else if(c->packet.command & CC_TILT_UP)
		encode_pan_tilt_command(c, TILT_UP, c->packet.tilt);
}

static void encode_lens_function(struct combiner *c, enum lens_t func) {
	uint8_t mess[3];
	format_receiver(mess, c->packet.receiver);
	mess[1] |= func << 1;
	combiner_write(c, mess, 3);
}

static inline void encode_zoom(struct combiner *c) {
	if(c->packet.zoom < 0)
		encode_lens_function(c, XL_ZOOM_OUT);
	else if(c->packet.zoom > 0)
		encode_lens_function(c, XL_ZOOM_IN);
}

static inline void encode_focus(struct combiner *c) {
	if(c->packet.focus < 0)
		encode_lens_function(c, XL_FOCUS_NEAR);
	else if(c->packet.focus > 0)
		encode_lens_function(c, XL_FOCUS_FAR);
}

static inline void encode_iris(struct combiner *c) {
	if(c->packet.iris < 0)
		encode_lens_function(c, XL_IRIS_CLOSE);
	else if(c->packet.iris > 0)
		encode_lens_function(c, XL_IRIS_OPEN);
}

static inline void encode_aux(struct combiner *c) {
	uint8_t mess[3];
	if(c->packet.aux > 0) {
		format_receiver(mess, c->packet.receiver);
		mess[1] |= (LUT_AUX[c->packet.aux]) << 1;
		mess[1] |= (EX_AUX << 4);
		combiner_write(c, mess, 3);
	}
}

int manchester_do_write(struct combiner *c) {
	if(!c->packet.receiver)
		return 0;
//	ccpacket_debug(&c->packet);
	encode_pan(c);
	encode_tilt(c);
	encode_zoom(c);
	encode_focus(c);
	encode_iris(c);
	encode_aux(c);
	ccpacket_init(&c->packet);
	return 0;
}
