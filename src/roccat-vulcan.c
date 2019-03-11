#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#include "roccat-vulcan.h"

// Globals
uint16_t rv_products[3]   = { 0x3098, 0x307a,  0x0000 };
char * rv_products_str[3] = { "3098", "307a",  NULL };
int rv_verbose = 0;
int rv_daemon  = 0;
rv_rgb rv_colors[RV_NUM_COLORS] = {
	{ .r = 0x0000, .g = 0x0000, .b =  0x0077 }, // Base color, dark blue
	{ .r = 0x08ff, .g = 0x0000, .b = -0x00ff },	// Real typing color, initial key (over-red and under-blue)
	{ .r = 0x08ff, .g = 0x0000, .b = -0x008f },	// Real typing color, second-stage key
	{ .r = 0x08ff, .g = 0x0000, .b =  0x0000 },	// Real typing color, third-stage key
	{ .r = 0x00bb, .g = 0x0000, .b =  0x00cc }, // Ghost typing color, initial key
	{ .r = 0x0099, .g = 0x0000, .b =  0x00bb }, // Ghost typing color, second-stage key
	{ .r = 0x0055, .g = 0x0000, .b =  0x00aa }, // Ghost typing color, third-stage key
	{ .r = 0x0000, .g = 0x0000, .b =  0x0000 },
	{ .r = 0x0000, .g = 0x0000, .b =  0x0000 },
	{ .r = 0x0000, .g = 0x0000, .b =  0x0000 }
};

rv_rgb* rv_fixed[RV_NUM_KEYS];

rv_rgb rv_color_off = { .r = 0x0000, .g = 0x0000, .b = 0x0000 };

void show_usage(const char *arg0) {
	rv_printf(RV_LOG_NORMAL, "\n");
	rv_printf(RV_LOG_NORMAL, "By default, %s forks into background and plays 'impact' effect. In this\n", arg0);
	rv_printf(RV_LOG_NORMAL, "mode, effect colors can be changed by providing -c options and keys can be\n");
	rv_printf(RV_LOG_NORMAL, "set to a fixed color using -k options\n");
	rv_printf(RV_LOG_NORMAL, "\n");
	rv_printf(RV_LOG_NORMAL, "-c [colorIdx:r,g,b]: Change effect colors. Up to 10 colors with 'colorIdx' values\n");
	rv_printf(RV_LOG_NORMAL, "                     in the range of 0..9 can be specified. RGB values are given as\n");
	rv_printf(RV_LOG_NORMAL, "                     signed integers (−32768..32767), with effective values\n");
	rv_printf(RV_LOG_NORMAL, "                     being 0..255. Check the README.md for more information.\n");
	rv_printf(RV_LOG_NORMAL, "-k [keyName:r,g,b] : Set the key with 'keyName' to a static color. Keynames\n");
	rv_printf(RV_LOG_NORMAL, "                     are evdev KEY_* constants. RGB values should be in the\n");
	rv_printf(RV_LOG_NORMAL, "                     effective range of 0..255.\n");
	rv_printf(RV_LOG_NORMAL, "-v                 : Be verbose and don't fork, keep logging to STDOUT instead.\n");
	rv_printf(RV_LOG_NORMAL, "\n");
	rv_printf(RV_LOG_NORMAL, "-w [speed]         : Set up 'wave' effect with desired speed (1-11) and quit.\n");
	rv_printf(RV_LOG_NORMAL, "                     This effect is run by the hardware and does not require\n");
	rv_printf(RV_LOG_NORMAL, "                     host support. Other command line options do not apply.\n");
	rv_printf(RV_LOG_NORMAL, "\n");
	exit(RV_FAILURE);
}

