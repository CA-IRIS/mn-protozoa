#include <fcntl.h>
#include <stddef.h>
#include <string.h>
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

static inline struct sport* sport_configure(struct sport *port, int baud) {
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
	if(tcsetattr(port->fd, TCSAFLUSH, &ttyset) < 0)
		return NULL;

	return port;
}

struct sport* sport_init(struct sport *port, const char *name, int baud) {
	bzero(port, sizeof(struct sport));
	port->name = malloc(strlen(name) + 1);
	strcpy(port->name, name);
	port->baud = baud;
	port->rxbuf = malloc(BUFFER_SIZE);
	if(port->rxbuf == NULL)
		goto fail;
	if(buffer_init(port->rxbuf, BUFFER_SIZE) == NULL)
		goto fail;
	port->txbuf = malloc(BUFFER_SIZE);
	if(port->txbuf == NULL)
		goto fail;
	if(buffer_init(port->txbuf, BUFFER_SIZE) == NULL)
		goto fail;
	port->handler = NULL;
	port->fd = open(name, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if(port->fd < 0)
		goto fail;
	return sport_configure(port, baud);
fail:
	free(port->name);
	free(port->rxbuf);
	free(port->txbuf);
	return NULL;
}

ssize_t sport_read(struct sport *port) {
	ssize_t n_bytes = buffer_read(port->rxbuf, port->fd);
	if(n_bytes <= 0)
		return n_bytes;
	if(port->handler) {
		buffer_debug_in(port->rxbuf, n_bytes, port->name);
		port->handler->do_read(port->handler, port->rxbuf);
		return n_bytes;
	} else {
		/* Data is coming in on the port, but we're not set up to
		 * handle it -- just ignore. */
		return 0;
	}
}

ssize_t sport_write(struct sport *port) {
	buffer_debug_out(port->txbuf, port->name);
	return buffer_write(port->txbuf, port->fd);
}
