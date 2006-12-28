#include <assert.h>	/* for assert */
#include <string.h>	/* for memmove */
#include <unistd.h>	/* for read, write */
#include <sys/errno.h>	/* for errno */
#include "buffer.h"	/* for struct buffer and prototypes */

extern int errno;

/*
 * buffer_init		Initialize a new I/O buffer.
 *
 * n_bytes: size of buffer (bytes)
 * return: pointer to the buffer or NULL on error
 */
struct buffer *buffer_init(struct buffer *buf, size_t n_bytes) {
	buf->base = malloc(n_bytes);
	if(buf->base == NULL)
		return NULL;
	buf->end = buf->base + n_bytes;
	buf->pin = buf->base;
	buf->pout = buf->base;
	return buf;
}

/*
 * buffer_destroy	Destroy the previously initialized I/O buffer.
 */
void buffer_destroy(struct buffer *buf) {
	free(buf->base);
	buf->base = NULL;
	buf->end = NULL;
	buf->pin = NULL;
	buf->pout = NULL;
}

/*
 * buffer_clear		Clear the contents of the I/O buffer.
 */
void buffer_clear(struct buffer *buf) {
	buf->pin = buf->base;
	buf->pout = buf->base;
}

/*
 * buffer_available	Get the number of bytes available in the I/O buffer.
 *
 * return: number of bytes available
 */
inline size_t buffer_available(const struct buffer *buf) {
	assert(buf->pin >= buf->pout);
	return buf->pin - buf->pout;
}

/*
 * buffer_is_empty	Test if the I/O buffer is empty.
 *
 * return: true if buffer is empty; otherwise false
 */
inline bool buffer_is_empty(const struct buffer *buf) {
	return buffer_available(buf) == 0;
}

/*
 * buffer_space		Get the space remaining in the I/O buffer.
 *
 * return: space (bytes) remaining in buffer
 */
inline size_t buffer_space(const struct buffer *buf) {
	assert(buf->end >= buf->pin);
	return buf->end - buf->pin;
}

/*
 * buffer_is_full	Test if the I/O buffer is full.
 *
 * return: true if buffer is full; otherwise false
 */
inline bool buffer_is_full(const struct buffer *buf) {
	return buffer_space(buf) == 0;
}

/*
 * buffer_compact	Compact the I/O buffer to free up more space.
 */
static inline void buffer_compact(struct buffer *buf) {
	size_t a = buffer_available(buf);
	memmove(buf->base, buf->pout, a);
	buf->pout = buf->base;
	buf->pin = buf->pout + a;
}

/*
 * buffer_read		Read data from a file descriptor into the I/O buffer.
 *
 * fd: file descriptor to read data from
 * return: number of bytes read; -1 on error (with errno set)
 */
ssize_t buffer_read(struct buffer *buf, int fd) {
	size_t count = buffer_space(buf);
	if(count == 0) {
		buffer_compact(buf);
		count = buffer_space(buf);
		if(count == 0) {
			errno = ENOBUFS;
			return -1;
		}
	}
	ssize_t n_bytes = read(fd, buf->pin, count);
	if(n_bytes > 0)
		buf->pin += n_bytes;
	return n_bytes;
}

/*
 * buffer_write		Write data from the I/O buffer to a file descriptor.
 *
 * fd: file descriptor to write data to
 * return: number of bytes written; -1 on error (with errno set)
 */
ssize_t buffer_write(struct buffer *buf, int fd) {
	size_t count = buffer_available(buf);
	if(count == 0) {
		errno = ENOBUFS;
		return -1;
	}
	ssize_t n_bytes = write(fd, buf->pout, count);
	if(n_bytes > 0)
		buffer_consume(buf, n_bytes);
	return n_bytes;
}

/*
 * buffer_append	Append data to the I/O buffer.
 *
 * n_bytes: number of bytes to append
 * return: pointer to the appended data, or NULL on error
 */
void *buffer_append(struct buffer *buf, size_t n_bytes) {
	void *pin = buf->pin;
	if(buffer_space(buf) < n_bytes) {
		errno = ENOBUFS;
		return NULL;
	}
	buf->pin += n_bytes;
	return pin;
}

/*
 * buffer_next		Get the next position in the I/O buffer.
 *
 * return: pointer to the next buffer position
 */
inline void *buffer_next(struct buffer *buf) {
	return buf->pin;
}

/*
 * buffer_current	Get the current position in the I/O buffer.
 *
 * return: pointer to the current buffer position
 */
inline void *buffer_current(struct buffer *buf) {
	return buf->pout;
}

/*
 * buffer_consume	Consume data from the I/O buffer.
 *
 * n_bytes: number of bytes to consume from the current position
 */
void buffer_consume(struct buffer *buf, size_t n_bytes) {
	buf->pout += n_bytes;
	assert(buf->pout <= buf->pin);
	if(buf->pout == buf->pin)
		buffer_clear(buf);
}
