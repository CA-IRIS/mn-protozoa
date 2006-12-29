#include <stdarg.h>	/* for va_list, va_start, va_end */
#include <stdint.h>	/* for uint8_t */
#include <time.h>	/* for localtime, strftime */
#include <sys/time.h>	/* for gettimeofday */
#include "log.h"	/* for struct log, prototypes */

/*
 * log_init		Initialize a new message log.
 *
 * return: pointer to struct log or NULL on failure
 */
struct log *log_init(struct log *log) {
	log->out = stderr;
	log->debug = false;
	log->packet = false;
	log->stats = false;
	return log;
}

/*
 * log_open_file	Open a message log file.
 *
 * filename: name of file to append messages
 * return: pointer to struct log or NULL on failure
 */
struct log *log_open_file(struct log *log, const char *filename) {
	FILE *out = fopen(filename, "a");
	if(out) {
		log->out = out;
		return log;
	} else
		return NULL;
}

/*
 * log_destroy		Destroy a previously initialized message log.
 */
void log_destroy(struct log *log) {
	fclose(log->out);
}

/*
 * log_line_start	Start a new line in the message log.
 */
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

/*
 * log_line_end		End the current line in the message log.
 */
void log_line_end(struct log *log) {
	fprintf(log->out, "\n");
	fflush(log->out);
}

/*
 * log_printf		Print a message on the current line in the message log.
 *
 * format: printf format string
 * ...: printf arguments
 */
void log_printf(struct log *log, const char *format, ...) {
	va_list va;
	va_start(va, format);
	vfprintf(log->out, format, va);
	va_end(va);
}

/*
 * log_println		Print a full message on a new line in the message log.
 *
 * format: printf format string
 * ...: printf arguments
 */
void log_println(struct log *log, const char *format, ...) {
	va_list va;
	va_start(va, format);
	log_line_start(log);
	vfprintf(log->out, format, va);
	log_line_end(log);
	va_end(va);
}
