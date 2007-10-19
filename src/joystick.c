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

/*
 * Linux joystick input event driver.
 *
 * Event records are 8 octets long:
 *
 *	0-3	timestamp
 *	4-5	value (-32767 to 32767)
 *	6	event type {0x01: button, 0x02: joystick, 0x80: initial value}
 *	7	number (button [0 - N] or axis {0: x, 1: y, 2: z})
 */
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
#define JBUTTON_PREVIOUS	(10)
#define JBUTTON_NEXT		(11)

static inline int decode_speed(uint8_t *mess) {
	return *(short *)(mess + 4);
}

static inline bool decode_pressed(uint8_t *mess) {
	return *(short *)(mess + 4) != 0;
}

static inline int remap_int(int value, int irange, int orange) {
	int v = abs(value) * orange;
	return v / irange;
}

static inline int remap_speed(int value) {
	return remap_int(value, JSPEED_MAX, SPEED_MAX);
}

static inline bool decode_pan_tilt_zoom(struct ccpacket *p, uint8_t *mess) {
	uint8_t number = mess[7];
	short speed = decode_speed(mess);

	switch(number) {
		case JAXIS_PAN:
			p->command ^= p->command & CC_PAN;
			if(speed <= 0)
				p->command |= CC_PAN_LEFT;
			if(speed > 0)
				p->command |= CC_PAN_RIGHT;
			p->pan = remap_speed(speed);
			break;
		case JAXIS_TILT:
			p->command ^= p->command & CC_TILT;
			if(speed < 0)
				p->command |= CC_TILT_UP;
			if(speed >= 0)
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
	return true;
}

static bool moved_since_pressed(struct ccpacket *p) {
	return (p->command & CC_RECALL) == 0;
}

static inline bool decode_button(struct ccreader *r, uint8_t *mess) {
	struct ccpacket *p = &r->packet;
	uint8_t number = mess[7];
	bool pressed = decode_pressed(mess);
	bool moved = moved_since_pressed(p);

	p->command ^= p->command & CC_PAN_TILT;

	switch(number) {
		case JBUTTON_FOCUS_NEAR:
			if(pressed)
				p->focus = FOCUS_NEAR;
			else
				p->focus = FOCUS_NONE;
			return true;
		case JBUTTON_FOCUS_FAR:
			if(pressed)
				p->focus = FOCUS_FAR;
			else
				p->focus = FOCUS_NONE;
			return true;
		case JBUTTON_IRIS_CLOSE:
			if(pressed)
				p->iris = IRIS_CLOSE;
			else
				p->iris = IRIS_NONE;
			return true;
		case JBUTTON_IRIS_OPEN:
			if(pressed)
				p->iris = IRIS_OPEN;
			else
				p->iris = IRIS_NONE;
			return true;
		case JBUTTON_AUX_1:
			if(pressed)
				p->aux = AUX_1;
			else
				p->aux = AUX_NONE;
			return true;
		case JBUTTON_AUX_2:
			if(pressed)
				p->aux = AUX_2;
			else
				p->aux = AUX_NONE;
			return true;
		case JBUTTON_PRESET_1:
			p->command ^= p->command & CC_PRESET;
			p->preset = 1;
			if(pressed)
				p->command |= CC_RECALL;
			else if(moved)
				p->command |= CC_STORE;
			else
				break;
			return true;
		case JBUTTON_PRESET_2:
			p->command ^= p->command & CC_PRESET;
			p->preset = 2;
			if(pressed)
				p->command |= CC_RECALL;
			else if(moved)
				p->command |= CC_STORE;
			else
				break;
			return true;
		case JBUTTON_PRESET_3:
			p->command ^= p->command & CC_PRESET;
			p->preset = 3;
			if(pressed)
				p->command |= CC_RECALL;
			else if(moved)
				p->command |= CC_STORE;
			else
				break;
			return true;
		case JBUTTON_PRESET_4:
			p->command ^= p->command & CC_PRESET;
			p->preset = 4;
			if(pressed)
				p->command |= CC_RECALL;
			else if(moved)
				p->command |= CC_STORE;
			else
				break;
			return true;
		case JBUTTON_PREVIOUS:
			if(pressed)
				ccreader_previous_camera(r);
			break;
		case JBUTTON_NEXT:
			if(pressed)
				ccreader_next_camera(r);
			break;
	}
	p->preset = 0;
	return false;
}

static inline bool joystick_decode_event(struct ccreader *r, uint8_t *mess) {
	uint8_t ev_type = mess[6];

	if(ev_type & JEVENT_AXIS)
		return decode_pan_tilt_zoom(&r->packet, mess);
	else if((ev_type & JEVENT_BUTTON) && !(ev_type & JEVENT_INITIAL))
		return decode_button(r, mess);
	else
		return false;
}

static inline bool joystick_read_message(struct ccreader *r,
	struct buffer *rxbuf)
{
	uint8_t *mess = buffer_output(rxbuf);
	bool m = joystick_decode_event(r, mess);
	buffer_consume(rxbuf, JEVENT_OCTETS);
	return m;
}

void joystick_do_read(struct ccreader *r, struct buffer *rxbuf) {
	int c = 0;
	while(buffer_available(rxbuf) >= JEVENT_OCTETS)
		c += joystick_read_message(r, rxbuf);
	/* FIXME: consolidate events if the writers can't keep up */
	if(c)
		ccreader_process_packet_no_clear(r);
	r->packet.preset = 0;
}
