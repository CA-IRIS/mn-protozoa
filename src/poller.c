#include <stdio.h>	/* for printf */
#include <string.h>	/* for strerror */
#include <unistd.h>
#include <sys/errno.h>	/* for errno */
#include <sys/poll.h>

#include "sport.h"
#include "combiner.h"
#include "manchester.h"

extern int errno;

#define NPORTS 1

int main(int argc, char* argv[])
{
	struct sport port[NPORTS];
	struct pollfd pollfds[NPORTS];
	ssize_t nbytes;
	int i;
	struct combiner *cmbnr;

	if(sport_init(&port[0], "/dev/ttyS0") == NULL)
		goto fail;
	cmbnr = malloc(sizeof(struct combiner));
	if(cmbnr == NULL)
		goto fail;
	cmbnr->handler.do_read = manchester_do_read;
	cmbnr->txbuf = &port[0].txbuf;
	port[0].handler = &cmbnr->handler;

	pollfds[0].fd = port[0].fd;
	pollfds[0].events = POLLIN;
	do {
		if(poll(pollfds, NPORTS, -1) < 0)
			goto fail;
		for(i = 0; i < NPORTS; i++) {
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
