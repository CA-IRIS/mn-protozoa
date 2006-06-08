#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/*
 * A buffer is used for I/O buffering. It consists of four pointers to a
 * heap memory buffer. Data is read into the buffer at "pin". Data is written
 * out of the buffer at "pout". So, base <= pout <= pin < end.
 */
struct buffer {
	uint8_t*	base;	/* base address of buffer */
	uint8_t*	end;	/* end address of buffer */
	uint8_t*	pin;	/* input pointer location */
	uint8_t*	pout;	/* output pointer location */
};

struct buffer *buffer_init(struct buffer *buf, size_t size);
void buffer_destroy(struct buffer *buf);
void buffer_clear(struct buffer *buf);
bool buffer_is_empty(const struct buffer *buf);
int buffer_available(const struct buffer *buf);
void buffer_skip(struct buffer *buf, size_t count);
bool buffer_is_full(const struct buffer *buf);
ssize_t buffer_read(struct buffer *buf, int fd);
ssize_t buffer_write(struct buffer *buf, int fd);
void buffer_put(struct buffer *buf, uint8_t value);
uint8_t buffer_peek(const struct buffer *buf);
uint8_t buffer_get(struct buffer *buf);

#endif
