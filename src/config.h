#ifndef _H_ROCCAT_VULCAN_CONFIG
#define _H_ROCCAT_VULCAN_CONFIG

#include "roccat-vulcan.h"

int rv_parse_color_assignment(const char *line, int *color_idx, rv_rgb *rgb);
int rv_parse_layout_assignment(const char *line, int *layout);
int rv_apply_preset_file(const char *path, int *mode, int *speed, int *layout, rv_rgb colors[RV_NUM_COLORS], rv_rgb *fixed[RV_NUM_KEYS]);

#endif
