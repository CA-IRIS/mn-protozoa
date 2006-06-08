#include "ccpacket.h"

void ccpacket_init(struct ccpacket *p) {
	p->receiver = 0;
	p->pan = 0;
	p->tilt = 0;
	p->zoom = ZOOM_NONE;
	p->focus = FOCUS_NONE;
	p->iris = IRIS_NONE;
	p->aux = 0;
}
