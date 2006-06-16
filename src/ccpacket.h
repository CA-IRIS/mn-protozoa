#ifndef __CCPACKET_H__
#define __CCPACKET_H__

enum status_t {
	STATUS_NONE = 0,
	STATUS_REQUEST = 1 << 0,
	STATUS_SECTOR = 1 << 1,
	STATUS_PRESET = 1 << 2,
	STATUS_AUX_SET_2 = 1 << 3,
	STATUS_EXTENDED = (STATUS_SECTOR | STATUS_PRESET | STATUS_AUX_SET_2),
};

enum command_t {
	CC_PAN_LEFT = 1 << 0,
	CC_PAN_RIGHT = 1 << 1,
	CC_TILT_UP = 1 << 2,
	CC_TILT_DOWN = 1 << 3,
	CC_RECALL = 1 << 4,
	CC_STORE = 1 << 5,
	CC_AUTO_IRIS = 1 << 6,
	CC_AUTO_PAN = 1 << 7,
	CC_LENS_SPEED = 1 << 8,
	CC_ACK_ALARM = 1 << 9,
};

#define SPEED_MAX ((1 << 11) - 1)

enum zoom_t {
	ZOOM_OUT = -1,
	ZOOM_NONE = 0,
	ZOOM_IN = 1,
};

enum focus_t {
	FOCUS_NEAR = -1,
	FOCUS_NONE = 0,
	FOCUS_FAR = 1,
};

enum iris_t {
	IRIS_CLOSE = -1,
	IRIS_NONE = 0,
	IRIS_OPEN = 1,
};

enum aux_t {
	AUX_NONE = 0,
	AUX_1 = 1 << 0,
	AUX_2 = 1 << 1,
	AUX_3 = 1 << 2,
	AUX_4 = 1 << 3,
	AUX_5 = 1 << 4,
	AUX_6 = 1 << 5,
};

/*
 * A camera control packet is a protocol-neutral representation of a single
 * message to a camera receiver driver.
 */
struct ccpacket {
	int	receiver;	/* receiver address: 1 to 255 */
	enum status_t	status;	/* status request type */
	enum command_t	command;/* bitmask of commands */
	int		pan;	/* 0 (none) to 4095 (fast) */
	int		tilt;	/* 0 (none) to 4095 (fast) */
	enum zoom_t	zoom;	/* -1 (out), 0 (no change), or 1 (in) */
	enum focus_t	focus;	/* -1 (near), 0 (no change), or 1 (far) */
	enum iris_t	iris;	/* -1 (close), 0 (no change), or 1 (open) */
	enum aux_t	aux;	/* bitmask of aux functions */
	int		preset;	/* preset number */
};

void ccpacket_init(struct ccpacket *p);
void ccpacket_debug(struct ccpacket *p);

#endif
