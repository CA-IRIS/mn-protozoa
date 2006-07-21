#include "buffer.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>	/* for read, write */
#include <sys/errno.h>

extern int errno;

struct buffer *buffer_init(struct buffer *buf, size_t n_bytes) {
	buf->base = malloc(n_bytes);
	if(buf->base == NULL)
		return NULL;
	buf->end = buf->base + n_bytes;
	buf->pin = buf->base;
	buf->pout = buf->base;
	buf->debug = false;
	return buf;
}

void buffer_destroy(struct buffer *buf) {
	free(buf->base);
	buf->base = NULL;
	buf->end = NULL;
	buf->pin = NULL;
	buf->pout = NULL;
}

void buffer_clear(struct buffer *buf) {
	buf->pin = buf->base;
	buf->pout = buf->base;
}

inline size_t buffer_available(const struct buffer *buf) {
	return buf->pin - buf->pout;
}

inline bool buffer_is_empty(const struct buffer *buf) {
	return buffer_available(buf) <= 0;
}

inline size_t buffer_space(const struct buffer *buf) {
	return buf->end - buf->pin;
}

inline bool buffer_is_full(const struct buffer *buf) {
	return buffer_space(buf) <= 0;
}

static inline void buffer_compact(struct buffer *buf) {
	size_t a = buffer_available(buf);
	memmove(buf->base, buf->pout, a);
	buf->pout = buf->base;
	buf->pin = buf->pout + a;
}

ssize_t buffer_read(struct buffer *buf, int fd) {
	size_t count = buffer_space(buf);
	if(count <= 0) {
		buffer_compact(buf);
		count = buffer_space(buf);
		if(count <= 0) {
			errno = ENOBUFS;
			return -1;
		}
	}
	ssize_t n_bytes = read(fd, buf->pin, count);
	if(n_bytes > 0)
		buf->pin += n_bytes;
	return n_bytes;
}

ssize_t buffer_write(struct buffer *buf, int fd) {
	size_t count = buffer_available(buf);
	if(count <= 0) {
		errno = ENOBUFS;
		return -1;
	}
	ssize_t n_bytes = write(fd, buf->pout, count);
	if(n_bytes > 0)
		buffer_skip(buf, n_bytes);
	return n_bytes;
}

void *buffer_append(struct buffer *buf, size_t n_bytes) {
	void *pin = buf->pin;
	if(buffer_space(buf) < n_bytes) {
		errno = ENOBUFS;
		return NULL;
	}
	buf->pin += n_bytes;
	return pin;
}

inline void *buffer_current(struct buffer *buf) {
	return buf->pout;
}

void buffer_skip(struct buffer *buf, size_t n_bytes) {
	buf->pout += n_bytes;
	if(buf->pout >= buf->pin)
		buffer_clear(buf);
}

static void buffer_debug(struct buffer *buf, const char *name,
	const char *prefix, void *start)
{
	uint8_t *mess;
	uint8_t *stop = buf->pin;
	fprintf(stderr, name);
	fprintf(stderr, prefix);
	for(mess = start; mess < stop; mess++)
		fprintf(stderr, " %02x", *mess);
	fprintf(stderr, "\n");
}

void buffer_debug_in(struct buffer *buf, size_t n_bytes, const char *name) {
	if(buf->debug)
		buffer_debug(buf, name, "  in:", buf->pin - n_bytes);
}

void buffer_debug_out(struct buffer *buf, const char *name) {
	if(buf->debug)
		buffer_debug(buf, name, " out:", buf->pout);
}
