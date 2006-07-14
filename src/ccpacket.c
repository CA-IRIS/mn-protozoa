#include <stdio.h>
#include "ccpacket.h"

void ccpacket_init(struct ccpacket *p) {
	p->receiver = 0;
	p->status = STATUS_NONE;
	p->command = 0;
	p->pan = 0;
	p->tilt = 0;
	p->zoom = ZOOM_NONE;
	p->focus = FOCUS_NONE;
	p->iris = IRIS_NONE;
	p->aux = 0;
	p->preset = 0;
}

void ccpacket_debug(struct ccpacket *p) {
	printf("rcv: %d", p->receiver);
	if(p->status)
		printf(" status: %d", p->status);
	if(p->command)
		printf(" command: %d", p->command);
	if(p->command & CC_PAN)
		printf(" pan: %d", p->pan);
	if(p->command & CC_TILT)
		printf(" tilt: %d", p->tilt);
	if(p->zoom)
		printf(" zoom: %d", p->zoom);
	if(p->focus)
		printf(" focus: %d", p->focus);
	if(p->iris)
		printf(" iris: %d", p->iris);
	if(p->aux)
		printf(" aux: %d", p->aux);
	if(p->preset)
		printf(" preset: %d", p->preset);
	printf("\n");
}
