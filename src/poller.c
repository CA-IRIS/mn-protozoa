#include <stdio.h>	/* for printf */
#include <string.h>	/* for strerror */
#include <unistd.h>
#include <sys/errno.h>	/* for errno */
#include <sys/poll.h>

#include "sport.h"
#include "config.h"

static const char *CONF_FILE = "protozoa.conf";

extern int errno;

int main(int argc, char* argv[])
{
	ssize_t nbytes;
	int i, n_ports;
	struct sport *port;
	struct pollfd *pollfds;

	n_ports = config_read(CONF_FILE, &port);
	if(n_ports <= 0) {
		printf("Error reading configuration file: %s\n", CONF_FILE);
		goto fail;
	}

	pollfds = malloc(sizeof(struct pollfd) * n_ports);
	if(pollfds == NULL)
		goto fail;
	for(i = 0; i < n_ports; i++) {
		pollfds[i].fd = port[i].fd;
		pollfds[i].events = POLLIN;
	}

	do {
		if(poll(pollfds, n_ports, -1) < 0)
			goto fail;
		for(i = 0; i < n_ports; i++) {
			if(pollfds[i].revents & POLLIN) {
				nbytes = sport_read(port + i);
				if(nbytes < 0)
					goto fail;
			}
			if(pollfds[i].revents & POLLOUT) {
				nbytes = sport_write(port + i);
				if(nbytes < 0)
					goto fail;
			}
		}
	} while(1);
fail:
	if(errno)
		printf("Error: %s\n", strerror(errno));
	return errno;
}