int main(int argc, char* argv[])
{
	int opt, i;
	pid_t pid;
	int mode  = RV_MODE_FX;
	int speed = 6;
	int rgb_idx = 0;
	rv_rgb rgb;
	char *keyname;

	memset(rv_fixed, 0, sizeof(rv_fixed));

	rv_printf(RV_LOG_NORMAL, "ROCCAT Vulcan for Linux [github.com/duncanthrax/roccat-vulcan]\n");

	while ((opt = getopt(argc, argv, "hvw:c:k:")) != -1) {
		switch (opt) {
			case 'h':
				show_usage(argv[0]);
			break;
			case 'c':
				if (sscanf(optarg, "%d:%hd,%hd,%hd", &rgb_idx, &(rgb.r), &(rgb.g), &(rgb.b)) == 4) {
					if (rgb_idx >= 0 && rgb_idx < 10) {
						rv_printf(RV_LOG_NORMAL, "Color %u set to %hd,%hd,%hd\n", rgb_idx, rgb.r, rgb.g, rgb.b);
						rv_colors[rgb_idx] = rgb;
					}
					else {
						rv_printf(RV_LOG_NORMAL, "Error: No such color index '%d'\n", rgb_idx);
						show_usage(argv[0]);
					}
				}
				else {
					rv_printf(RV_LOG_NORMAL, "Error: Unable to parse color (-c) argument\n");
					show_usage(argv[0]);
				}
			break;
			case 'k':
				if (sscanf(optarg, "%m[^:]:%hd,%hd,%hd", &keyname, &(rgb.r), &(rgb.g), &(rgb.b)) == 4) {
					int k = rv_get_keycode(keyname);
					if (k >= 0) {
						rv_fixed[k] = malloc(sizeof(rv_rgb));
						if (!rv_fixed[k]) {
							rv_printf(RV_LOG_NORMAL, "Error: Unable to allocate memory\n");
							return -1;
						}
						memcpy(rv_fixed[k], &rgb, sizeof(rv_rgb));
						rv_printf(RV_LOG_NORMAL, "Key %s set to fixed color %hd,%hd,%hd\n", keyname, rgb.r, rgb.g, rgb.b);
					}
					else {
						rv_printf(RV_LOG_NORMAL, "Error: Unknown key code '%s'\n", keyname);
						return -1;
					}
				}
				else {
					rv_printf(RV_LOG_NORMAL, "Error: Unable to parse key (-k) argument\n");
					show_usage(argv[0]);
				}
			break;
			case 'w':
				mode  = RV_MODE_WAVE;
				speed = atoi(optarg);
				if (speed) {
					if (speed > 11) speed = 11;
					if (speed < 1) speed = 1;
				}
				else speed = 6;
			break;
			case 'v':
				rv_verbose = 1;
			break;
			default:
				show_usage(argv[0]);
		}
	}
	if (optind < argc) {
		rv_printf(RV_LOG_NORMAL, "Error: Expected argument after options\n");
		show_usage(argv[0]);
	}

	switch (mode) {
		case RV_MODE_WAVE:
			if (rv_open_device() < 0) {
				rv_printf(RV_LOG_NORMAL, "Error: Unable to find keyboard\n");
				return RV_FAILURE;
			}
			if (rv_send_init(RV_MODE_WAVE, speed)) {
				rv_printf(RV_LOG_NORMAL, "Error: Failed to send wave effect sequence.\n");
				return RV_FAILURE;
			}
			rv_printf(RV_LOG_NORMAL, "Wave effect sequence sent, quitting.\n");
			return 0;
		break;

		case RV_MODE_FX:
			if (!rv_verbose) {
				rv_printf(RV_LOG_NORMAL, "Detaching from terminal, running impact effect\n");
				pid = fork();
				if (pid < 0) exit(RV_FAILURE);
				if (pid > 0) exit(RV_SUCCESS);

				//if (setsid() < 0) exit(RV_FAILURE);

				signal(SIGCHLD, SIG_IGN);
				signal(SIGHUP, SIG_IGN);

				pid = fork();
				if (pid < 0) exit(RV_FAILURE);
				if (pid > 0) exit(RV_SUCCESS);

				chdir("/");

				close(0);
				close(1);
				close(2);
			}

			if (rv_open_device() < 0) {
				rv_printf(RV_LOG_NORMAL, "Error: Unable to find keyboard\n");
				return RV_FAILURE;
			}

			if (rv_send_init(RV_MODE_FX, -1)) {
				rv_printf(RV_LOG_NORMAL, "Error: Failed to send initialization sequence.\n");
				return RV_FAILURE;
			}

			if (rv_fx_init() != 0) {
				rv_printf(RV_LOG_NORMAL, "Error: Failed to initialize LEDs\n");
				return RV_FAILURE;
			};

			return rv_fx_impact();
		break;

		default:
			show_usage(argv[0]);
	}
}
