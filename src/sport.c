#include <assert.h>
#include <fcntl.h>
#include <stddef.h>
#include <termios.h>
#include "sport.h"

static inline int baud_mask(int baud) {
	switch(baud) {
		case 1200:
			return B1200;
		case 2400:
			return B2400;
		case 4800:
			return B4800;
		case 9600:
			return B9600;
		case 19200:
			return B19200;
		case 38400:
			return B38400;
		default:
			return -1;
	}
}

static inline int sport_configure(struct channel *chn) {
	struct termios ttyset;

	ttyset.c_iflag = 0;
	ttyset.c_lflag = 0;
	ttyset.c_oflag = 0;
	ttyset.c_cflag = CREAD | CS8 | CLOCAL;
	ttyset.c_cc[VMIN] = 0;
	ttyset.c_cc[VTIME] = 1;

	/* sport baud rate stored in channel->extra parameter */
	int b = baud_mask(chn->extra);
	if(b < 0)
		return -1;
	if(cfsetispeed(&ttyset, b) < 0)
		return -1;
	if(cfsetospeed(&ttyset, b) < 0)
		return -1;
	if(tcsetattr(chn->fd, TCSAFLUSH, &ttyset) < 0)
		return -1;
	return 0;
}

int sport_open(struct channel *chn) {
	assert(chn->fd == 0);
	chn->fd = open(chn->name, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if(chn->fd < 0) {
		chn->fd = 0;
		return -1;
	}
	return sport_configure(chn);
}
