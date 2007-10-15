/*
 * protozoa -- CCTV transcoder / mixer for PTZ
 * Copyright (C) 2007  Minnesota Department of Transportation
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
#include <stdint.h>	/* for uint8_t */
#include "ccreader.h"
#include "joystick.h"

#define JEVENT_OCTETS	(8)

#define JEVENT_BUTTON	(0x01)
#define JEVENT_AXIS	(0x02)
#define JEVENT_INITIAL	(0x80)

#define JAXIS_PAN	(0)
#define JAXIS_TILT	(1)
#define JAXIS_ZOOM	(2)

#define JSPEED_MAX	(32767)

/*
 * Event records are 8 octets long:
 *
 *	0-3	timestamp
 *	4-5	value (-32767 to 32767)
 *	6	event type {0x01: button, 0x02: joystick, 0x80: initial value}
 *	7	number (button [0 - N] or axis {0: x, 1: y, 2: z})
 */

static inline int decode_speed(uint8_t *mess) {
	return *(short *)(mess + 4);
}

static inline int remap_int(int value, int irange, int orange) {
	int v = abs(value) * orange;
	int result = v / irange;
	if((v * 2) >= irange)
		return result + 1;
	else
		return result;
}

static inline int remap_speed(int value) {
	return remap_int(value, JSPEED_MAX, SPEED_MAX);
}

static inline void decode_pan_tilt_zoom(struct ccpacket *p, uint8_t *mess) {
	uint8_t number = mess[7];
	short speed = decode_speed(mess);

	switch(number) {
		case JAXIS_PAN:
			p->command ^= (p->command & CC_PAN);
			if(speed < 0)
				p->command |= CC_PAN_LEFT;
			if(speed > 0)
				p->command |= CC_PAN_RIGHT;
			p->pan = remap_speed(speed);
			break;
		case JAXIS_TILT:
			p->command ^= (p->command & CC_TILT);
			if(speed < 0)
				p->command |= CC_TILT_UP;
			if(speed > 0)
				p->command |= CC_TILT_DOWN;
			p->tilt = remap_speed(speed);
			break;
		case JAXIS_ZOOM:
			if(speed < 0)
				p->zoom = ZOOM_OUT;
			else if(speed > 0)
				p->zoom = ZOOM_IN;
			else
				p->zoom = ZOOM_NONE;
			break;
	}
}

static inline void joystick_decode_event(struct ccpacket *p, uint8_t *mess) {
	uint8_t ev_type = mess[6];

	if(ev_type & JEVENT_AXIS)
		decode_pan_tilt_zoom(p, mess);
//	if(ev_type & JEVENT_BUTTON)
//		decode_button(p, mess);

	p->receiver = 1;
}

static inline void joystick_read_message(struct ccreader *r,
	struct buffer *rxbuf)
{
	uint8_t *mess = buffer_output(rxbuf);
	joystick_decode_event(&r->packet, mess);
	buffer_consume(rxbuf, JEVENT_OCTETS);
}

void joystick_do_read(struct ccreader *r, struct buffer *rxbuf) {
	while(buffer_available(rxbuf) >= JEVENT_OCTETS)
		joystick_read_message(r, rxbuf);
	/* FIXME: consolidate events if the writers can't keep up */
	ccreader_process_packet_no_clear(r);
}
