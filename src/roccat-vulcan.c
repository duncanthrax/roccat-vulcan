#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#include "roccat-vulcan.h"

// Globals
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

rv_rgb rv_color_off = { .r = 0x0000, .g = 0x0000, .b = 0x0000 };

void show_usage(const char *arg0) {
	rv_printf(RV_LOG_NORMAL, "\n");
	rv_printf(RV_LOG_NORMAL, "-f         : Fork into background and play 'impact' effect. In this\n");
	rv_printf(RV_LOG_NORMAL, "             mode, colors can be changed by providing -c options.\n");
	rv_printf(RV_LOG_NORMAL, "-w [speed] : Set up 'wave' effect with desired speed (1-12) and quit.\n");
	rv_printf(RV_LOG_NORMAL, "-c [r,g,b] : Change up to 7 colors for 'impact' effect by repeatedly\n");
	rv_printf(RV_LOG_NORMAL, "             specifying this option. RGB values are given as signed\n");
	rv_printf(RV_LOG_NORMAL, "             16-bit integers. See the README for more information on\n");
	rv_printf(RV_LOG_NORMAL, "             the colors and the rationale behind this.\n");
	rv_printf(RV_LOG_NORMAL, "-v         : Be verbose. In '-f' mode, don't fork but keep logging to\n");
	rv_printf(RV_LOG_NORMAL, "             STDOUT instead.\n");
	exit(RV_FAILURE);
}

int main(int argc, char* argv[])
{
	int opt, i;
	pid_t pid;
	int mode  = RV_MODE_NONE;
	int speed = 6;
	int rgb_idx = 0;
	rv_rgb rgb;
	
	rv_printf(RV_LOG_NORMAL, "ROCCAT Vulcan for Linux [github.com/duncanthrax/roccat-vulcan]\n");

	while ((opt = getopt(argc, argv, "fvw:c:")) != -1) {
		switch (opt) {
			case 'c':
				if (rgb_idx < RV_NUM_COLORS && sscanf(optarg, "%hd,%hd,%hd", &(rgb.r), &(rgb.g), &(rgb.b)) == 3) {
					rv_printf(RV_LOG_NORMAL, "Color %u set to %hd,%hd,%hd\n", rgb_idx, rgb.r, rgb.g, rgb.b);
					rv_colors[rgb_idx] = rgb;
				}
				else rv_printf(RV_LOG_NORMAL, "Error: Unable to parse color (-c) argument\n");
				rgb_idx++;
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
			case 'f':
				mode = RV_MODE_FX;
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
			if (rv_open_device(RV_VENDOR, RV_PRODUCT) < 0) {
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

			if (rv_open_device(RV_VENDOR, RV_PRODUCT) < 0) {
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
