#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <stdbool.h>
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
	bool		debug;
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
uint8_t *buffer_append(struct buffer *buf, size_t n_bytes);
uint8_t *buffer_current(struct buffer *buf);
void buffer_skip(struct buffer *buf, size_t n_bytes);
void buffer_debug_in(struct buffer *buf, size_t n_bytes, const char *name);
void buffer_debug_out(struct buffer *buf, const char *name);

#endif
