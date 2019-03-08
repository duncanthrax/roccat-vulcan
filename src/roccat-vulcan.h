#include <stdint.h>

#ifndef _H_ROCCAT_VULCAN
#define _H_ROCCAT_VULCAN

#define RV_VENDOR   0x1e7d
#define RV_PRODUCT  0x307a
#define RV_VENDOR_STR  "1e7d"
#define RV_PRODUCT_STR "307a"

#define RV_INPUT_DEV_DIR "/sys/class/input"
#define RV_INPUT_DEV_PREFIX "event"

#define RV_SUCCESS 0
#define RV_FAILURE -1

#define RV_NUM_KEYS 144

#define RV_MAX_STR  512

#define RV_MODE_NONE 0
#define RV_MODE_WAVE 1
#define RV_MODE_FX   2

#define RV_LOG_NORMAL  0
#define RV_LOG_VERBOSE 1

#define RV_MAX_NEIGH 10
#define RV_MAX_EV_CODE 254

#define RV_MAX_CONCURRENT_KEYS 10

typedef struct rv_rgb_type {
    int16_t r;
    int16_t g;
    int16_t b;
} rv_rgb;

typedef struct rv_rgb_map_type {
    rv_rgb key[RV_NUM_KEYS];
} rv_rgb_map;

#define RV_NUM_COLORS 10
rv_rgb rv_colors[RV_NUM_COLORS];
rv_rgb rv_color_off;

// Globals (roccat-vulcan.c)
int rv_verbose;
int rv_daemon;

// HID I/O functions (hid.c)
int rv_open_device(unsigned short vendor_id, unsigned short product_id);
int rv_wait_for_ctrl_device();
int rv_get_ctrl_report(unsigned char report_id);
int rv_set_ctrl_report(unsigned char report_id, int mode, int byteopt);
int rv_send_led_map(rv_rgb_map *map);
int rv_send_init(int type, int opt);

// Logging I/O functions (output.c)
void rv_print_buffer(unsigned char *buffer, int len);
void rv_printf(int verbose, const char *format, ...);

// Evdev
int rv_init_evdev();
int rv_update_evdev();
unsigned char rv_active_keys[RV_NUM_KEYS];
unsigned char rv_released_keys[RV_MAX_CONCURRENT_KEYS];
unsigned char rv_pressed_keys[RV_MAX_CONCURRENT_KEYS];
unsigned char rv_repeated_keys[RV_MAX_CONCURRENT_KEYS];

// FX functions (fx.c)
int rv_fx_init();
int rv_fx_impact();
void rv_led_set(unsigned char *map, unsigned char k, unsigned char r, unsigned char g, unsigned char b);
void rv_led_get(unsigned char *map, unsigned char k, unsigned char *r, unsigned char *g, unsigned char *b);

#endif
