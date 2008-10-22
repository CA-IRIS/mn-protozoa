/*
 * protozoa -- CCTV transcoder / mixer for PTZ
 * Copyright (C) 2006-2008  Minnesota Department of Transportation
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
#include <assert.h>		/* for assert */
#include <errno.h>		/* for EINPROGRESS */
#include <fcntl.h>		/* for open, O_RDWR, O_NOCTTY, O_NONBLOCK */
#include <netdb.h>		/* for socket stuff */
#include <netinet/tcp.h>	/* for TCP_NODELAY */
#include <unistd.h>		/* for close */
#include <string.h>		/* for memset, memcpy, strlen, strcpy */
#include <termios.h>		/* for serial port stuff */
#include "channel.h"		/* for struct channel and prototypes */

#define BUFFER_SIZE 256

/*
 * channel_fill_sockaddr	Fill a sockaddr structure
 *
 * return: NULL on error; pointer to struct sockaddr on success
 */
static struct sockaddr_in *channel_fill_sockaddr(struct channel *chn,
	struct sockaddr_in *sa)
{
	struct hostent *host = gethostbyname(chn->name);
	if(host == NULL)
		return NULL;
	sa->sin_family = AF_INET;
	memcpy(&sa->sin_addr.s_addr, host->h_addr, host->h_length);
	/* tcp port stored in chn->extra parameter */
	sa->sin_port = htons(chn->extra);
	return sa;
}

/*
 * channel_set_tcp_keepalive	Set keepalive option on a socket. This is
 *				needed because some sockets never write data,
 *				so they will never notice a connection is lost
 *				without using keepalive probes.
 *
 * return: fd of socket on success; -1 on error
 */
static int channel_set_tcp_keepalive(int fd) {
	static int on = 1;		/* turn "on" values for setsockopt */
	static int kcnt = 4;		/* 4 keepalive probes */
	static int kidle = 30;		/* 30 second keepalive idle time */
	static int kintvl = 10;		/* 10 second keepalive interval time */
	if(setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) < 0)
		return -1;
	if(setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &kcnt, sizeof(kcnt)) < 0)
		return -1;
	if(setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &kidle, sizeof(kidle)) < 0)
		return -1;
	if(setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &kintvl, sizeof(kintvl)) < 0)
		return -1;
	return fd;
}

/*
 * channel_open_socket	Open a TCP socket for the I/O channel.
 *
 * return: fd of socket on success; -1 on error
 */
static int channel_open_socket() {
	int on = 1;	/* turn "on" values for setsockopt */
	int fd = socket(PF_INET, SOCK_STREAM, 0);
	if(fd < 0)
		return -1;
	if(fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
		goto fail;
	if(channel_set_tcp_keepalive(fd) < 0)
		goto fail;
	if(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) < 0)
		goto fail;
	if(setsockopt(fd, SOL_IP, IP_RECVERR, &on, sizeof(on)) < 0)
		goto fail;
	return fd;
fail:
	close(fd);
	return -1;
}

/*
 * channel_init		Initialize a new I/O channel.
 *
 * name: channel name
 * extra: extra data to initialize the channel
 * listen: flag to indicate tcp listen channel
 * log: message logger
 * return: struct channel or NULL on error
 */
struct channel* channel_init(struct channel *chn, const char *name, int extra,
	bool listen, struct log *log)
{
	memset(chn, 0, sizeof(struct channel));
	chn->extra = extra;
	chn->listen = listen;
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
	memset(chn, 0, sizeof(struct channel));
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
	memset(chn, 0, sizeof(struct channel));
}

/*
 * channel_matches	Test if a channel matches the given parameters.
 */
bool channel_matches(struct channel *chn, const char *name, int extra,
	bool listen)
{
	if(strcmp(chn->name, name) == 0)
		return (chn->extra == extra) && (chn->listen == listen);
	else
		return false;
}

/*
 * channel_is_sport	Test if the channel is a serial port.
 *
 * return: true if channel is a serial port; otherwise false
 */
static inline bool channel_is_sport(const struct channel *chn) {
	return chn->name[0] == '/';
}

/*
 * channel_sport_baud_mask	Get the baud mask for a baud rate.
 *
 * baud: baud rate to get the mask for
 * return: baud mask
 */
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

/*
 * channel_configure_sport	Configure a serial port for the I/O channel.
 *
 * return: 0 on success; -1 on error
 */
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

/*
 * channel_open_sport	Open a serial port for the I/O channel.
 *
 * return: 0 on success; -1 on error
 */
static int channel_open_sport(struct channel *chn) {
	do {
		chn->fd = open(chn->name, O_RDWR | O_NOCTTY | O_NONBLOCK);
	} while(chn->fd < 0 && errno == EINTR);
	if(chn->fd < 0) {
		chn->fd = 0;
		return -1;
	}
	if(chn->extra)
		return channel_configure_sport(chn);
	else
		return 0;
}

/*
 * channel_open_listener	Open a tcp port for listening.
 *
 * return: 0 on success; -1 on error
 */
