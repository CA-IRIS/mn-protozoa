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

#define JBUTTON_FOCUS_NEAR	(0)
#define JBUTTON_FOCUS_FAR	(1)
#define JBUTTON_IRIS_CLOSE	(2)
#define JBUTTON_IRIS_OPEN	(3)
#define JBUTTON_AUX_1		(4)
#define JBUTTON_AUX_2		(5)
#define JBUTTON_PRESET_1	(6)
#define JBUTTON_PRESET_2	(7)
#define JBUTTON_PRESET_3	(8)
#define JBUTTON_PRESET_4	(9)

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

static inline bool decode_pressed(uint8_t *mess) {
	return *(short *)(mess + 4) != 0;
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
			p->command ^= p->command & CC_PAN;
			if(speed < 0)
				p->command |= CC_PAN_LEFT;
			if(speed > 0)
				p->command |= CC_PAN_RIGHT;
			p->pan = remap_speed(speed);
			break;
		case JAXIS_TILT:
			p->command ^= p->command & CC_TILT;
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
	p->command ^= p->command & CC_PRESET;
}

static bool moved_since_pressed(struct ccpacket *p) {
	return (p->command & CC_RECALL) == 0;
}

static inline void decode_button(struct ccpacket *p, uint8_t *mess) {
	uint8_t number = mess[7];
	bool pressed = decode_pressed(mess);
	bool moved = moved_since_pressed(p);

	switch(number) {
		case JBUTTON_FOCUS_NEAR:
			if(pressed)
				p->focus = FOCUS_NEAR;
			else
				p->focus = FOCUS_NONE;
			break;
		case JBUTTON_FOCUS_FAR:
			if(pressed)
				p->focus = FOCUS_FAR;
			else
				p->focus = FOCUS_NONE;
			break;
		case JBUTTON_IRIS_CLOSE:
			if(pressed)
				p->iris = IRIS_CLOSE;
			else
				p->iris = IRIS_NONE;
			break;
		case JBUTTON_IRIS_OPEN:
			if(pressed)
				p->iris = IRIS_OPEN;
			else
				p->iris = IRIS_NONE;
			break;
		case JBUTTON_AUX_1:
			if(pressed)
				p->aux = AUX_1;
			else
				p->aux = AUX_NONE;
			break;
		case JBUTTON_AUX_2:
			if(pressed)
				p->aux = AUX_2;
			else
				p->aux = AUX_NONE;
			break;
		case JBUTTON_PRESET_1:
			p->command ^= p->command & CC_PRESET;
			p->preset = 1;
			if(pressed)
				p->command |= CC_RECALL;
			else if(moved)
				p->command |= CC_STORE;
			else
				p->preset = 0;
			break;
		case JBUTTON_PRESET_2:
			p->command ^= p->command & CC_PRESET;
			p->preset = 2;
			if(pressed)
				p->command |= CC_RECALL;
			else if(moved)
				p->command |= CC_STORE;
			else
				p->preset = 0;
			break;
		case JBUTTON_PRESET_3:
			p->command ^= p->command & CC_PRESET;
			p->preset = 3;
			if(pressed)
				p->command |= CC_RECALL;
			else if(moved)
				p->command |= CC_STORE;
			else
				p->preset = 0;
			break;
		case JBUTTON_PRESET_4:
			p->command ^= p->command & CC_PRESET;
			p->preset = 4;
			if(pressed)
				p->command |= CC_RECALL;
			else if(moved)
				p->command |= CC_STORE;
			else
				p->preset = 0;
			break;
	}
}

static inline void joystick_decode_event(struct ccpacket *p, uint8_t *mess) {
	uint8_t ev_type = mess[6];

	if(ev_type & JEVENT_AXIS)
		decode_pan_tilt_zoom(p, mess);
	else if(ev_type & JEVENT_BUTTON)
		decode_button(p, mess);

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
	r->packet.preset = 0;
}
