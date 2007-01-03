#include <stdbool.h>
#include <stdint.h>	/* for uint8_t */
#include <strings.h>
#include "ccreader.h"
#include "vicon.h"
#include "bitarray.h"

#define FLAG (0x80)
#define SIZE_STATUS (2)
#define SIZE_COMMAND (6)
#define SIZE_EXTENDED (10)

enum vicon_bit_t {
	BIT_COMMAND = 12,
	BIT_ACK_ALARM = 13,
	BIT_EXTENDED = 14,
	BIT_AUTO_IRIS = 17,
	BIT_AUTO_PAN = 18,
	BIT_TILT_DOWN = 19,
	BIT_TILT_UP = 20,
	BIT_PAN_RIGHT = 21,
	BIT_PAN_LEFT = 22,
	BIT_LENS_SPEED = 24,
	BIT_IRIS_CLOSE = 25,
	BIT_IRIS_OPEN = 26,
	BIT_FOCUS_NEAR = 27,
	BIT_FOCUS_FAR = 28,
	BIT_ZOOM_IN = 29,
	BIT_ZOOM_OUT = 30,
	BIT_AUX_6 = 33,
	BIT_AUX_5 = 34,
	BIT_AUX_4 = 35,
	BIT_AUX_3 = 36,
	BIT_AUX_2 = 37,
	BIT_AUX_1 = 38,
	BIT_RECALL = 45,
	BIT_STORE = 46,
	BIT_STAT_V15UVS = 48,
	BIT_EX_STORE = 48,
	BIT_EX_STATUS = 49,
	BIT_EX_REQUEST = 52,
	BIT_STAT_SECTOR = 56,
	BIT_STAT_PRESET = 57,
	BIT_STAT_AUX_SET_2 = 58,
};

static inline int decode_receiver(uint8_t *mess) {
	return ((mess[0] & 0x0f) << 4) | (mess[1] & 0x0f);
}

static inline bool is_command(uint8_t *mess) {
	return bit_is_set(mess, BIT_COMMAND);
}

static inline bool is_extended_command(uint8_t *mess) {
	return bit_is_set(mess, BIT_COMMAND) && bit_is_set(mess, BIT_EXTENDED);
}

static inline void decode_pan(struct ccpacket *p, uint8_t *mess) {
	if(bit_is_set(mess, BIT_PAN_RIGHT)) {
		p->command |= CC_PAN_RIGHT;
		p->pan = SPEED_MAX;
	} else if(bit_is_set(mess, BIT_PAN_LEFT)) {
		p->command |= CC_PAN_LEFT;
		p->pan = SPEED_MAX;
	} else {
		p->command |= CC_PAN_LEFT;
		p->pan = 0;
	}
}

