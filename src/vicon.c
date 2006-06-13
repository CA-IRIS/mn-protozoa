#include <stdbool.h>
#include <stdio.h>
#include <strings.h>
#include "sport.h"
#include "ccpacket.h"
#include "combiner.h"

#define FLAG 0x80

#define BIT_COMMAND	(12)
#define BIT_ACK_ALARM	(13)
#define BIT_EXTENDED	(14)
#define BIT_AUTO_IRIS	(17)
#define BIT_AUTO_PAN	(18)
#define BIT_TILT_DOWN	(19)
#define BIT_TILT_UP	(20)
#define BIT_PAN_RIGHT	(21)
#define BIT_PAN_LEFT	(22)
#define BIT_LENS_SPEED	(24)
#define BIT_IRIS_CLOSE	(25)
#define BIT_IRIS_OPEN	(26)
#define BIT_FOCUS_NEAR	(27)
#define BIT_FOCUS_FAR	(28)
#define BIT_ZOOM_IN	(29)
#define BIT_ZOOM_OUT	(30)
#define BIT_AUX_6	(33)
#define BIT_AUX_5	(34)
#define BIT_AUX_4	(35)
#define BIT_AUX_3	(36)
#define BIT_AUX_2	(37)
#define BIT_AUX_1	(38)
#define BIT_RECALL	(45)
#define BIT_STORE	(46)
#define BIT_EX_PRESET	(52)

static inline int decode_receiver(uint8_t *mess) {
	return ((mess[0] & 0x0f) << 4) | (mess[1] & 0x0f);
}

static inline bool bit_is_set(uint8_t *mess, int bit) {
	int by = bit / 8;
	int mask = 1 << (bit % 8);
	return (mess[by] & mask) != 0;
}

static inline void bit_set(uint8_t *mess, int bit) {
	int by = bit / 8;
	int mask = 1 << (bit % 8);
	mess[by] |= mask;
}

static inline bool is_command(struct buffer *rxbuf) {
	uint8_t *mess = rxbuf->pout;
	return bit_is_set(mess, BIT_COMMAND);
}

static inline bool is_extended_command(struct buffer *rxbuf) {
	uint8_t *mess = rxbuf->pout;
	return bit_is_set(mess, BIT_COMMAND) && bit_is_set(mess, BIT_EXTENDED);
}

static inline void decode_pan(struct ccpacket *p, uint8_t *mess) {
	if(bit_is_set(mess, BIT_PAN_RIGHT)) {
		p->command |= CC_PAN_RIGHT;
		p->pan = SPEED_MAX;
	} else if(bit_is_set(mess, BIT_PAN_LEFT)) {
		p->command |= CC_PAN_LEFT;
		p->pan = SPEED_MAX;
	}
}

