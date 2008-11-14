/*
 * protozoa -- CCTV transcoder / mixer for PTZ
 * Copyright (C) 2007-2008  Minnesota Department of Transportation
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

static inline bool decode_pan_tilt_zoom(struct ccpacket *pkt, uint8_t *mess) {
	uint8_t number = mess[7];
	short speed = decode_speed(mess);

	switch(number) {
		case JAXIS_PAN:
			pkt->command ^= pkt->command & CC_PAN;
			if(speed <= 0)
				pkt->command |= CC_PAN_LEFT;
			if(speed > 0)
				pkt->command |= CC_PAN_RIGHT;
			pkt->pan = remap_speed(speed);
			break;
		case JAXIS_TILT:
			pkt->command ^= pkt->command & CC_TILT;
			if(speed < 0)
				pkt->command |= CC_TILT_UP;
			if(speed >= 0)
				pkt->command |= CC_TILT_DOWN;
			pkt->tilt = remap_speed(speed);
			break;
		case JAXIS_ZOOM:
			if(speed < 0)
				pkt->zoom = ZOOM_OUT;
			else if(speed > 0)
				pkt->zoom = ZOOM_IN;
			else
				pkt->zoom = ZOOM_NONE;
			break;
	}
	pkt->command ^= pkt->command & CC_PRESET;
	return true;
}

static bool moved_since_pressed(struct ccpacket *pkt) {
	return (pkt->command & CC_RECALL) == 0;
}

static inline bool decode_button(struct ccreader *rdr, uint8_t *mess) {
	struct ccpacket *pkt = &rdr->packet;
	uint8_t number = mess[7];
	bool pressed = decode_pressed(mess);
	bool moved = moved_since_pressed(pkt);

	pkt->command ^= pkt->command & CC_PAN_TILT;

	switch(number) {
		case JBUTTON_FOCUS_NEAR:
			if(pressed)
				pkt->focus = FOCUS_NEAR;
			else
				pkt->focus = FOCUS_NONE;
			return true;
		case JBUTTON_FOCUS_FAR:
			if(pressed)
				pkt->focus = FOCUS_FAR;
			else
				pkt->focus = FOCUS_NONE;
			return true;
		case JBUTTON_IRIS_CLOSE:
			if(pressed)
				pkt->iris = IRIS_CLOSE;
			else
				pkt->iris = IRIS_NONE;
			return true;
		case JBUTTON_IRIS_OPEN:
			if(pressed)
				pkt->iris = IRIS_OPEN;
			else
				pkt->iris = IRIS_NONE;
			return true;
		case JBUTTON_AUX_1:
			if(pressed)
				pkt->aux = AUX_1;
			else
				pkt->aux = AUX_NONE;
			return true;
		case JBUTTON_AUX_2:
			if(pressed)
				pkt->aux = AUX_2;
			else
				pkt->aux = AUX_NONE;
			return true;
		case JBUTTON_PRESET_1:
			pkt->command ^= pkt->command & CC_PRESET;
			pkt->preset = 1;
			if(pressed)
				pkt->command |= CC_RECALL;
			else if(moved)
				pkt->command |= CC_STORE;
			else
				break;
			return true;
		case JBUTTON_PRESET_2:
			pkt->command ^= pkt->command & CC_PRESET;
			pkt->preset = 2;
			if(pressed)
				pkt->command |= CC_RECALL;
			else if(moved)
				pkt->command |= CC_STORE;
			else
				break;
			return true;
		case JBUTTON_PRESET_3:
			pkt->command ^= pkt->command & CC_PRESET;
			pkt->preset = 3;
			if(pressed)
				pkt->command |= CC_RECALL;
			else if(moved)
				pkt->command |= CC_STORE;
			else
				break;
			return true;
		case JBUTTON_PRESET_4:
			pkt->command ^= pkt->command & CC_PRESET;
			pkt->preset = 4;
			if(pressed)
				pkt->command |= CC_RECALL;
			else if(moved)
				pkt->command |= CC_STORE;
			else
				break;
			return true;
		case JBUTTON_PREVIOUS:
			if(pressed)
				ccreader_previous_camera(rdr);
			break;
		case JBUTTON_NEXT:
			if(pressed)
				ccreader_next_camera(rdr);
			break;
	}
	pkt->preset = 0;
	return false;
}

static inline bool joystick_decode_event(struct ccreader *rdr, uint8_t *mess) {
	uint8_t ev_type = mess[6];

	if(ev_type & JEVENT_AXIS)
		return decode_pan_tilt_zoom(&rdr->packet, mess);
	else if((ev_type & JEVENT_BUTTON) && !(ev_type & JEVENT_INITIAL))
		return decode_button(rdr, mess);
	else
		return false;
}

static inline bool joystick_read_message(struct ccreader *rdr,
	struct buffer *rxbuf)
{
	uint8_t *mess = buffer_output(rxbuf);
	bool m = joystick_decode_event(rdr, mess);
	buffer_consume(rxbuf, JEVENT_OCTETS);
	return m;
}

void joystick_do_read(struct ccreader *rdr, struct buffer *rxbuf) {
	int c = 0;
	while(buffer_available(rxbuf) >= JEVENT_OCTETS)
		c += joystick_read_message(rdr, rxbuf);
	if(c)
		ccreader_process_packet_no_clear(rdr);
	rdr->packet.preset = 0;
}
