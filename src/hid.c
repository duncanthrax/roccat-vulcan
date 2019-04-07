#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <linux/hidraw.h>
#include <libudev.h>
#include <sys/ioctl.h>

#include "hidapi/hidapi.h"

#include "roccat-vulcan.h"

#define RV_CTRL_INTERFACE 1
#define RV_LED_INTERFACE  3

hid_device *led_device;
int ctrl_device;

void rv_close_ctrl_device() {
	if (ctrl_device) close(ctrl_device);
	ctrl_device = 0;
}

int hidraw_get_feature_report(int fd, unsigned char *buf, int size) {
	int res = ioctl(fd, HIDIOCGFEATURE(size), buf);
	if (res < 0) {
		return 0;
	}

	return size;
}

int hidraw_send_feature_report(int fd, unsigned char *buf, int size) {
	int res = ioctl(fd, HIDIOCSFEATURE(size), buf);
	if (res < 0) {
		return 0;
	}

	return size;
}

int rv_open_device() {

	// Loop through product IDs.
	int p = 0;
	while (rv_products[p]) {
		unsigned short product_id = rv_products[p];

		// For LED device, use hidapi-libusb, since we need to disconnect it
		// from the default kernel driver.

		struct hid_device_info *dev, *devs = NULL;
		led_device = NULL;
		devs = hid_enumerate(RV_VENDOR, product_id);
		dev = devs;
		while (dev) {
			if (dev->interface_number == RV_LED_INTERFACE) {
				rv_printf(RV_LOG_NORMAL, "open_device(%04hx, %04hx): LED interface at USB path %s\n", RV_VENDOR, product_id, dev->path);
				led_device = hid_open_path(dev->path);
				if (!led_device) {
					rv_printf(RV_LOG_VERBOSE, "open_device(%04hx, %04hx): Unable to open LED interface %s\n", RV_VENDOR, product_id, dev->path);
					goto NEXT_PRODUCT;
				}
				if (hid_set_nonblocking(led_device, 1) < 0) {
					rv_printf(RV_LOG_VERBOSE, "open_device(%04hx, %04hx): Unable to set LED interface %s to non-blocking mode\n", RV_VENDOR, product_id, dev->path);
					goto NEXT_PRODUCT;
				};
			}
			else {
				rv_printf(RV_LOG_VERBOSE, "open_device(%04hx, %04hx): ignoring non-LED interface #%d\n", RV_VENDOR, product_id, dev->interface_number);
			}
			dev = dev->next;
		}

		if (!led_device) {
			rv_printf(RV_LOG_VERBOSE, "open_device(%04hx, %04hx): No LED device found\n", RV_VENDOR, product_id);
			goto NEXT_PRODUCT;
		}

		if (devs) { hid_free_enumeration(devs); devs = NULL; };

		// For CTRL device, use native HIDRAW access. After sending the init
		// sequence, we will close it.

		struct udev *udev = udev_new();
		struct udev_enumerate *enumerate = udev_enumerate_new(udev);
		udev_enumerate_add_match_subsystem(enumerate, "hidraw");
		udev_enumerate_scan_devices(enumerate);

		struct udev_list_entry *entries = udev_enumerate_get_list_entry(enumerate);
		struct udev_list_entry *cur;
		udev_list_entry_foreach(cur, entries) {
			struct udev_device *usb_dev = NULL;
			struct udev_device *raw_dev = NULL;
			const char *sysfs_path = udev_list_entry_get_name(cur);
			if (!sysfs_path) goto NEXT_ENTRY;

			raw_dev = udev_device_new_from_syspath(udev, sysfs_path);
			const char *dev_path = udev_device_get_devnode(raw_dev);
			if (!dev_path) goto NEXT_ENTRY;

			usb_dev = udev_device_get_parent_with_subsystem_devtype(
				raw_dev,
				"usb",
				"usb_interface");
			if (!usb_dev) goto NEXT_ENTRY;

			const char *info = udev_device_get_sysattr_value(usb_dev, "uevent");
			if (!info) goto NEXT_ENTRY;

			const char *itf = udev_device_get_sysattr_value(usb_dev, "bInterfaceNumber");
			if (!itf) goto NEXT_ENTRY;

			// We're looking for vid/pid and interface number
			if (atoi(itf) == RV_CTRL_INTERFACE) {
				char searchstr[64];
				snprintf(searchstr, 64, "PRODUCT=%hx/%hx", RV_VENDOR, product_id);

				if (strstr(info, searchstr) != NULL) {
					ctrl_device = open(dev_path, O_RDWR|O_NONBLOCK);
					if (!ctrl_device) {
						rv_printf(RV_LOG_VERBOSE, "open_device(%04hx, %04hx): Unable to open CTRL device at %s\n", RV_VENDOR, product_id, dev_path);
						goto NEXT_ENTRY;
					}
					rv_printf(RV_LOG_NORMAL, "open_device(%04hx, %04hx): CTRL interface at %s\n", RV_VENDOR, product_id, dev_path);
					break;
				}
			}

			NEXT_ENTRY:
			if (raw_dev) udev_device_unref(raw_dev);
		}

		if (!ctrl_device) {
			rv_printf(RV_LOG_VERBOSE, "open_device(%04hx, %04hx): No CTRL device found\n", RV_VENDOR, product_id);
			goto NEXT_PRODUCT;
		}

		return 0;

		NEXT_PRODUCT:
		if (devs) { hid_free_enumeration(devs); devs = NULL; };
		if (led_device) hid_close(led_device);
		p++;
	}

	return -1;
}

