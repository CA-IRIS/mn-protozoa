#ifndef __CCPACKET_H__
#define __CCPACKET_H__

#include "log.h"
#include "timeval.h"

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
	CC_PAN = (CC_PAN_LEFT | CC_PAN_RIGHT),
	CC_TILT_UP = 1 << 2,
	CC_TILT_DOWN = 1 << 3,
	CC_TILT = (CC_TILT_UP | CC_TILT_DOWN),
	CC_PAN_TILT = (CC_PAN | CC_TILT),
	CC_PRESET_NONE = 0,
	CC_PRESET_RECALL = 1 << 4,
	CC_PRESET_STORE = 1 << 5,
	CC_PRESET_CLEAR = 1 << 6,
	CC_PRESET = CC_PRESET_RECALL | CC_PRESET_STORE | CC_PRESET_CLEAR,
	CC_AUTO_PAN = 1 << 8,
	CC_MANUAL_PAN = 1 << 9,
	CC_ACK_ALARM = 1 << 11,
	CC_CAMERA_ON = 1 << 12,
	CC_CAMERA_OFF = 1 << 13,
	CC_MENU_OPEN = 1 << 14,
	CC_MENU_ENTER = 1 << 15,
	CC_MENU_CANCEL = 1 << 16,
};

enum lens_t {
	CC_ZOOM_STOP = 1 << 0,
	CC_ZOOM_IN = 1 << 1,
	CC_ZOOM_OUT = 1 << 2,
	CC_ZOOM = CC_ZOOM_STOP | CC_ZOOM_IN | CC_ZOOM_OUT,
	CC_FOCUS_STOP = 1 << 3,
	CC_FOCUS_NEAR = 1 << 4,
	CC_FOCUS_FAR = 1 << 5,
	CC_FOCUS_AUTO = 1 << 6,
	CC_FOCUS = CC_FOCUS_STOP | CC_FOCUS_NEAR | CC_FOCUS_FAR | CC_FOCUS_AUTO,
	CC_IRIS_STOP = 1 << 7,
	CC_IRIS_CLOSE = 1 << 8,
	CC_IRIS_OPEN = 1 << 9,
	CC_IRIS_AUTO = 1 << 10,
	CC_IRIS = CC_IRIS_STOP | CC_IRIS_CLOSE | CC_IRIS_OPEN | CC_IRIS_AUTO,
	CC_LENS_NONE = 1 << 11,
	CC_LENS_SPEED = 1 << 12,
	CC_LENS = CC_LENS_NONE | CC_LENS_SPEED,
};

#define SPEED_MAX ((1 << 11) - 1)

enum aux_t {
	AUX_NONE = 0,
	AUX_1 = 1 << 0,
	AUX_2 = 1 << 1,
	AUX_3 = 1 << 2,
	AUX_4 = 1 << 3,
	AUX_5 = 1 << 4,
	AUX_6 = 1 << 5,
	AUX_7 = 1 << 6,
	AUX_8 = 1 << 7,
	AUX_CLEAR = 1 << 8,
};

/*
 * A camera control packet is a protocol-neutral representation of a single
 * message to a camera receiver driver.
 */
struct ccpacket {
	int		receiver;	/* receiver address: 1 to 1024 */
	enum status_t	status;		/* status request type */
	enum command_t	command;	/* bitmask of commands */
	int		pan;		/* 0 (none) to SPEED_MAX (fast) */
	int		tilt;		/* 0 (none) to SPEED_MAX (fast) */
	enum lens_t	lens;		/* bitmask of lens functions */
	enum aux_t	aux;		/* bitmask of aux functions */
	int		preset;		/* preset number */
	struct timeval	expire;		/* expiration time */
};

void ccpacket_init(struct ccpacket *pkt);
void ccpacket_clear(struct ccpacket *pkt);
void ccpacket_set_receiver(struct ccpacket *self, int receiver);
int ccpacket_get_receiver(const struct ccpacket *self);
void ccpacket_set_status(struct ccpacket *self, enum status_t s);
enum status_t ccpacket_get_status(const struct ccpacket *self);
void ccpacket_set_pan_speed(struct ccpacket *self, int speed);
int ccpacket_get_pan_speed(const struct ccpacket *self);
bool ccpacket_has_pan(const struct ccpacket *self);
void ccpacket_set_tilt_speed(struct ccpacket *self, int speed);
int ccpacket_get_tilt_speed(const struct ccpacket *self);
bool ccpacket_has_tilt(const struct ccpacket *self);
void ccpacket_set_timeout(struct ccpacket *pkt, unsigned int timeout);
bool ccpacket_is_expired(struct ccpacket *self, unsigned int timeout);
void ccpacket_set_preset(struct ccpacket *self, enum command_t pm, int p_num);
enum command_t ccpacket_get_preset_mode(const struct ccpacket *self);
int ccpacket_get_preset_number(const struct ccpacket *self);
bool ccpacket_is_stop(struct ccpacket *pkt);
void ccpacket_set_zoom(struct ccpacket *self, enum lens_t zm);
enum lens_t ccpacket_get_zoom(const struct ccpacket *self);
void ccpacket_set_focus(struct ccpacket *self, enum lens_t fm);
enum lens_t ccpacket_get_focus(const struct ccpacket *self);
void ccpacket_set_iris(struct ccpacket *self, enum lens_t im);
enum lens_t ccpacket_get_iris(const struct ccpacket *self);
void ccpacket_set_lens(struct ccpacket *self, enum lens_t lm);
enum lens_t ccpacket_get_lens(const struct ccpacket *self);
bool ccpacket_has_command(const struct ccpacket *pkt);
bool ccpacket_has_aux(struct ccpacket *pkt);
bool ccpacket_has_autopan(const struct ccpacket *pkt);
bool ccpacket_has_power(const struct ccpacket *pkt);
void ccpacket_log(struct ccpacket *pkt, struct log *log, const char *dir,
	const char *name);
void ccpacket_count(struct ccpacket *pkt);
void ccpacket_drop(struct ccpacket *pkt);
void ccpacket_copy(struct ccpacket *dest, struct ccpacket *src);

#endif
