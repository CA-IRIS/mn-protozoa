#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "log.h"

struct log *log_init(struct log *log) {
	log->out = stderr;
	log->debug = false;
	log->packet = false;
	log->stats = false;
	return log;
}

struct log *log_open_file(struct log *log, const char *filename) {
	FILE *out = fopen(filename, "a");
	if(out) {
		log->out = out;
		return log;
	} else
		return NULL;
}

void log_destroy(struct log *log) {
	fclose(log->out);
}

void log_line_start(struct log *log) {
	struct timeval tv;
	struct timezone tz;
	struct tm *now;
	char buf[17];

	gettimeofday(&tv, &tz);
	now = localtime(&tv.tv_sec);
	strftime(buf, 17, "%b %d %H:%M:%S ", now);
	fprintf(log->out, buf);
}

void log_line_end(struct log *log) {
	fprintf(log->out, "\n");
	fflush(log->out);
}

void log_printf(struct log *log, const char *format, ...) {
	va_list va;
	va_start(va, format);
	vfprintf(log->out, format, va);
	va_end(va);
}

void log_println(struct log *log, const char *format, ...) {
	va_list va;
	va_start(va, format);
	log_line_start(log);
	vfprintf(log->out, format, va);
	log_line_end(log);
	va_end(va);
}

static void log_buffer(struct log *log, struct buffer *buf, const char *prefix,
	const char *name, void *start)
{
	uint8_t *mess;
	uint8_t *stop = buffer_next(buf);

	log_line_start(log);
	log_printf(log, prefix, name);
	for(mess = start; mess < stop; mess++)
		log_printf(log, " %02x", *mess);
	log_line_end(log);
}

void log_buffer_in(struct log *log, struct buffer *buf, const char *name,
	size_t n_bytes)
{
	if(log->debug)
		log_buffer(log, buf, "%s  in:", name, buffer_next(buf)-n_bytes);
}

void log_buffer_out(struct log *log, struct buffer *buf, const char *name) {
	if(log->debug)
		log_buffer(log, buf, "%s out:", name, buffer_current(buf));
}
