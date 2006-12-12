#ifndef __LOG_H__
#define __LOG_H__

#include <stdbool.h>
#include <stdio.h>

struct log {
	FILE	*out;		/* output stream */
	bool	quiet;		/* quiet (only major events logged) */
	bool	debug;		/* debugging input commands */
	bool	stats;		/* stats logging */
};

struct log *log_init(struct log *log);
struct log *log_init_file(struct log *log, const char *filename);
void log_destroy(struct log *log);
void log_line_start(struct log *log);
void log_line_end(struct log *log);
void log_printf(struct log *log, const char *format, ...);
void log_println(struct log *log, const char *format, ...);

#endif
