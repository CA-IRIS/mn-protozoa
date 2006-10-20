#include <fcntl.h>
#include <stddef.h>
#include <termios.h>
#include <sys/errno.h>
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

static inline struct channel* sport_configure(struct channel *chn, int baud) {
	struct termios ttyset;

	ttyset.c_iflag = 0;
	ttyset.c_lflag = 0;
	ttyset.c_oflag = 0;
	ttyset.c_cflag = CREAD | CS8 | CLOCAL;
	ttyset.c_cc[VMIN] = 0;
	ttyset.c_cc[VTIME] = 1;

	int b = baud_mask(baud);
	if(b < 0)
		return NULL;
	if(cfsetispeed(&ttyset, b) < 0)
		return NULL;
	if(cfsetospeed(&ttyset, b) < 0)
		return NULL;
	if(tcsetattr(chn->fd, TCSAFLUSH, &ttyset) < 0)
		return NULL;

	return chn;
}

struct channel* sport_init(struct channel *chn, const char *name, int baud) {
	if(channel_init(chn, name) == NULL)
		return NULL;
	chn->fd = open(name, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if(chn->fd < 0)
		goto fail;
	if(sport_configure(chn, baud) == NULL)
		goto fail;
	return chn;
fail:
	channel_destroy(chn);
	return NULL;
}
