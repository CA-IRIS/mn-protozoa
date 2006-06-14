#include "poller.h"

struct poller *poller_init(struct poller *p, int n_ports, struct sport *port) {
	int i;

	p->n_ports = n_ports;
	p->port = port;
	p->pollfds = malloc(sizeof(struct pollfd) * n_ports);
	if(p->pollfds == NULL)
		return NULL;
	for(i = 0; i < n_ports; i++)
		p->pollfds[i].fd = port[i].fd;
	return p;
}

static void poller_register_events(struct poller *p) {
	int i;

	for(i = 0; i < p->n_ports; i++) {
		if(buffer_is_empty(&p->port[i].txbuf))
			p->pollfds[i].events = POLLIN;
		else
			p->pollfds[i].events = POLLIN | POLLOUT;
	}
}

static int poller_do_poll(struct poller *p) {
	int i;
	ssize_t nbytes;

	if(poll(p->pollfds, p->n_ports, -1) < 0)
		return -1;
	for(i = 0; i < p->n_ports; i++) {
		if(p->pollfds[i].revents & POLLIN) {
			nbytes = sport_read(p->port + i);
			if(nbytes < 0)
				return nbytes;
		}
		if(p->pollfds[i].revents & POLLOUT) {
			nbytes = sport_write(p->port + i);
			if(nbytes < 0)
				return nbytes;
		}
	}
	return 0;
}

int poller_loop(struct poller *p) {
	int r = 0;
	do {
		poller_register_events(p);
		r = poller_do_poll(p);
	} while(r >= 0);
	return r;
}
