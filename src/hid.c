#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "hidapi/hidapi.h"

#include "roccat-vulcan.h"

#define RV_CTRL_INTERFACE 1
#define RV_LED_INTERFACE  3

hid_device *ctrl_handle, *led_handle;

int rv_open_device() {
	wchar_t wstr[RV_MAX_STR];
	struct hid_device_info *devs, *dev, *ctrl, *led;
	int p = 0;

	while (rv_products[p]) {
		unsigned short product_id = rv_products[p];

		devs = hid_enumerate(RV_VENDOR, product_id);
		dev = devs;
		ctrl = NULL;
		led = NULL;
		while (dev) {
			if (dev->interface_number == RV_CTRL_INTERFACE) ctrl = dev;
			if (dev->interface_number == RV_LED_INTERFACE)   led = dev;
			dev = dev->next;
		}

		if (!ctrl || !led) {
			rv_printf(RV_LOG_VERBOSE, "open_device(%04hx, %04hx): No matching device found\n", RV_VENDOR, product_id);
			goto NEXT_PRODUCT;
		}

		ctrl_handle = hid_open_path(ctrl->path);
		led_handle  = hid_open_path(led->path);

		if (!ctrl_handle || !led_handle) {
			rv_printf(RV_LOG_VERBOSE, "open_device(%04hx, %04hx): Unable to open HID devices %s and/or %s\n", RV_VENDOR, product_id, ctrl->path, led->path);
			goto NEXT_PRODUCT;
		}

		if (hid_get_product_string(ctrl_handle, wstr, RV_MAX_STR) < 0) {
			rv_printf(RV_LOG_VERBOSE, "open_device(%04hx, %04hx): Unable to get product string.\n", RV_VENDOR, product_id);
			goto NEXT_PRODUCT;
		};

		rv_printf(RV_LOG_VERBOSE, "open_device(%04hx, %04hx): %ls, devices %s and %s\n", RV_VENDOR, product_id, wstr, ctrl->path, led->path);
		hid_free_enumeration(devs);
		return 0;

		NEXT_PRODUCT:
		hid_free_enumeration(devs); devs = NULL;
		p++;
	}

	if (devs) hid_free_enumeration(devs);
	return -1;
}

int rv_wait_for_ctrl_device() {
	unsigned char buffer[] = { 0x04, 0x00, 0x00, 0x00 };
	int res;

	while (1) {
		// 150ms is the magic number here, should suffice on first try.
		usleep(150000);

		res = hid_get_feature_report(ctrl_handle, buffer, sizeof(buffer));
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
	res = hid_get_feature_report(ctrl_handle, buffer, length);
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

	res = hid_send_feature_report(ctrl_handle, buffer, length);
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
	if (hid_write(led_handle, workbuf, 65) != 65) {
		return RV_FAILURE;
	}

	// Six more chunks
	for (i = 1; i < 7; i++) {
		workbuf[0] = 0x00;
		memcpy(&workbuf[1], &hwmap[(i * 64) - 4], 64);
		if (hid_write(led_handle, workbuf, 65) != 65) {
			return RV_FAILURE;
		}
	}

	return RV_SUCCESS;
}

int rv_send_init(int type, int opt) {
	return (
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
		rv_wait_for_ctrl_device()
	);
}