int rv_wait_for_ctrl_device() {
	unsigned char buffer[] = { 0x04, 0x00, 0x00, 0x00 };
	int res;

	while (1) {
		// 150ms is the magic number here, should suffice on first try.
		usleep(150000);

		res = hidraw_get_feature_report(ctrl_device, buffer, sizeof(buffer));
		if (res) {
			rv_printf(RV_LOG_VERBOSE, "rv_wait_for_ctrl_device(): ");
			rv_print_buffer(buffer, res);
			if (buffer[1] == 0x01) break;
		}
		else {
			rv_printf(RV_LOG_VERBOSE, "rv_wait_for_ctrl_device() failed\n");
			return RV_FAILURE;
		}
	}

	return 0;
}

int rv_get_ctrl_report(unsigned char report_id) {
	int res;
	unsigned char *buffer = NULL;
	int length = 0;

	switch(report_id) {
		case 0x0f:
			length = 8;
		break;
	}

	if (!length) {
		rv_printf(RV_LOG_VERBOSE, "[Fatal] rv_get_ctrl_report(%02hhx): No such report_id %02hhx\n", report_id, report_id);
		exit(RV_FAILURE);
	}

	buffer = malloc(length);
	if (!buffer) {
		rv_printf(RV_LOG_VERBOSE, "[Fatal] rv_get_ctrl_report(%02hhx): malloc failed\n", report_id);
		exit(RV_FAILURE);
	}

	buffer[0] = report_id;
	res = hidraw_get_feature_report(ctrl_device, buffer, length);
	if (res) {
		rv_printf(RV_LOG_VERBOSE, "rv_get_ctrl_report(%02hhx): ", report_id);
		rv_print_buffer(buffer, res);
		free(buffer);
		return 0;
	}
	else {
		rv_printf(RV_LOG_VERBOSE, "rv_get_ctrl_report(%02hhx) failed\n", report_id);
		free(buffer);
		return RV_FAILURE;
	}
}


