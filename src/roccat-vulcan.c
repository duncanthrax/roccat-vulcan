#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#include "roccat-vulcan.h"

#define FX_MODE_IMPACT 0
#define FX_MODE_PIPED 1

// Globals
uint16_t rv_products[3]   = { 0x3098, 0x307a,  0x0000 };
char * rv_products_str[3] = { "3098", "307a",  NULL };
int rv_verbose = 0;
rv_rgb rv_colors[RV_NUM_COLORS] = {
	{ .r = 0x0000, .g = 0x0000, .b =  0x0077 },
	{ .r = 0x08ff, .g = 0x0000, .b = -0x00ff },
	{ .r = 0x08ff, .g = 0x0000, .b = -0x008f },
	{ .r = 0x08ff, .g = 0x0000, .b =  0x0000 },
	{ .r = 0x00bb, .g = 0x0000, .b =  0x00cc },
	{ .r = 0x0099, .g = 0x0000, .b =  0x00bb },
	{ .r = 0x0055, .g = 0x0000, .b =  0x00aa },
	{ .r = 0x0000, .g = 0x0000, .b =  0x0000 },
	{ .r = 0x0000, .g = 0x0000, .b =  0x0000 },
	{ .r = 0x0000, .g = 0x0000, .b =  0x0000 }
};
char * rv_colors_desc[RV_NUM_COLORS] = {
	"Base keyboard color (dark blue)",
	"Typing color, initial key (over-red, under-blue)",
	"Typing color, first neighbor key",
	"Typing color, second neighbor key",
	"Ghost typing color, initial key",
	"Ghost typing color, first neighbor key",
	"Ghost typing color, second neighbor key",
	NULL,
	NULL,
	NULL
};

int rv_topo_model = RV_TOPO_ISO;

rv_rgb* rv_fixed[RV_NUM_KEYS];

rv_rgb rv_color_off = { .r = 0x0000, .g = 0x0000, .b = 0x0000 };

void show_usage(const char *arg0) {
	rv_printf(RV_LOG_NORMAL, "\n");
	rv_printf(RV_LOG_NORMAL, "By default, %s plays 'impact' effect. In this mode, effect colors can\n", arg0);
	rv_printf(RV_LOG_NORMAL, "be changed by providing -c options and keys can be set to a fixed color\n");
	rv_printf(RV_LOG_NORMAL, "using -k options\n");
	rv_printf(RV_LOG_NORMAL, "\n");
	rv_printf(RV_LOG_NORMAL, "-b [hwmodel]       : Specify keyboard key layout. Supported hwmodels are 'iso'\n");
	rv_printf(RV_LOG_NORMAL, "                     and 'ansi'. Default is 'iso'.\n");
	rv_printf(RV_LOG_NORMAL, "-c [colorIdx:r,g,b]: Change effect colors. Up to 10 colors with 'colorIdx' values\n");
	rv_printf(RV_LOG_NORMAL, "                     in the range of 0..9 can be specified. RGB values are given as\n");
	rv_printf(RV_LOG_NORMAL, "                     signed integers (−32768..32767), with effective values\n");
	rv_printf(RV_LOG_NORMAL, "                     being 0..255. Check the README.md for more information.\n");
	rv_printf(RV_LOG_NORMAL, "-k [keyName:r,g,b] : Set the key with 'keyName' to a static color. Keynames\n");
	rv_printf(RV_LOG_NORMAL, "                     are evdev KEY_* constants. RGB values should be in the\n");
	rv_printf(RV_LOG_NORMAL, "                     effective range of 0..255.\n");
	rv_printf(RV_LOG_NORMAL, "-v                 : Be verbose.\n");
	rv_printf(RV_LOG_NORMAL, "\n");
	rv_printf(RV_LOG_NORMAL, "-p [pipePath]      : Read commands from a named pipe. To set a key to a static color,\n");
	rv_printf(RV_LOG_NORMAL, "                     write rgb:keyName:r,g,b to the pipe. Keynames are evdev KEY_*\n");
	rv_printf(RV_LOG_NORMAL, "                     constants or alternatively 'all' to modify all keys.\n");
	rv_printf(RV_LOG_NORMAL, "                     RGB values should be in the effective range of 0..255.\n");
	rv_printf(RV_LOG_NORMAL, "                     Other command line options do not apply.\n");
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
	void (*topo_func)();
	int fx_mode = FX_MODE_IMPACT;
	char *file_name;

	setvbuf(stdout, NULL, _IONBF, 0);

	memset(rv_fixed, 0, sizeof(rv_fixed));

	rv_printf(RV_LOG_NORMAL, "ROCCAT Vulcan for Linux [github.com/duncanthrax/roccat-vulcan]\n");

	while ((opt = getopt(argc, argv, "hvw:p:c:k:b:t:")) != -1) {
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
			case 'b':
				if (strcmp(optarg,"ansi") == 0) {
					rv_topo_model = RV_TOPO_ANSI;
				}
				else if (strcmp(optarg,"iso") == 0) {
					rv_topo_model = RV_TOPO_ISO;
				}
				else {
					rv_printf(RV_LOG_NORMAL, "Error: Unknown topology hwmodel '%s'\n", optarg);
					return -1;
				};
			break;
			case 't':
				mode  = RV_MODE_TOPO;
				if (strcmp(optarg,"rows") == 0) {
					topo_func = &rv_fx_topo_rows;
				}
				else if (strcmp(optarg,"cols") == 0) {
					topo_func = &rv_fx_topo_cols;
				}
				else if (strcmp(optarg,"keys") == 0) {
					topo_func = &rv_fx_topo_keys;
				}
				else if (strcmp(optarg,"neigh") == 0) {
					topo_func = &rv_fx_topo_neigh;
				}
				else {
					rv_printf(RV_LOG_NORMAL, "Error: Unknown topology recording function '%s'\n", optarg);
					return -1;
				};
			break;
			case 'v':
				rv_verbose = 1;
			break;
			case 'p':
				fx_mode = FX_MODE_PIPED;
				file_name = optarg;
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
		case RV_MODE_TOPO:
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

			(*topo_func)();

		break;

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

			if (fx_mode == FX_MODE_PIPED) {
				rv_printf(RV_LOG_NORMAL, "Reading commands pipe '%s'\n", file_name);
				rv_printf(RV_LOG_NORMAL, "Command format: keyName:r,g,b\n");
				rv_printf(RV_LOG_NORMAL, "Keynames are evdev KEY_* constants, or 'all'.\n");
				rv_printf(RV_LOG_NORMAL, "RGB values should be in the effective range of 0..255.\n");
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

				rv_fx_piped(file_name);
			}
			else {
				rv_printf(RV_LOG_NORMAL, "Effect Color Table (change these with -c option)\n");
				rv_printf(RV_LOG_NORMAL, "colorIdx    R      G      B  Desc\n");
				rv_printf(RV_LOG_NORMAL, "------------------------------------------------\n");

				for (i = 0; i < RV_NUM_COLORS; i++) {
					rv_printf(RV_LOG_NORMAL, "%d     % 7hd% 7hd% 7hd  %s\n", i, rv_colors[i].r, rv_colors[i].g, rv_colors[i].b, rv_colors_desc[i]);
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

				rv_fx_impact();
			}
		break;

		default:
			show_usage(argv[0]);
	}
}
