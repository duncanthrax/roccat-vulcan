#include <stdint.h>

#ifndef _H_ROCCAT_VULCAN
#define _H_ROCCAT_VULCAN

#define RV_VENDOR   0x1e7d
#define RV_VENDOR_STR  "1e7d"

#define RV_INPUT_DEV_DIR "/sys/class/input"
#define RV_INPUT_DEV_PREFIX "event"

#define RV_SUCCESS 0
#define RV_FAILURE -1

#define RV_NUM_KEYS 144

#define RV_MAX_STR  512

#define RV_MODE_NONE 0
#define RV_MODE_WAVE 1
#define RV_MODE_FX   2
#define RV_MODE_TOPO 3

#define RV_LOG_NORMAL  0
#define RV_LOG_VERBOSE 1

#define RV_MAX_EV_CODE 0x2ff

#define RV_MAX_CONCURRENT_KEYS 10


// These ones we know about. There might be more.
#define RV_NUM_TOPO_MODELS 2
enum rv_topo_models {
    RV_TOPO_ISO,
    RV_TOPO_ANSI
};
extern int rv_topo_model;

typedef struct rv_rgb_type {
    int16_t r;
    int16_t g;
    int16_t b;
} rv_rgb;

typedef struct rv_rgb_map_type {
    rv_rgb key[RV_NUM_KEYS];
} rv_rgb_map;

#define RV_NUM_COLORS 10
extern rv_rgb rv_colors[RV_NUM_COLORS];
extern rv_rgb rv_color_off;

// Globals (roccat-vulcan.c)
extern int rv_verbose;
extern uint16_t rv_products[3];
extern char * rv_products_str[3];
extern rv_rgb* rv_fixed[RV_NUM_KEYS];

// HID I/O functions (hid.c)
int rv_open_device();
int rv_wait_for_ctrl_device();
int rv_get_ctrl_report(unsigned char report_id);
int rv_set_ctrl_report(unsigned char report_id, int mode, int byteopt);
int rv_send_led_map(rv_rgb_map *map);
int rv_send_init(int type, int opt);

// Logging I/O functions (output.c)
void rv_print_buffer(unsigned char *buffer, int len);
void rv_printf(int verbose, const char *format, ...);

// Evdev
int rv_init_evdev(int);
int rv_update_evdev();
int rv_get_keycode();
int rv_get_evdev_keypress();
const char *rv_get_ev_keyname();
extern unsigned char rv_active_keys[RV_NUM_KEYS];
extern unsigned char rv_released_keys[RV_MAX_CONCURRENT_KEYS];
extern unsigned char rv_pressed_keys[RV_MAX_CONCURRENT_KEYS];
extern unsigned char rv_repeated_keys[RV_MAX_CONCURRENT_KEYS];

// FX functions (fx.c)
int  rv_fx_init();
void rv_fx_impact();
void rv_fx_topo_rows();
void rv_fx_topo_cols();
void rv_fx_topo_keys();
void rv_fx_topo_neigh();
void rv_fx_piped(char *pipe_name);

#endif
