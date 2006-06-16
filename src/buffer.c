#include "buffer.h"
#include <string.h>
#include <unistd.h>	/* for read, write */
#include <sys/errno.h>

extern int errno;

struct buffer *buffer_init(struct buffer *buf, size_t size) {
	buf->base = malloc(size);
	if(buf->base == NULL)
		return NULL;
	buf->end = buf->base + size;
	buf->pin = buf->base;
	buf->pout = buf->base;
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

inline int buffer_available(const struct buffer *buf) {
	return buf->pin - buf->pout;
}

inline bool buffer_is_empty(const struct buffer *buf) {
	return buffer_available(buf) <= 0;
}

void buffer_skip(struct buffer *buf, size_t count) {
	buf->pout += count;
	if(buf->pout >= buf->pin)
		buffer_clear(buf);
}

inline bool buffer_is_full(const struct buffer *buf) {
	return buf->pin >= buf->end;
}

inline int buffer_remaining(const struct buffer *buf) {
	return buf->end - buf->pin;
}

int buffer_compact(struct buffer *buf) {
	int a = buffer_available(buf);
	memmove(buf->base, buf->pout, a);
	buf->pout = buf->base;
	buf->pin = buf->pout + a;
	return buffer_remaining(buf);
}

ssize_t buffer_read(struct buffer *buf, int fd) {
	size_t count = buffer_remaining(buf);
	if(count <= 0) {
		count = buffer_compact(buf);
		if(count <= 0) {
			errno = ENOBUFS;
			return -1;
		}
	}
	ssize_t nbytes = read(fd, buf->pin, count);
	if(nbytes > 0)
		buf->pin += nbytes;
	return nbytes;
}

ssize_t buffer_write(struct buffer *buf, int fd) {
	size_t count = buf->pin - buf->pout;
	if(count <= 0) {
		errno = ENOBUFS;
		return -1;
	}
	ssize_t nbytes = write(fd, buf->pout, count);
	if(nbytes < 0)
		return nbytes;
	if(nbytes == count)
		buffer_clear(buf);
	else
		buf->pout += nbytes;
	return nbytes;
}

inline void buffer_put(struct buffer *buf, uint8_t value) {
	*buf->pin = value;
	buf->pin++;
}

inline uint8_t buffer_get(struct buffer *buf) {
	uint8_t value = *buf->pout;
	buf->pout++;
	if(buf->pout == buf->pin)
		buffer_clear(buf);
	return value;
}

inline uint8_t buffer_peek(const struct buffer *buf) {
	return *buf->pout;
}
