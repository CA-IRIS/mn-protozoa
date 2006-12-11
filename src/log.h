#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>

struct log {
	FILE	*out;
};

struct log *log_init(struct log *log);
struct log *log_init_file(struct log *log, const char *filename);
void log_destroy(struct log *log);
void log_line_start(struct log *log);
void log_line_end(struct log *log);
void log_printf(struct log *log, const char *format, ...);
void log_println(struct log *log, const char *format, ...);

#endif