static int channel_open_listener(struct channel *chn) {
	static int on = 1;	/* turn "on" values for setsockopt */
	struct sockaddr_in sa;
	if(channel_fill_sockaddr(chn, &sa) == NULL)
		return -1;
	chn->fd = channel_open_socket();
	if(chn->fd < 0) {
		chn->fd = 0;
		return -1;
	}
	if(setsockopt(chn->fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
		goto fail;
	if(bind(chn->fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
		goto fail;
	if(listen(chn->fd, 1) < 0)
		goto fail;
	chn->sfd = chn->fd;
	return 0;
fail:
	close(chn->fd);
	return -1;
}

/*
 * channel_accept	Accept a tcp client connection on the I/O channel.
 *
 * return: fd on success; -1 on error
 */
static int channel_accept(struct channel *chn) {
	int fd = accept(chn->sfd, NULL, 0);
	if(fd < 0)
		return fd;
	channel_log(chn, "accepting");
	chn->fd = fd;
	return fd;
}

/*
 * channel_open_tcp	Open a tcp port for the I/O channel.
 *
 * return: 0 on success; -1 on error
 */
static int channel_open_tcp(struct channel *chn) {
	struct sockaddr_in sa;
	int r;
	if(channel_fill_sockaddr(chn, &sa) == NULL)
		return -1;
	chn->fd = channel_open_socket();
	if(chn->fd < 0) {
		chn->fd = 0;
		return -1;
	}
	r = connect(chn->fd, (struct sockaddr *)&sa, sizeof(sa));
	if(r < 0 && errno == EINPROGRESS)
		return 0;
	else
		return r;
}

/*
 * channel_is_localhost	Test if the I/O channel is a localhost address.
 *
 * return: true if channel is defined to be a localhost address
 */
static bool channel_is_localhost(const struct channel *chn) {
	if(strstr(chn->name, "localhost") == chn->name)
		return true;
	if(strstr(chn->name, "0.0.0.0") == chn->name)
		return true;
	return false;
}

/*
 * channel_should_listen	Test if the I/O channel should listen.
 *
 * return: true if the channel should listen; otherwise false
 */
static bool channel_should_listen(const struct channel *chn) {
	return chn->listen && channel_is_localhost(chn);
}

/*
 * channel_open		Open the I/O channel.
 *
 * return: 0 on success; -1 on error
 */
int channel_open(struct channel *chn) {
	assert(chn->fd == 0);
	chn->needs_response = false;
	if(channel_should_listen(chn))
		channel_log(chn, "listening");
	else
		channel_log(chn, "opening");
	if(channel_is_sport(chn))
		return channel_open_sport(chn);
	else {
		if(channel_should_listen(chn))
			return channel_open_listener(chn);
		else
			return channel_open_tcp(chn);
	}
}

/*
 * channel_close	Close the I/O channel.
 *
 * return: 0 on success; -1 on error
 */
int channel_close(struct channel *chn) {
	/* don't clear txbuf; send buffered data after channel is reopened */
	buffer_clear(&chn->rxbuf);
	if(channel_is_open(chn)) {
		channel_log(chn, "closing");
		int r = close(chn->fd);
		chn->fd = chn->sfd;
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
 * channel_needs_reading	Test if the I/O channel needs reading.
 *
 * return: true if channel needs to be read; otherwise false
 */
bool channel_needs_reading(const struct channel *chn) {
	return channel_has_reader(chn) || chn->needs_response;
}

/*
 * channel_needs_writing	Test if the I/O channel needs writing.
 *
 * return true if channel needs to be writtin; otherwise false
 */
bool channel_needs_writing(const struct channel *chn) {
	return !(buffer_is_empty(&chn->txbuf) || chn->needs_response);
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
 * channel_is_listening	Test if the I/O channel is listening.
 *
 * return: true if the channel is listening; otherwise false
 */
static inline bool channel_is_listening(const struct channel *chn) {
	return chn->sfd == chn->fd;
}

/*
 * channel_log		Log a message related to the I/O channel.
 *
 * msg: message to write to log
 */
void channel_log(struct channel *chn, const char* msg) {
	log_println(chn->log, "channel: %s %s:%d", msg, chn->name,
		chn->extra);
}

/*
 * channel_log_buffer	Log buffer debug information.
 *
 * buf: buffer to debug
 * prefix: prefix to print on the log message
 * start: pointer to start of buffer debug information
 */
static void channel_log_buffer(struct channel *chn, struct buffer *buf,
	const char *prefix, void *start)
{
	uint8_t *mess;
	uint8_t *stop = buffer_input(buf);

	log_line_start(chn->log);
	log_printf(chn->log, prefix);
	log_printf(chn->log, " %s:%d", chn->name, chn->extra);
	for(mess = start; mess < stop; mess++)
		log_printf(chn->log, " %02x", *mess);
	log_line_end(chn->log);
}

/*
 * channel_log_buffer_in	Log channel receive buffer information.
 *
 * n_bytes: number of bytes received
 */
static void channel_log_buffer_in(struct channel *chn, size_t n_bytes) {
	if(chn->log->debug) {
		channel_log_buffer(chn, &chn->rxbuf, "debug: IN",
			buffer_input(&chn->rxbuf) - n_bytes);
	}
}

/*
 * channel_log_buffer_out	Log channel transmit buffer information.
 */
static void channel_log_buffer_out(struct channel *chn) {
	if(chn->log->debug) {
		channel_log_buffer(chn, &chn->txbuf, "debug: OUT",
			buffer_output(&chn->txbuf));
	}
}

/*
 * channel_read		Read from the I/O channel.
 *
 * return: number of bytes read; -1 on error
 */
ssize_t channel_read(struct channel *chn) {
	ssize_t n_bytes;

	if(channel_is_listening(chn))
		return channel_accept(chn);
	n_bytes = buffer_read(&chn->rxbuf, chn->fd);
	if(n_bytes <= 0)
		return n_bytes;
	chn->needs_response = false;
	if(channel_has_reader(chn)) {
		channel_log_buffer_in(chn, n_bytes);
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
	chn->needs_response = chn->response_required;
	channel_log_buffer_out(chn);
	return buffer_write(&chn->txbuf, chn->fd);
}
