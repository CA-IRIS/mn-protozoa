#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * A buffer is used for I/O buffering. It consists of four pointers to a
 * heap memory buffer. Data is read into the buffer at "pin". Data is written
 * out of the buffer at "pout". So, base <= pout <= pin < end.
 */
struct buffer {
	void	*base;	/* base address of buffer */
	void	*end;	/* end address of buffer */
	void	*pin;	/* input pointer location */
	void	*pout;	/* output pointer location */
};

struct buffer *buffer_init(struct buffer *buf, size_t n_bytes);
void buffer_destroy(struct buffer *buf);
void buffer_clear(struct buffer *buf);
size_t buffer_available(const struct buffer *buf);
bool buffer_is_empty(const struct buffer *buf);
size_t buffer_space(const struct buffer *buf);
bool buffer_is_full(const struct buffer *buf);
ssize_t buffer_read(struct buffer *buf, int fd);
ssize_t buffer_write(struct buffer *buf, int fd);
void *buffer_append(struct buffer *buf, size_t n_bytes);
void *buffer_next(struct buffer *buf);
void *buffer_current(struct buffer *buf);
void buffer_consume(struct buffer *buf, size_t n_bytes);

#endif
