#include <stdbool.h>
#include <stdio.h>
#include "sport.h"
#include "ccpacket.h"

#define FLAG 0x80

static inline bool pt_command(uint8_t *mess) {
	return (mess[2] & 0x02) != 0;
}

static inline int parse_receiver(uint8_t *mess) {
	return ((mess[0] & 0x03) << 6 | (mess[1] & 0x01) << 5 |
		(mess[2] >> 2) & 0x1f) + 1;
}

static inline int pt_bits(uint8_t *mess) {
	return (mess[1] >> 4) & 0x03;
}

static inline int pt_extra(uint8_t *mess) {
	return (mess[1] >> 1) & 0x07;
}

/* Valid pan/tilt speeds are 0 - 6, here's a lookup table */
static const int SPEED[] = { 0, 170, 341, 512, 682, 852, 1023 };

static inline int pt_speed(uint8_t *mess) {
	return SPEED[pt_extra(mess)];
}

enum pt_command_t { TILT_UP, TILT_DOWN, PAN_LEFT, PAN_RIGHT };

static inline void parse_pan_tilt(struct ccpacket *p, enum pt_command_t cmnd,
	int speed)
{
	switch(cmnd) {
		case PAN_LEFT:
			p->pan = -speed;
			break;
		case PAN_RIGHT:
			p->pan = speed;
			break;
		case TILT_UP:
			p->tilt = -speed;
			break;
		case TILT_DOWN:
			p->tilt = speed;
			break;
	}
}

static inline void parse_lens(struct ccpacket *p, int extra) {
	switch(extra) {
		case 0x03:
			p->zoom = ZOOM_IN;
			break;
		case 0x06:
			p->zoom = ZOOM_OUT;
			break;
		case 0x02:
			p->focus = FOCUS_FAR;
			break;
		case 0x05:
			p->focus = FOCUS_NEAR;
			break;
		case 0x01:
			p->iris = IRIS_OPEN;
			break;
		case 0x04:
			p->iris = IRIS_CLOSE;
			break;
	}
}

static const enum aux_t AUX[] = {
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
	p->aux = AUX[extra];
}

enum ex_function_t { EX_LENS, EX_AUX, EX_RCL_PRESET, EX_STO_PRESET };

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

static void manchester_parse_packet(uint8_t *mess) {
	struct ccpacket p;

	ccpacket_init(&p);

	p.receiver = parse_receiver(mess);
	parse_packet(&p, mess);

	printf("rcv: %d pan: %d tilt: %d zoom: %d focus: %d iris: %d aux: %d\n",
		p.receiver, p.pan, p.tilt, p.zoom, p.focus, p.iris, p.aux);
}

static int manchester_read_message(struct buffer *rxbuf) {
	while(buffer_peek(rxbuf) & FLAG == 0)
		printf("Manchester: unexpected byte %02X\n", buffer_get(rxbuf));
	if(buffer_available(rxbuf) < 3) {
		printf("Manchester: incomplete message\n");
		return -1;
	}
	manchester_parse_packet(rxbuf->pout);
	buffer_skip(rxbuf, 3);
	return 0;
}

int manchester_do_read(struct handler *h, struct buffer *rxbuf) {
	while(!buffer_is_empty(rxbuf)) {
		if(manchester_read_message(rxbuf) < 0)
			return -1;
	}
	return 0;
}
