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

static inline int parse_receiver(uint8_t *mess) {
	return 1 + (((mess[0] & 0x03) << 6) | ((mess[1] & 0x01) << 5) |
		((mess[2] >> 2) & 0x1f));
}

static inline int pt_bits(uint8_t *mess) {
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

static inline void parse_pan_tilt(struct ccpacket *p, enum pt_command_t cmnd,
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

static inline void parse_lens(struct ccpacket *p, enum lens_t extra) {
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

static inline void parse_aux(struct ccpacket *p, int extra) {
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

static inline void parse_extended(struct ccpacket *p, enum ex_function_t cmnd,
	int extra)
{
	switch(cmnd) {
		case EX_LENS:
			parse_lens(p, extra);
			break;
		case EX_AUX:
			parse_aux(p, extra);
			break;
		case EX_RCL_PRESET:
			/* FIXME */
			break;
		case EX_STO_PRESET:
			/* FIXME */
			break;
	}
}

static inline void parse_packet(struct ccpacket *p, uint8_t *mess) {
	int cmnd = pt_bits(mess);
	if(pt_command(mess))
		parse_pan_tilt(p, cmnd, pt_speed(mess));
	else
		parse_extended(p, cmnd, pt_extra(mess));
}

static inline void manchester_parse_packet(struct combiner *c, uint8_t *mess) {
	int receiver = parse_receiver(mess);
	if(c->packet.receiver != receiver)
		c->do_write(c);
	c->packet.receiver = receiver;
	parse_packet(&c->packet, mess);

printf(" in: %02x %02x %02x\n", mess[0], mess[1], mess[2]);
}

static inline int manchester_read_message(struct combiner *c,
	struct buffer *rxbuf)
{
	if((buffer_peek(rxbuf) & FLAG) == 0) {
		printf("Manchester: unexpected byte %02X\n", buffer_get(rxbuf));
		return 0;
	}
	manchester_parse_packet(c, rxbuf->pout);
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

static inline void format_pan(uint8_t *mess, struct ccpacket *p) {
	int speed = (abs(p->pan) / 170) & 0x07;
	mess[1] |= speed << 1;
	if(p->command & CC_PAN_LEFT)
		mess[1] |= PAN_LEFT << 4;
	else if(p->command & CC_PAN_RIGHT)
		mess[1] |= PAN_RIGHT << 4;
	mess[2] |= PT_COMMAND;
}

static void manchester_send_pan(struct combiner *c) {
	uint8_t mess[3];
	format_receiver(mess, c->packet.receiver);
	format_pan(mess, &c->packet);
	combiner_write(c, mess, 3);
}

static inline void format_tilt(uint8_t *mess, struct ccpacket *p) {
	int speed = (abs(p->tilt) / 170) & 0x07;
	mess[1] |= speed << 1;
	if(p->command & CC_TILT_DOWN)
		mess[1] |= TILT_DOWN << 4;
	else if(p->command & CC_TILT_UP)
		mess[1] |= TILT_UP << 4;
	mess[2] |= PT_COMMAND;
}

static void manchester_send_tilt(struct combiner *c) {
	uint8_t mess[3];
	format_receiver(mess, c->packet.receiver);
	format_tilt(mess, &c->packet);
	combiner_write(c, mess, 3);
}

static inline void format_zoom(uint8_t *mess, struct ccpacket *p) {
	if(p->zoom < 0)
		mess[1] |= XL_ZOOM_OUT << 1;
	else if(p->zoom > 0)
		mess[1] |= XL_ZOOM_IN << 1;
}

static void manchester_send_zoom(struct combiner *c) {
	uint8_t mess[3];
	format_receiver(mess, c->packet.receiver);
	format_zoom(mess, &c->packet);
	combiner_write(c, mess, 3);
}

int manchester_do_write(struct combiner *c) {
	if(!c->packet.receiver)
		return 0;
	ccpacket_debug(&c->packet);
	if(c->packet.command & (CC_PAN_LEFT | CC_PAN_RIGHT))
		manchester_send_pan(c);
	if(c->packet.command & (CC_TILT_UP | CC_TILT_DOWN))
		manchester_send_tilt(c);
	if(c->packet.zoom)
		manchester_send_zoom(c);
	ccpacket_init(&c->packet);
	return 0;
}
