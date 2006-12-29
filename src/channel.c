#include <assert.h>		/* for assert */
#include <fcntl.h>		/* for open, O_RDWR, O_NOCTTY, O_NONBLOCK */
#include <netdb.h>		/* for socket stuff */
#include <netinet/tcp.h>	/* for TCP_NODELAY */
#include <unistd.h>		/* for close */
#include <string.h>		/* for bzero, strlen, strcpy */
#include <strings.h>		/* for bcopy */
#include <termios.h>		/* for serial port stuff */
#include "channel.h"		/* for struct channel and prototypes */

#define BUFFER_SIZE 256

/*
 * channel_init		Initialize a new I/O channel.
 *
 * name: channel name
 * extra: extra data to initialize the channel
 * log: message logger
 * return: struct channel or NULL on error
 */
struct channel* channel_init(struct channel *chn, const char *name, int extra,
	struct log *log)
{
	bzero(chn, sizeof(struct channel));
	chn->extra = extra;
	chn->log = log;
	chn->name = malloc(strlen(name) + 1);
	if(chn->name == NULL)
		goto fail;
	strcpy(chn->name, name);
	if(buffer_init(&chn->rxbuf, BUFFER_SIZE) == NULL)
		goto fail;
	if(buffer_init(&chn->txbuf, BUFFER_SIZE) == NULL)
		goto fail;
	chn->reader = NULL;
	return chn;
fail:
	free(chn->name);
	chn->name = NULL;
	return NULL;
}

/*
 * channel_destroy	Destroy the previously initialized I/O channel.
 */
void channel_destroy(struct channel *chn) {
	channel_close(chn);
	buffer_destroy(&chn->rxbuf);
	buffer_destroy(&chn->txbuf);
	free(chn->name);
}

/*
 * channel_is_sport	Test if the channel is a serial port.
 *
 * return: true if channel is a serial port; otherwise false
 */
static inline bool channel_is_sport(const struct channel *chn) {
	return chn->name[0] == '/';
}

static inline int channel_sport_baud_mask(int baud) {
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

static inline int channel_configure_sport(struct channel *chn) {
	struct termios ttyset;

	ttyset.c_iflag = 0;
	ttyset.c_lflag = 0;
	ttyset.c_oflag = 0;
	ttyset.c_cflag = CREAD | CS8 | CLOCAL;
	ttyset.c_cc[VMIN] = 0;
	ttyset.c_cc[VTIME] = 1;

	/* serial port baud rate stored in chn->extra parameter */
	int b = channel_sport_baud_mask(chn->extra);
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

static int channel_open_sport(struct channel *chn) {
	chn->fd = open(chn->name, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if(chn->fd < 0) {
		chn->fd = 0;
		return -1;
	}
	return channel_configure_sport(chn);
}

static int channel_open_tcp(struct channel *chn) {
	struct hostent *host;
	struct sockaddr_in sa;
	int on = 1;	/* turn "on" values for setsockopt */

	host = gethostbyname(chn->name);
	if(host == NULL)
		return -1;
	chn->fd = socket(PF_INET, SOCK_STREAM, 0);
	if(chn->fd < 0) {
		chn->fd = 0;
		return -1;
	}
	if(setsockopt(chn->fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) < 0)
		return -1;
	if(setsockopt(chn->fd, SOL_IP, IP_RECVERR, &on, sizeof(on)) < 0)
		return -1;
	sa.sin_family = AF_INET;
	bcopy(host->h_addr, &sa.sin_addr.s_addr, host->h_length);
	/* tcp port stored in chn->extra parameter */
	sa.sin_port = htons(chn->extra);
	if(connect(chn->fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
		return -1;
	else
		return 0;
}

/*
 * channel_open		Open the I/O channel.
 *
 * return: 0 on success; -1 on error
 */
int channel_open(struct channel *chn) {
	assert(chn->fd == 0);
	channel_log(chn, "opening");
	if(channel_is_sport(chn))
		return channel_open_sport(chn);
	else
		return channel_open_tcp(chn);
}

/*
 * channel_close	Close the I/O channel.
 *
 * return: 0 on success; -1 on error
 */
int channel_close(struct channel *chn) {
	buffer_clear(&chn->rxbuf);
	buffer_clear(&chn->txbuf);
	if(channel_is_open(chn)) {
		channel_log(chn, "closing");
		int r = close(chn->fd);
		chn->fd = 0;
		return r;
	} else
		return 0;
}

/*
 * channel_is_open	Test if the I/O channel is currently open.
 *
 * return: true if channel is open; otherwise false
 */
bool channel_is_open(const struct channel *chn) {
	return (bool)(chn->fd);
}

/*
 * channel_has_reader	Test if the I/O channel has a reader.
 *
 * return: true if channel has a reader; otherwise false
 */
bool channel_has_reader(const struct channel *chn) {
	return chn->reader != NULL;
}

/*
 * channel_is_waiting	Test if the I/O channel is waiting to read or write.
 *
 * return: true if the channel is waiting; otherwise false
 */
bool channel_is_waiting(const struct channel *chn) {
	return (!buffer_is_empty(&chn->txbuf)) || (chn->reader != NULL);
}

/*
 * channel_log		Log a message related to the I/O channel.
 *
 * msg: message to write to log
 */
void channel_log(struct channel *chn, const char* msg) {
	log_println(chn->log, "channel: %s %s", msg, chn->name);
}

/*
 * channel_read		Read from the I/O channel.
 *
 * return: number of bytes read; -1 on error
 */
ssize_t channel_read(struct channel *chn) {
	ssize_t n_bytes = buffer_read(&chn->rxbuf, chn->fd);
	if(n_bytes <= 0)
		return n_bytes;
	if(channel_has_reader(chn)) {
		log_buffer_in(chn->log, &chn->rxbuf, chn->name, n_bytes);
		chn->reader->do_read(chn->reader, &chn->rxbuf);
		return n_bytes;
	} else {
		/* Data is coming in on the channel, but we're not set up to
		 * handle it -- just ignore. */
		buffer_clear(&chn->rxbuf);
		return 0;
	}
}

/*
 * channel_write	Write buffered data to the I/O channel.
 *
 * return: number of bytes written; -1 on error
 */
ssize_t channel_write(struct channel *chn) {
	log_buffer_out(chn->log, &chn->txbuf, chn->name);
	return buffer_write(&chn->txbuf, chn->fd);
}