static inline void decode_tilt(struct ccpacket *p, uint8_t *mess) {
	if(bit_is_set(mess, BIT_TILT_UP)) {
		p->command |= CC_TILT_UP;
		p->tilt = SPEED_MAX;
	} else if(bit_is_set(mess, BIT_TILT_DOWN)) {
		p->command |= CC_TILT_DOWN;
		p->tilt = SPEED_MAX;
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
	p->pan = ((mess[6] & 0x0f) << 7) | (mess[7] & 0x3f);
	p->tilt = ((mess[8] & 0x0f) << 7) | (mess[9] & 0x3f);
}

static inline void decode_ex_preset(struct ccpacket *p, uint8_t *mess) {
	printf("vicon: FIXME extended preset");
}

static inline int vicon_decode_extended(struct combiner *c,
	struct buffer *rxbuf)
{
	uint8_t *mess = rxbuf->pout;
	if(buffer_available(rxbuf) < 10)
		return 1;
	c->packet.receiver = decode_receiver(mess);
	decode_pan(&c->packet, mess);
	decode_tilt(&c->packet, mess);
	decode_lens(&c->packet, mess);
	decode_toggles(&c->packet, mess);
	decode_aux(&c->packet, mess);
	decode_preset(&c->packet, mess);
	if(bit_is_set(mess, BIT_EX_PRESET))
		decode_ex_preset(&c->packet, mess);
	else
		decode_ex_speed(&c->packet, mess);
printf(" in: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", mess[0], mess[1], mess[2], mess[3], mess[4], mess[5], mess[6], mess[7], mess[8], mess[9]);
	buffer_skip(rxbuf, 10);
	return 0;
}

static inline int vicon_decode_command(struct combiner *c,
	struct buffer *rxbuf)
{
	uint8_t *mess = rxbuf->pout;
	if(buffer_available(rxbuf) < 6)
		return 1;
	c->packet.receiver = decode_receiver(mess);
	decode_pan(&c->packet, mess);
	decode_tilt(&c->packet, mess);
	decode_lens(&c->packet, mess);
	decode_toggles(&c->packet, mess);
	decode_aux(&c->packet, mess);
	decode_preset(&c->packet, mess);
printf(" in: %02x %02x %02x %02x %02x %02x\n", mess[0], mess[1], mess[2], mess[3], mess[4], mess[5]);
	buffer_skip(rxbuf, 6);
	return c->do_write(c);
}

static inline int vicon_decode_status(struct combiner *c,
	struct buffer *rxbuf)
{
	uint8_t *mess = rxbuf->pout;
	if(buffer_available(rxbuf) < 2)
		return 1;
	c->packet.receiver = decode_receiver(mess);
printf(" in: %02x %02x\n", mess[0], mess[1]);
	buffer_skip(rxbuf, 2);
	return c->do_write(c);
}

static inline int vicon_decode_message(struct combiner *c,
	struct buffer *rxbuf)
{
	if((buffer_peek(rxbuf) & FLAG) == 0) {
		printf("Vicon: unexpected byte %02X\n", buffer_get(rxbuf));
		return 0;
	}
	ccpacket_init(&c->packet);
	if(is_extended_command(rxbuf))
		return vicon_decode_extended(c, rxbuf);
	else if(is_command(rxbuf))
		return vicon_decode_command(c, rxbuf);
	else
		return vicon_decode_status(c, rxbuf);
}

int vicon_do_read(struct handler *h, struct buffer *rxbuf) {
	struct combiner *c = (struct combiner *)h;

	while(buffer_available(rxbuf) >= 2) {
		int m = vicon_decode_message(c, rxbuf);
		if(m < 0)
			return m;
		else if(m > 0)
			break;
	}
	return 0;
}

static inline void encode_receiver(uint8_t *mess, int receiver) {
	mess[0] = FLAG | ((receiver >> 4) & 0x0f);
	mess[1] = receiver & 0x0f;
}

static void encode_pan_tilt(uint8_t *mess, struct ccpacket *p) {
	if(p->command & CC_PAN_LEFT)
		bit_set(mess, BIT_PAN_LEFT);
	else if(p->command & CC_PAN_RIGHT)
		bit_set(mess, BIT_PAN_RIGHT);
	if(p->command & CC_TILT_UP)
		bit_set(mess, BIT_TILT_UP);
	else if(p->command & CC_TILT_DOWN)
		bit_set(mess, BIT_TILT_DOWN);
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

static void encode_speeds(uint8_t *mess, struct ccpacket *p) {
	mess[6] = (p->pan >> 7) & 0x0f;
	mess[7] = p->pan & 0x3f;
	mess[8] = (p->tilt >> 7) & 0x0f;
	mess[9] = p->tilt & 0x3f;
}

static void encode_extended_speed(struct combiner *c) {
	uint8_t mess[10];
	bzero(mess, 10);
	encode_receiver(mess, c->packet.receiver);
	bit_set(mess, BIT_COMMAND);
	bit_set(mess, BIT_EXTENDED);
	encode_pan_tilt(mess, &c->packet);
	encode_lens(mess, &c->packet);
	encode_toggles(mess, &c->packet);
	encode_aux(mess, &c->packet);
	encode_preset(mess, &c->packet);
	encode_speeds(mess, &c->packet);
	combiner_write(c, mess, 10);
}

int vicon_do_write(struct combiner *c) {
	if(!c->packet.receiver)
		return 0;
	encode_extended_speed(c);
	ccpacket_init(&c->packet);
	return 0;
}
