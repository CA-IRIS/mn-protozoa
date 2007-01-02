#include <string.h>
#include "config.h"
#include "ccreader.h"
#include "ccwriter.h"

struct config *config_init(struct config *cfg, struct log *log) {
	bzero(cfg, sizeof(struct config));
	cfg->line = malloc(LINE_LENGTH);
	if(cfg->line == NULL)
		goto fail;
	cfg->chns = NULL;
	cfg->n_channels = 0;
	cfg->log = log;
	if(log->stats) {
		cfg->counter = malloc(sizeof(struct packet_counter));
		if(cfg->counter == NULL)
			goto fail;
		counter_init(cfg->counter, log);
	} else
		cfg->counter = NULL;
	return cfg;
fail:
	free(cfg->line);
	return NULL;
}

void config_destroy(struct config *cfg) {
	free(cfg->counter);
	free(cfg->chns);
	free(cfg->line);
	cfg->counter = NULL;
	cfg->chns = NULL;
	cfg->line = NULL;
}

static struct channel *config_find_channel(struct config *cfg,
	const char *name)
{
	int i;
	for(i = 0; i < cfg->n_channels; i++) {
		if(strcmp(name, cfg->chns[i].name) == 0)
			return cfg->chns + i;
	}
	return NULL;
}

static struct channel *config_new_channel(struct config *cfg, const char *name,
	int extra)
{
	struct channel *chn, *chns;

	chns = realloc(cfg->chns, sizeof(struct channel) * cfg->n_channels + 1);
	if(chns == NULL)
		goto fail;
	chn = chns + cfg->n_channels;
	if(channel_init(chn, name, extra, cfg->log) == NULL)
		goto fail;
	cfg->n_channels++;
	cfg->chns = chns;
	return chn;
fail:
	log_println(cfg->log, "config: channel %s init error", name);
	return NULL;
}

static struct channel *config_get_channel(struct config *cfg, const char *name,
	int extra)
{
	struct channel *chn;

	chn = config_find_channel(cfg, name);
	if(chn)
		return chn;
	else
		return config_new_channel(cfg, name, extra);
}

static int config_directive(struct config *cfg, const char *protocol_in,
	const char *port_in, int extra_in, const char *protocol_out,
	const char *port_out, int extra_out, int base, int range)
{
	struct channel *chn_in, *chn_out;
	struct ccreader *reader;
	struct ccwriter *writer;

	log_println(cfg->log, "config: %s %s %d -> %s %s %d %d %d", protocol_in,
		port_in, extra_in, protocol_out, port_out, extra_out,
		base, range);
	chn_in = config_get_channel(cfg, port_in, extra_in);
	if(chn_in == NULL)
		goto fail;
	if(chn_in->reader == NULL) {
		reader = ccreader_create(chn_in->name, protocol_in, cfg->log);
		chn_in->reader = reader;
		reader->packet.counter = cfg->counter;
	} else {
		/* FIXME: check for redefined protocol */
		reader = chn_in->reader;
	}
	chn_out = config_get_channel(cfg, port_out, extra_out);
	if(chn_out == NULL)
		goto fail;
	writer = ccwriter_create(chn_out, protocol_out, base, range);
	if(writer == NULL)
		goto fail;
	ccreader_add_writer(reader, writer);
	return 0;
fail:
	return -1;
}

static void config_skip_comments(struct config *cfg) {
	int i;
	for(i = 0; i < LINE_LENGTH; i++) {
		if(cfg->line[i] == '#')
			cfg->line[i] = '\0';
	}
}

static int config_scan_directive(struct config *cfg) {
	int i;
	char protocol_in[16], protocol_out[16];
	char port_in[16], port_out[16];
	int extra_in = 9600;	/* extra => baud rate for serial ports */
	int extra_out = 9600;
	int base = 0;
	int range = 0;

	i = sscanf(cfg->line, "%15s %15s %d %15s %15s %d %d %d", protocol_in,
		port_in, &extra_in, protocol_out, port_out, &extra_out, &base,
		&range);
	if(i == 5)
		extra_out = extra_in;
	if(i > 4)
		return config_directive(cfg, protocol_in, port_in, extra_in,
			protocol_out, port_out, extra_out, base, range);
	else if(i <= 0)
		return 0;
	else {
		log_println(cfg->log, "Invalid directive: %s", cfg->line);
		return -1;
	}
}

int config_read(struct config *cfg, const char *filename) {
	FILE *f = fopen(filename, "r");
	if(f == NULL)
		return -1;

	while(fgets(cfg->line, LINE_LENGTH, f) == cfg->line) {
		config_skip_comments(cfg);
		if(config_scan_directive(cfg))
			goto fail;
	}
	if(cfg->n_channels == 0) {
		log_println(cfg->log, "Error reading configuration file: %s",
			filename);
	}
	fclose(f);
	return cfg->n_channels;
fail:
	fclose(f);
	return -1;
}
