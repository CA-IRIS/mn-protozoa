#include <stdio.h>
#include "sport.h"

#define FLAG 0x80

static inline int pt_bits(uint8_t *mess) {
	return (mess[1] >> 4) & 0x03;
}

static inline int pt_speed(uint8_t *mess) {
	return (mess[1] >> 1) & 0x07;
}

static int pan_value(uint8_t *mess) {
	int cmnd = pt_bits(mess);
	if(mess[2] & 0x02) {
		if(cmnd == 0x02)
			return -pt_speed(mess);
		else if(cmnd == 0x03)
			return pt_speed(mess);
	}
	return 0;
}

static int tilt_value(uint8_t *mess) {
	int cmnd = pt_bits(mess);
	if(mess[2] & 0x02) {
		if(cmnd == 0x00)
			return -pt_speed(mess);
		else if(cmnd == 0x01)
			return pt_speed(mess);
	}
	return 0;
}

static int manchester_read_message(struct buffer *rxbuf) {
	uint8_t mess[3];
	int i;

	while(buffer_peek(rxbuf) & FLAG == 0)
		printf("Manchester: unexpected byte %02X", buffer_get(rxbuf));
	for(i = 0; i < 3; i++) {
		if(buffer_is_empty(rxbuf)) {
			printf("Manchester: incomplete message");
			return -1;
		}
		mess[i] = buffer_get(rxbuf);
	}
	printf("%02x %02x %02x\n", mess[0], mess[1], mess[2]);
	int receiver = (mess[0] & 0x03) << 6 | (mess[1] & 0x01) << 5 |
		(mess[2] >> 2) & 0x1f;
	printf("receiver: %d pan: %d tilt: %d\n", receiver + 1,
		pan_value(mess), tilt_value(mess));
	return 0;
}

int manchester_do_read(struct handler *h, struct buffer *rxbuf) {
	while(!buffer_is_empty(rxbuf)) {
		if(manchester_read_message(rxbuf) < 0)
			return -1;
	}
	return 0;
}