int rv_set_ctrl_report(unsigned char report_id, int mode, int byteopt) {
	int res;
	unsigned char *buffer = NULL;
	int length = 0;
	int malloced = 0;

	switch(report_id) {
		case 0x15:
			buffer = (unsigned char *)"\x15\x00\x01";
			length = 3;
		break;
		case 0x05:
			buffer = (unsigned char *)"\x05\x04\x00\x04";
			length = 4;
		break;
		case 0x07:
			buffer = (unsigned char *)"\x07\x5f\x00\x3a\x00\x00\x3b\x00\x00\x3c\x00\x00\x3d\x00\x00\x3e" \
						"\x00\x00\x3f\x00\x00\x40\x00\x00\x41\x00\x00\x42\x00\x00\x43\x00" \
						"\x00\x44\x00\x00\x45\x00\x00\x46\x00\x00\x47\x00\x00\x48\x00\x00" \
						"\xb3\x00\x00\xb4\x00\x00\xb5\x00\x00\xb6\x00\x00\xc2\x00\x00\xc3" \
						"\x00\x00\xc0\x00\x00\xc1\x00\x00\xce\x00\x00\xcf\x00\x00\xcc\x00" \
						"\x00\xcd\x00\x00\x46\x00\x00\xfc\x00\x00\x48\x00\x00\xcd\x0e";
			length = 95;
		break;
		case 0x0a:
			buffer = (unsigned char *)"\x0a\x08\x00\xff\xf1\x00\x02\x02";
			length = 8;
		break;
		case 0x0b:
			buffer = (unsigned char *)"\x0b\x41\x00\x1e\x00\x00\x1f\x00\x00\x20\x00\x00\x21\x00\x00\x22" \
						"\x00\x00\x14\x00\x00\x1a\x00\x00\x08\x00\x00\x15\x00\x00\x17\x00" \
						"\x00\x04\x00\x00\x16\x00\x00\x07\x00\x00\x09\x00\x00\x0a\x00\x00" \
						"\x1d\x00\x00\x1b\x00\x00\x06\x00\x00\x19\x00\x00\x05\x00\x00\xde\x01";
			length = 65;
		break;
		case 0x06:
			buffer = (unsigned char *)"\x06\x85\x00\x3a\x29\x35\x1e\x2b\x39\xe1\xe0\x3b\x1f\x14\x1a\x04" \
						"\x64\x00\x00\x3d\x3c\x20\x21\x08\x16\x1d\xe2\x3e\x23\x22\x15\x07" \
						"\x1b\x06\x8b\x3f\x24\x00\x17\x0a\x09\x19\x91\x40\x41\x00\x1c\x18" \
						"\x0b\x05\x2c\x42\x26\x25\x0c\x0d\x0e\x10\x11\x43\x2a\x27\x2d\x12" \
						"\x0f\x36\x8a\x44\x45\x89\x2e\x13\x33\x37\x90\x46\x49\x4c\x2f\x30" \
						"\x34\x38\x88\x47\x4a\x4d\x31\x32\x00\x87\xe6\x48\x4b\x4e\x28\x52" \
						"\x50\xe5\xe7\xd2\x53\x5f\x5c\x59\x51\x00\xf1\xd1\x54\x60\x5d\x5a" \
						"\x4f\x8e\x65\xd0\x55\x61\x5e\x5b\x62\xa4\xe4\xfc\x56\x57\x85\x58" \
						"\x63\x00\x00\xc2\x24";
			length = 133;
		break;
		case 0x09:
			buffer = (unsigned char *)"\x09\x2b\x00\x49\x00\x00\x4a\x00\x00\x4b\x00\x00\x4c\x00\x00\x4d" \
						"\x00\x00\x4e\x00\x00\xa4\x00\x00\x8e\x00\x00\xd0\x00\x00\xd1\x00" \
						"\x00\x00\x00\x00\x01\x00\x00\x00\x00\xcd\x04";
			length = 43;
		break;
		case 0x0d:
			length = 443;

			if (mode == RV_MODE_WAVE) {
				buffer = malloc(length);
				if (!buffer) {
					rv_printf(RV_LOG_VERBOSE, "rv_set_ctrl_report(%02hhx): malloc failed\n", report_id);
					exit(RV_FAILURE);
				}
				malloced = 1;
				memcpy((char *)buffer,"\x0d\xbb\x01\x00\x0a\x04\x05\x45\x83\xca\xca\xca\xca\xca\xca\xce" \
							"\xce\xd2\xce\xce\xd2\x19\x19\x19\x19\x19\x19\x23\x23\x2d\x23\x23" \
							"\x2d\xe0\xe0\xe0\xe0\xe0\xe0\xe3\xe3\xe6\xe3\xe3\xe6\xd2\xd2\xd5" \
							"\xd2\xd2\xd5\xd5\xd5\xd9\xd5\x00\xd9\x2d\x2d\x36\x2d\x2d\x36\x36" \
							"\x36\x40\x36\x00\x40\xe6\xe6\xe9\xe6\xe6\xe9\xe9\xe9\xec\xe9\x00" \
							"\xec\xd9\xd9\xdd\xd9\xdd\xdd\xe0\xe0\xdd\xe0\xe4\xe4\x40\x40\x4a" \
							"\x40\x4a\x4a\x53\x53\x4a\x53\x5d\x5d\xec\xec\xef\xec\xef\xef\xf2" \
							"\xf2\xef\xf2\xf5\xf5\xe4\xe4\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
							"\x00\x5d\x5d\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf5\xf5\x00" \
							"\x00\x00\x00\x00\x00\x00\x00\x00\x00\xe4\xe4\xe8\xe8\xe8\xe8\xe8" \
							"\xeb\xeb\xeb\x00\xeb\x5d\x5d\x67\x67\x67\x67\x67\x70\x70\x70\x00" \
							"\x70\xf5\xf5\xf8\xf8\xf8\xf8\xf8\xfb\xfb\xfb\x00\xfb\xeb\xef\xef" \
							"\xef\x00\xef\xf0\xf0\xed\xf0\xf0\x00\x70\x7a\x7a\x7a\x00\x7a\x7a" \
							"\x7a\x6f\x7a\x7a\x00\xfb\xfd\xfd\xfd\x00\xfd\xf8\xf8\xea\xf8\xf8" \
							"\x00\xed\xed\xea\xed\xed\x00\xed\xea\xea\xf6\xe7\xea\x6f\x6f\x65" \
							"\x6f\x6f\x00\x6f\x65\x65\x66\x5a\x65\xea\xea\xdc\xea\xea\x00\xea" \
							"\xdc\xdc\x00\xce\xdc\xea\xe7\xe5\xe7\xe5\xe5\x00\x00\x00\x00\x00" \
							"\x00\x65\x5a\x50\x5a\x50\x50\x00\x00\x00\x00\x00\x00\xdc\xce\xc0" \
							"\xce\xc0\xc0\x00\x00\x00\x00\x00\x00\xe7\x00\x00\xe2\xe2\xe2\xe2" \
							"\xdf\xdf\xdf\xdf\xdf\x5a\x00\x00\x45\x45\x45\x45\x3b\x3b\x3b\x3b" \
							"\x3b\xce\x00\x00\xb2\xb2\xb2\xb2\xa4\xa4\xa4\xa4\xa4\xdc\xdc\xdc" \
							"\xdc\x00\xda\xda\xda\xda\xda\x00\xd7\x30\x30\x30\x30\x00\x26\x26" \
							"\x26\x26\x26\x00\x1c\x96\x96\x96\x96\x00\x88\x88\x88\x88\x88\x00" \
							"\x7a\xd7\xd7\xd7\x00\xd4\xd4\xd4\xd4\xd4\xd1\xd1\xd1\x1c\x1c\x1c" \
							"\x00\x11\x11\x11\x11\x11\x06\x06\x06\x7a\x7a\x7a\x00\x6c\x6c\x6c" \
							"\x6c\x6c\x5e\x5e\x5e\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
							"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
							"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x21\xcf", length);
				// byte 5   == 01-slow 06-med 0b-fast
				// byte 441 == 1e-slow 23-med 28-fast
				if (byteopt < 1)  byteopt = 1;
				if (byteopt > 11) byteopt = 11;
				buffer[5]   = (unsigned char)(byteopt);
				buffer[441] = (unsigned char)(byteopt + 29);
			}
			else {
				buffer = (unsigned char *)"\x0d\xbb\x01\x00\x06\x0b\x05\x45\x83\xca\xca\xca\xca\xca\xca\xce" \
							"\xce\xd2\xce\xce\xd2\x19\x19\x19\x19\x19\x19\x23\x23\x2d\x23\x23" \
							"\x2d\xe0\xe0\xe0\xe0\xe0\xe0\xe3\xe3\xe6\xe3\xe3\xe6\xd2\xd2\xd5" \
							"\xd2\xd2\xd5\xd5\xd5\xd9\xd5\x00\xd9\x2d\x2d\x36\x2d\x2d\x36\x36" \
							"\x36\x40\x36\x00\x40\xe6\xe6\xe9\xe6\xe6\xe9\xe9\xe9\xec\xe9\x00" \
							"\xec\xd9\xd9\xdd\xd9\xdd\xdd\xe0\xe0\xdd\xe0\xe4\xe4\x40\x40\x4a" \
							"\x40\x4a\x4a\x53\x53\x4a\x53\x5d\x5d\xec\xec\xef\xec\xef\xef\xf2" \
							"\xf2\xef\xf2\xf5\xf5\xe4\xe4\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
							"\x00\x5d\x5d\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf5\xf5\x00" \
							"\x00\x00\x00\x00\x00\x00\x00\x00\x00\xe4\xe4\xe8\xe8\xe8\xe8\xe8" \
							"\xeb\xeb\xeb\x00\xeb\x5d\x5d\x67\x67\x67\x67\x67\x70\x70\x70\x00" \
							"\x70\xf5\xf5\xf8\xf8\xf8\xf8\xf8\xfb\xfb\xfb\x00\xfb\xeb\xef\xef" \
							"\xef\x00\xef\xf0\xf0\xed\xf0\xf0\x00\x70\x7a\x7a\x7a\x00\x7a\x7a" \
							"\x7a\x6f\x7a\x7a\x00\xfb\xfd\xfd\xfd\x00\xfd\xf8\xf8\xea\xf8\xf8" \
							"\x00\xed\xed\xea\xed\xed\x00\xed\xea\xea\xf6\xe7\xea\x6f\x6f\x65" \
							"\x6f\x6f\x00\x6f\x65\x65\x66\x5a\x65\xea\xea\xdc\xea\xea\x00\xea" \
							"\xdc\xdc\x00\xce\xdc\xea\xe7\xe5\xe7\xe5\xe5\x00\x00\x00\x00\x00" \
							"\x00\x65\x5a\x50\x5a\x50\x50\x00\x00\x00\x00\x00\x00\xdc\xce\xc0" \
							"\xce\xc0\xc0\x00\x00\x00\x00\x00\x00\xe7\x00\x00\xe2\xe2\xe2\xe2" \
							"\xdf\xdf\xdf\xdf\xdf\x5a\x00\x00\x45\x45\x45\x45\x3b\x3b\x3b\x3b" \
							"\x3b\xce\x00\x00\xb2\xb2\xb2\xb2\xa4\xa4\xa4\xa4\xa4\xdc\xdc\xdc" \
							"\xdc\x00\xda\xda\xda\xda\xda\x00\xd7\x30\x30\x30\x30\x00\x26\x26" \
							"\x26\x26\x26\x00\x1c\x96\x96\x96\x96\x00\x88\x88\x88\x88\x88\x00" \
							"\x7a\xd7\xd7\xd7\x00\xd4\xd4\xd4\xd4\xd4\xd1\xd1\xd1\x1c\x1c\x1c" \
							"\x00\x11\x11\x11\x11\x11\x06\x06\x06\x7a\x7a\x7a\x00\x6c\x6c\x6c" \
							"\x6c\x6c\x5e\x5e\x5e\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
							"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
							"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x24\xcf";
			}
		break;
		case 0x13:
			if (mode == RV_MODE_WAVE)
				buffer = (unsigned char *)"\x13\x08\x00\x00\x00\x00\x00\x00";
			else
				buffer = (unsigned char *)"\x13\x08\x01\x00\x00\x00\x00\x00";
			length = 8;
		break;
	}

	if (!buffer) {
		rv_printf(RV_LOG_VERBOSE, "[Fatal] rv_set_ctrl_report(%02hhx): No such report_id %02hhx\n", report_id, report_id);
		exit(RV_FAILURE);
	}

	res = hidraw_send_feature_report(ctrl_device, buffer, length);
	if (malloced) free(buffer);
	if (res == length) {
		rv_printf(RV_LOG_VERBOSE, "rv_set_ctrl_report(%02hhx): %u bytes sent\n", report_id, res);
		return RV_SUCCESS;
	}
	else {
		rv_printf(RV_LOG_VERBOSE, "rv_set_ctrl_report(%02hhx) failed\n", report_id);
		return RV_FAILURE;
	}
}