static inline void decode_tilt(struct ccpacket *p, uint8_t *mess) {
	if(bit_is_set(mess, BIT_TILT_UP)) {
		p->command |= CC_TILT_UP;
		p->tilt = SPEED_MAX;
	} else if(bit_is_set(mess, BIT_TILT_DOWN)) {
		p->command |= CC_TILT_DOWN;
		p->tilt = SPEED_MAX;
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

static inline void decode_toggles(struct ccpacket *p, uint8_t *mess) {
	if(bit_is_set(mess, BIT_ACK_ALARM))
		p->command |= CC_ACK_ALARM;
	if(bit_is_set(mess, BIT_AUTO_IRIS))
		p->command |= CC_AUTO_IRIS;
	if(bit_is_set(mess, BIT_AUTO_PAN))
		p->command |= CC_AUTO_PAN;
	if(bit_is_set(mess, BIT_LENS_SPEED))
		p->command |= CC_LENS_SPEED;
}

static inline void decode_aux(struct ccpacket *p, uint8_t *mess) {
	p->aux = 0;
	if(bit_is_set(mess, BIT_AUX_1))
		p->aux |= AUX_1;
	if(bit_is_set(mess, BIT_AUX_2))
		p->aux |= AUX_2;
	if(bit_is_set(mess, BIT_AUX_3))
		p->aux |= AUX_3;
	if(bit_is_set(mess, BIT_AUX_4))
		p->aux |= AUX_4;
	if(bit_is_set(mess, BIT_AUX_5))
		p->aux |= AUX_5;
	if(bit_is_set(mess, BIT_AUX_6))
		p->aux |= AUX_6;
}

static inline void decode_preset(struct ccpacket *p, uint8_t *mess) {
	if(bit_is_set(mess, BIT_RECALL))
		p->command = CC_RECALL;
	else if(bit_is_set(mess, BIT_STORE))
		p->command = CC_STORE;
	p->preset = mess[5] & 0x0f;
}

static inline void decode_ex_speed(struct ccpacket *p, uint8_t *mess) {
	p->pan = ((mess[6] & 0x0f) << 7) | (mess[7] & 0x7f);
	p->tilt = ((mess[8] & 0x0f) << 7) | (mess[9] & 0x7f);
}

static inline void decode_ex_status(struct ccpacket *p, uint8_t *mess) {
	p->status = STATUS_REQUEST;
	if(bit_is_set(mess, BIT_STAT_SECTOR))
		p->status |= STATUS_SECTOR;
	if(bit_is_set(mess, BIT_STAT_PRESET))
		p->status |= STATUS_PRESET;
	if(bit_is_set(mess, BIT_STAT_V15UVS) &&
	   bit_is_set(mess, BIT_STAT_AUX_SET_2))
		p->status |= STATUS_AUX_SET_2;
}

static inline void decode_ex_preset(struct ccpacket *p, uint8_t *mess) {
	if(bit_is_set(mess, BIT_EX_STORE))
		p->command = CC_STORE;
	else
		p->command = CC_RECALL;
	p->preset = mess[7] & 0x7f;
	p->pan = mess[8] & 0x7f;
	p->tilt = mess[9] & 0x7f;
}

static inline enum decode_t vicon_decode_extended(struct ccreader *r,
	uint8_t *mess, struct buffer *rxbuf)
{
	if(buffer_available(rxbuf) < SIZE_EXTENDED)
		return DONE;
	r->packet.receiver = decode_receiver(mess);
	decode_pan(&r->packet, mess);
	decode_tilt(&r->packet, mess);
	decode_lens(&r->packet, mess);
	decode_toggles(&r->packet, mess);
	decode_aux(&r->packet, mess);
	decode_preset(&r->packet, mess);
	if(bit_is_set(mess, BIT_EX_REQUEST)) {
		if(bit_is_set(mess, BIT_EX_STATUS))
			decode_ex_status(&r->packet, mess);
		else
			decode_ex_preset(&r->packet, mess);
	} else
		decode_ex_speed(&r->packet, mess);
	buffer_consume(rxbuf, SIZE_EXTENDED);
	ccreader_process_packet(r);
	return MORE;
}

static inline enum decode_t vicon_decode_command(struct ccreader *r,
	uint8_t *mess, struct buffer *rxbuf)
{
	if(buffer_available(rxbuf) < SIZE_COMMAND)
		return DONE;
	r->packet.receiver = decode_receiver(mess);
	decode_pan(&r->packet, mess);
	decode_tilt(&r->packet, mess);
	decode_lens(&r->packet, mess);
	decode_toggles(&r->packet, mess);
	decode_aux(&r->packet, mess);
	decode_preset(&r->packet, mess);
	buffer_consume(rxbuf, SIZE_COMMAND);
	ccreader_process_packet(r);
	return MORE;
}

static inline enum decode_t vicon_decode_status(struct ccreader *r,
	uint8_t *mess, struct buffer *rxbuf)
{
	if(buffer_available(rxbuf) < SIZE_STATUS)
		return DONE;
	r->packet.receiver = decode_receiver(mess);
	r->packet.status = STATUS_REQUEST;
	buffer_consume(rxbuf, SIZE_STATUS);
	ccreader_process_packet(r);
	return MORE;
}

static inline enum decode_t vicon_decode_message(struct ccreader *r,
	struct buffer *rxbuf)
{
	uint8_t *mess = buffer_output(rxbuf);
	if((mess[0] & FLAG) == 0) {
		log_println(r->log, "Vicon: unexpected byte %02X", mess[0]);
		buffer_consume(rxbuf, 1);
		return MORE;
	}
	if(is_extended_command(mess))
		return vicon_decode_extended(r, mess, rxbuf);
	else if(is_command(mess))
		return vicon_decode_command(r, mess, rxbuf);
	else
		return vicon_decode_status(r, mess, rxbuf);
}

void vicon_do_read(struct ccreader *r, struct buffer *rxbuf) {
	while(buffer_available(rxbuf) >= SIZE_STATUS) {
		if(vicon_decode_message(r, rxbuf) == DONE)
			break;
	}
}

static inline void encode_receiver(uint8_t *mess, int receiver) {
	mess[0] = FLAG | ((receiver >> 4) & 0x0f);
	mess[1] = receiver & 0x0f;
}

static void encode_pan_tilt(uint8_t *mess, struct ccpacket *p) {
	if(p->pan) {
		if(p->command & CC_PAN_LEFT)
			bit_set(mess, BIT_PAN_LEFT);
		else if(p->command & CC_PAN_RIGHT)
			bit_set(mess, BIT_PAN_RIGHT);
	}
	if(p->tilt) {
		if(p->command & CC_TILT_UP)
			bit_set(mess, BIT_TILT_UP);
		else if(p->command & CC_TILT_DOWN)
			bit_set(mess, BIT_TILT_DOWN);
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

static void encode_toggles(uint8_t *mess, struct ccpacket *p) {
	if(p->command == CC_ACK_ALARM)
		bit_set(mess, BIT_ACK_ALARM);
	if(p->command == CC_AUTO_IRIS)
		bit_set(mess, BIT_AUTO_IRIS);
	if(p->command == CC_AUTO_PAN)
		bit_set(mess, BIT_AUTO_PAN);
	if(p->command == CC_LENS_SPEED)
		bit_set(mess, BIT_LENS_SPEED);
}

static void encode_aux(uint8_t *mess, struct ccpacket *p) {
	if(p->aux & AUX_CLEAR)
		return;
	if(p->aux & AUX_1)
		bit_set(mess, BIT_AUX_1);
	if(p->aux & AUX_2)
		bit_set(mess, BIT_AUX_2);
	if(p->aux & AUX_3)
		bit_set(mess, BIT_AUX_3);
	if(p->aux & AUX_4)
		bit_set(mess, BIT_AUX_4);
	if(p->aux & AUX_5)
		bit_set(mess, BIT_AUX_5);
	if(p->aux & AUX_6)
		bit_set(mess, BIT_AUX_6);
}

static void encode_preset(uint8_t *mess, struct ccpacket *p) {
	if(p->command & CC_RECALL)
		bit_set(mess, BIT_RECALL);
	else if(p->command & CC_STORE)
		bit_set(mess, BIT_STORE);
	mess[5] |= p->preset & 0x0f;
}

static void encode_command(struct ccwriter *w, struct ccpacket *p) {
	uint8_t *mess = ccwriter_append(w, SIZE_COMMAND);
	if(mess) {
		bzero(mess, SIZE_COMMAND);
		encode_receiver(mess, p->receiver);
		bit_set(mess, BIT_COMMAND);
		encode_pan_tilt(mess, p);
		encode_lens(mess, p);
		encode_toggles(mess, p);
		encode_aux(mess, p);
		encode_preset(mess, p);
	}
}

static void encode_speeds(uint8_t *mess, struct ccpacket *p) {
	mess[6] = (p->pan >> 7) & 0x0f;
	mess[7] = p->pan & 0x7f;
	mess[8] = (p->tilt >> 7) & 0x0f;
	mess[9] = p->tilt & 0x7f;
}

static void encode_extended_speed(struct ccwriter *w, struct ccpacket *p) {
	uint8_t *mess = ccwriter_append(w, SIZE_EXTENDED);
	if(mess) {
		bzero(mess, SIZE_EXTENDED);
		encode_receiver(mess, p->receiver);
		bit_set(mess, BIT_COMMAND);
		bit_set(mess, BIT_EXTENDED);
		encode_pan_tilt(mess, p);
		encode_lens(mess, p);
		encode_toggles(mess, p);
		encode_aux(mess, p);
		encode_preset(mess, p);
		encode_speeds(mess, p);
	}
}

static void encode_extended_preset(struct ccwriter *w, struct ccpacket *p) {
	uint8_t *mess = ccwriter_append(w, SIZE_EXTENDED);
	if(mess) {
		bzero(mess, SIZE_EXTENDED);
		encode_receiver(mess, p->receiver);
		bit_set(mess, BIT_COMMAND);
		bit_set(mess, BIT_EXTENDED);
		bit_set(mess, BIT_EX_REQUEST);
		if(p->command & CC_STORE)
			bit_set(mess, BIT_EX_STORE);
		encode_lens(mess, p);
		encode_toggles(mess, p);
		encode_aux(mess, p);
		mess[7] |= p->preset & 0x7f;
		mess[8] |= p->pan & 0x7f;
		mess[9] |= p->tilt & 0x7f;
	}
}

static inline void encode_simple_status(struct ccwriter *w, struct ccpacket *p)
{
	uint8_t *mess = ccwriter_append(w, SIZE_STATUS);
	if(mess) {
		bzero(mess, SIZE_STATUS);
		encode_receiver(mess, p->receiver);
	}
}

static inline void encode_extended_status(struct ccwriter *w,
	struct ccpacket *p)
{
	uint8_t *mess = ccwriter_append(w, SIZE_EXTENDED);
	if(mess) {
		bzero(mess, SIZE_EXTENDED);
		encode_receiver(mess, p->receiver);
		bit_set(mess, BIT_COMMAND);
		bit_set(mess, BIT_EXTENDED);
		bit_set(mess, BIT_EX_STATUS);
		bit_set(mess, BIT_EX_REQUEST);
		if(p->status & STATUS_SECTOR)
			bit_set(mess, BIT_STAT_SECTOR);
		if(p->status & STATUS_PRESET)
			bit_set(mess, BIT_STAT_PRESET);
		if(p->status & STATUS_AUX_SET_2) {
			bit_set(mess, BIT_STAT_V15UVS);
			bit_set(mess, BIT_STAT_AUX_SET_2);
		}
	}
}

static void encode_status(struct ccwriter *w, struct ccpacket *p) {
	if(p->status & STATUS_EXTENDED)
		encode_extended_status(w, p);
	else
		encode_simple_status(w, p);
}

static inline bool is_extended_preset(struct ccpacket *p) {
	if(p->command & (CC_RECALL | CC_STORE))
		return (p->preset > 15) || p->pan || p->tilt;
	else
		return false;
}

unsigned int vicon_do_write(struct ccwriter *w, struct ccpacket *p) {
	if(p->receiver < 1 || p->receiver > 255)
		return 0;
	if(p->status)
		encode_status(w, p);
	else if(is_extended_preset(p))
		encode_extended_preset(w, p);
	else if(p->command & CC_PAN_TILT)
		encode_extended_speed(w, p);
	else
		encode_command(w, p);
	return 1;
}
