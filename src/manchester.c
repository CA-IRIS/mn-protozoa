#include <stdio.h>
#include "sport.h"

#define FLAG 0x80

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
	printf("receiver %d\n", receiver + 1);
	return 0;
}

int manchester_do_read(struct handler *h, struct buffer *rxbuf) {
	while(!buffer_is_empty(rxbuf)) {
		if(manchester_read_message(rxbuf) < 0)
			return -1;
	}
	return 0;
}