int rv_send_led_map(rv_rgb_map *src) {
	int i, k;
	rv_rgb rgb;
	// Send seven chunks with 64 bytes each
	unsigned char hwmap[444];
	// Plus one byte report ID for the lib
	unsigned char workbuf[65];

	memset(hwmap, 0, sizeof(hwmap));
	// Translate linear to hardware map
	for (k = 0; k < RV_NUM_KEYS; k++) {
		rgb = rv_fixed[k] ? *(rv_fixed[k]) : (src ? src->key[k] : rv_color_off);

		rgb.r = (rgb.r > 255) ? 255 : (rgb.r < 0) ? 0 : rgb.r;
		rgb.g = (rgb.g > 255) ? 255 : (rgb.g < 0) ? 0 : rgb.g;
		rgb.b = (rgb.b > 255) ? 255 : (rgb.b < 0) ? 0 : rgb.b;

		int offset = ((k / 12) * 36) + (k % 12);
		hwmap[offset + 0 ] = (unsigned char)rgb.r;
		hwmap[offset + 12] = (unsigned char)rgb.g;
		hwmap[offset + 24] = (unsigned char)rgb.b;
	}

	// First chunk comes with header
	workbuf[0] = 0x00;
	workbuf[1] = 0xa1;
	workbuf[2] = 0x01;
	workbuf[3] = 0x01;
	workbuf[4] = 0xb4;
	memcpy(&workbuf[5], hwmap, 60);
	if (hid_write(led_device, workbuf, 65) != 65) {
		return RV_FAILURE;
	}

	//usleep(5000);

	// Six more chunks
	for (i = 1; i < 7; i++) {
		workbuf[0] = 0x00;
		memcpy(&workbuf[1], &hwmap[(i * 64) - 4], 64);
		if (hid_write(led_device, workbuf, 65) != 65) {
			return RV_FAILURE;
		}
		//usleep(5000);
	}

	return RV_SUCCESS;
}

int rv_send_init(int type, int opt) {
	int rc =
		rv_get_ctrl_report(0x0f)             ||
		rv_set_ctrl_report(0x15, type, opt)  ||
		rv_wait_for_ctrl_device()            ||
		rv_set_ctrl_report(0x05, type, opt)  ||
		rv_wait_for_ctrl_device()            ||
		rv_set_ctrl_report(0x07, type, opt)  ||
		rv_wait_for_ctrl_device()            ||
		rv_set_ctrl_report(0x0a, type, opt)  ||
		rv_wait_for_ctrl_device()            ||
		rv_set_ctrl_report(0x0b, type, opt)  ||
		rv_wait_for_ctrl_device()            ||
		rv_set_ctrl_report(0x06, type, opt)  ||
		rv_wait_for_ctrl_device()            ||
		rv_set_ctrl_report(0x09, type, opt)  ||
		rv_wait_for_ctrl_device()            ||
		rv_set_ctrl_report(0x0d, type, opt)  ||
		rv_wait_for_ctrl_device()            ||
		rv_set_ctrl_report(0x13, type, opt)  ||
		rv_wait_for_ctrl_device();

		rv_close_ctrl_device();

		return rc;
}
