#include <stdio.h>	/* for printf */
#include <string.h>	/* for strerror */
#include <unistd.h>
#include <sys/errno.h>	/* for errno */
#include <sys/poll.h>

#include "sport.h"
#include "config.h"

extern int errno;

int main(int argc, char* argv[])
{
	ssize_t nbytes;
	int i, n_ports;
	struct sport *port;
	struct pollfd *pollfds;

	n_ports = config_read("protozoa.conf", &port);
	if(n_ports <= 0)
		goto fail;

	pollfds = malloc(sizeof(struct pollfd) * n_ports);
	if(pollfds == NULL)
		goto fail;
	for(i = 0; i < n_ports; i++) {
		printf("port: %s\n", port[i].name);
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
	printf("Error: %s\n", strerror(errno));
	return errno;
}
