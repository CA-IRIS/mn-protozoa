#include <stddef.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <sys/errno.h>
#include "sport.h"

static struct sport* sport_configure(struct sport *port, int baud) {
	struct termios ttyset;

	ttyset.c_iflag = 0;
	ttyset.c_lflag = 0;
	ttyset.c_oflag = 0;
	ttyset.c_cflag = CREAD | CS8 | CLOCAL;
	ttyset.c_cc[VMIN] = 0;
	ttyset.c_cc[VTIME] = 1;

	if(baud != 9600)
		return NULL;
	if(cfsetispeed(&ttyset, B9600) < 0)
		return NULL;
	if(cfsetospeed(&ttyset, B9600) < 0)
		return NULL;
	if(tcsetattr(port->fd, TCSAFLUSH, &ttyset) < 0)
		return NULL;

	return port;
}

static int sport_do_read(struct handler *h, struct buffer *rxbuf) {
	/* Do nothing */
	return 0;
}

static struct handler null_handler = {
	.do_read = sport_do_read,
};

struct sport* sport_init(struct sport *port, const char *name, int baud) {
	port->name = malloc(strlen(name) + 1);
	strcpy(port->name, name);
	if(buffer_init(&port->rxbuf, BUFFER_SIZE) == NULL)
		return NULL;
	if(buffer_init(&port->txbuf, BUFFER_SIZE) == NULL)
		return NULL;
	port->handler = &null_handler;
	port->fd = open(name, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if(port->fd < 0)
		return NULL;
	return sport_configure(port, baud);
}

ssize_t sport_read(struct sport *port) {
	ssize_t nbytes = buffer_read(&port->rxbuf, port->fd);
	if(nbytes <= 0)
		return nbytes;
	if(port->handler->do_read(port->handler, &port->rxbuf) < 0)
		return -1;
	return nbytes;
}

ssize_t sport_write(struct sport *port) {
	return buffer_write(&port->txbuf, port->fd);
}
