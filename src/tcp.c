#include <stddef.h>
#include <stdio.h>
#include <strings.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include "tcp.h"

struct channel* tcp_init(struct channel *chn, const char *name, int port) {
	struct hostent *host;
	struct sockaddr_in sa;
	int on = 1;	// turn "on" TCP_NODELAY with setsockopt

	if(channel_init(chn, name, port) == NULL)
		return NULL;

	host = gethostbyname(name);
	if(host == NULL) {
		fprintf(stderr, "Host name lookup failed: %s\n", name);
		goto fail;
	}
	chn->fd = socket(PF_INET, SOCK_STREAM, 0);
	if(chn->fd < 0) {
		fprintf(stderr, "Failed to create socket\n");
		goto fail;
	}
	if(setsockopt(chn->fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) < 0) {
		fprintf(stderr, "Failed to set TCP_NODELAY\n");
		goto fail;
	}
	sa.sin_family = AF_INET;
	bcopy(host->h_addr, &sa.sin_addr.s_addr, host->h_length);
	sa.sin_port = htons(port);
	if(connect(chn->fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		fprintf(stderr, "Connect to %s failed\n", name);
		goto fail;
	}

	return chn;
fail:
	channel_destroy(chn);
	return NULL;
}
