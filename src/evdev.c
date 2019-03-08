
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libevdev/libevdev.h>

#include "roccat-vulcan.h"

struct libevdev *rv_evdev = NULL;

unsigned char rv_active_keys[RV_NUM_KEYS];
unsigned char rv_released_keys[RV_MAX_CONCURRENT_KEYS];
unsigned char rv_pressed_keys[RV_MAX_CONCURRENT_KEYS];
unsigned char rv_repeated_keys[RV_MAX_CONCURRENT_KEYS];

// Event key code to Vulcan key number (max code 254)
// 0xff = Not an event code that maps to a Vulcan key.
unsigned char rv_ev2rv[] = {
	0xff, 0x00, 0x06, 0x0c, 0x12, 0x18, 0x1d, 0x21, 0x31, 0x36, 0x3c, 0x42, 0x48, 0x4f, 0x57, 0x02,
	0x07, 0x0d, 0x13, 0x19, 0x1e, 0x22, 0x32, 0x37, 0x3d, 0x43, 0x49, 0x50, 0x58, 0x05, 0x08, 0x0e,
	0x14, 0x1a, 0x1f, 0x23, 0x33, 0x38, 0x3e, 0x44, 0x4a, 0x01, 0x04, 0x60, 0x0f, 0x15, 0x1b, 0x20,
	0x24, 0x34, 0x39, 0x3f, 0x45, 0x4b, 0x52, 0x7c, 0x10, 0x25, 0x03, 0x0b, 0x11, 0x17, 0x1c, 0x30,
	0x35, 0x3b, 0x41, 0x4e, 0x54, 0x71, 0x67, 0x72, 0x78, 0x7d, 0x81, 0x73, 0x79, 0x7e, 0x82, 0x74,
	0x7a, 0x7f, 0x75, 0x80, 0xff, 0xff, 0x09, 0x55, 0x56, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x83, 0x59, 0x77, 0x63, 0x46, 0xff, 0x68, 0x6a, 0x6d, 0x66, 0x6f, 0x69, 0x6b, 0x6e, 0x64, 0x65,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x6c, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0a, 0xff, 0x53,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

#define RV_MAX_CMP_CHARS 4
int cmp_file(char *fpath, char *string) {
	char data[RV_MAX_CMP_CHARS];
	int fd, i;
	fd = open(fpath, O_RDONLY);
	if (!fd) return RV_FAILURE;
	i = read(fd, data, RV_MAX_CMP_CHARS);
	close(fd);
	if (i < strlen(string)) return RV_FAILURE;
	return strncmp(string, data, strlen(string));
}

int prefix_filter(const struct dirent *entry) {
	if (strncmp(entry->d_name, RV_INPUT_DEV_PREFIX, strlen(RV_INPUT_DEV_PREFIX)) == 0)
		return 1;
	return 0;
}

int rv_init_evdev() {
	struct dirent **event_dev_list;
	int evdev_fd, num_devs, i;
	char fpath[RV_MAX_STR+1];
	char event_dev[RV_MAX_STR+1];

	event_dev[0] = '\0';

	memset(rv_active_keys, 0x00, RV_NUM_KEYS);
	memset(rv_released_keys, 0xff, RV_MAX_CONCURRENT_KEYS);
	memset(rv_pressed_keys,  0xff, RV_MAX_CONCURRENT_KEYS);
	memset(rv_repeated_keys, 0xff, RV_MAX_CONCURRENT_KEYS);

	num_devs = scandir(RV_INPUT_DEV_DIR, &event_dev_list, prefix_filter, NULL);

	if (num_devs >= 0) {
		for (i = 0; i < num_devs; i++) {
			snprintf(fpath, RV_MAX_STR, "/sys/class/input/%s/device/id/product", event_dev_list[i]->d_name);
			if (cmp_file(fpath, RV_PRODUCT_STR) == RV_SUCCESS) {
				snprintf(fpath, RV_MAX_STR, "/sys/class/input/%s/device/id/vendor", event_dev_list[i]->d_name);
				if (cmp_file(fpath, RV_VENDOR_STR) == RV_SUCCESS) {
					snprintf(fpath, RV_MAX_STR, "/sys/class/input/%s/device/capabilities/led", event_dev_list[i]->d_name);
					if (cmp_file(fpath, "1f") == RV_SUCCESS) {
						snprintf(event_dev, RV_MAX_STR, "/dev/input/%s", event_dev_list[i]->d_name);
					}
				}
			}
			free(event_dev_list[i]);
		}
		free(event_dev_list);
	}
	else {
		rv_printf(RV_LOG_VERBOSE, "Error: No event input devices in %s\n", RV_INPUT_DEV_DIR);
		return(RV_FAILURE);
	}

	if (!event_dev[0]) {
		rv_printf(RV_LOG_VERBOSE, "Error: No event input device found\n");
		return(RV_FAILURE);
	}

	rv_printf(RV_LOG_VERBOSE, "Using event input device %s\n", event_dev);

	evdev_fd = open(event_dev, O_RDONLY|O_NONBLOCK);
	if (libevdev_new_from_fd(evdev_fd, &rv_evdev) < 0) return(RV_FAILURE);

	return RV_SUCCESS;
}

int rv_update_evdev() {
	struct input_event ev;
	int rc;
	int changes = 0;
	int num_released_keys = 0;
	int num_pressed_keys  = 0;
	int num_repeated_keys = 0;

	memset(rv_released_keys, 0xff, RV_MAX_CONCURRENT_KEYS);
	memset(rv_pressed_keys,  0xff, RV_MAX_CONCURRENT_KEYS);
	memset(rv_repeated_keys, 0xff, RV_MAX_CONCURRENT_KEYS);

	do {
		rc = libevdev_next_event(rv_evdev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
		if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
			if (ev.type == EV_KEY && ev.code <= RV_MAX_EV_CODE) {
				int k,i,j;
				int rv_code = rv_ev2rv[ev.code];

				rv_printf(RV_LOG_VERBOSE, "Event: %s(%u) %s(0x%02hhx) VAL(%d)\n",
				    libevdev_event_type_get_name(ev.type),ev.type,
				    libevdev_event_code_get_name(ev.type, ev.code), ev.code, ev.value);

				if (rv_code == 0xff) continue;

				switch (ev.value) {
					case 0:
						// Key released
						rv_active_keys[rv_code] = 0x00;
						if (num_released_keys < RV_MAX_CONCURRENT_KEYS) {
							rv_released_keys[num_released_keys++] = rv_code;
							changes++;
						}
					break;
					case 1:
						// Key pressed
						if (num_pressed_keys < RV_MAX_CONCURRENT_KEYS) {
							rv_pressed_keys[num_pressed_keys++] = rv_code;
							rv_active_keys[rv_code] = 0x01;
							changes++;
						}
					break;
					case 2:
						// Key on repeat
						if (num_repeated_keys < RV_MAX_CONCURRENT_KEYS) {
							rv_repeated_keys[num_repeated_keys++] = rv_code;
							changes++;
						}
					break;
				}
			}
		}
	}
	while (rc >= 0);

	return changes;
}