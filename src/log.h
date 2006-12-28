#ifndef __LOG_H__
#define __LOG_H__

#include <stdbool.h>
#include <stdio.h>
#include "buffer.h"

struct log {
	FILE	*out;		/* output stream */
	bool	debug;		/* debug raw input/output data */
	bool	packet;		/* log packet details */
	bool	stats;		/* log packet statistics */
};

struct log *log_init(struct log *log);
struct log *log_open_file(struct log *log, const char *filename);
void log_destroy(struct log *log);
void log_line_start(struct log *log);
void log_line_end(struct log *log);
void log_printf(struct log *log, const char *format, ...);
void log_println(struct log *log, const char *format, ...);
void log_buffer_in(struct log *log, struct buffer *buf, size_t n_bytes);
void log_buffer_out(struct log *log, struct buffer *buf);

#endif
