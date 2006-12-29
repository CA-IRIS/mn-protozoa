#include <strings.h>		/* for bcopy */
#include <netinet/tcp.h>	/* for TCP_NODELAY */
#include <netdb.h>
#include "tcp.h"

int tcp_open(struct channel *chn) {
	struct hostent *host;
	struct sockaddr_in sa;
	int on = 1;	/* turn "on" values for setsockopt */

	host = gethostbyname(chn->name);
	if(host == NULL)
		return -1;
	chn->fd = socket(PF_INET, SOCK_STREAM, 0);
	if(chn->fd < 0) {
		chn->fd = 0;
		return -1;
	}
	if(setsockopt(chn->fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) < 0)
		return -1;
	if(setsockopt(chn->fd, SOL_IP, IP_RECVERR, &on, sizeof(on)) < 0)
		return -1;
	sa.sin_family = AF_INET;
	bcopy(host->h_addr, &sa.sin_addr.s_addr, host->h_length);
	/* tcp port stored in channel->extra parameter */
	sa.sin_port = htons(chn->extra);
	if(connect(chn->fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
		return -1;
	else
		return 0;
}
