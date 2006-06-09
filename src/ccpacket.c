#include <stdio.h>
#include "ccpacket.h"

void ccpacket_init(struct ccpacket *p) {
	p->receiver = 0;
	p->preset = 0;
	p->pan = 0;
	p->tilt = 0;
	p->zoom = ZOOM_NONE;
	p->focus = FOCUS_NONE;
	p->iris = IRIS_NONE;
	p->aux = 0;
}

void ccpacket_debug(struct ccpacket *p) {
	printf("rcv: %d pan: %d tilt: %d zoom: %d focus: %d iris: %d aux: %d\n",
		p->receiver, p->pan, p->tilt, p->zoom, p->focus, p->iris,
		p->aux);
}
