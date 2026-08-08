#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

static char *rv_trim_whitespace(char *text) {
    char *end = NULL;

    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }

    if (*text == '\0') {
        return text;
    }

    end = text + strlen(text);
    while (end > text && isspace((unsigned char)*(end - 1))) {
        --end;
    }
    *end = '\0';

    return text;
}

int rv_parse_color_assignment(const char *line, int *color_idx, rv_rgb *rgb) {
    int idx = -1;
    int r = 0, g = 0, b = 0;
    if (!line || !color_idx || !rgb) {
        return RV_FAILURE;
    }
    if (sscanf(line, "color %d %d %d %d", &idx, &r, &g, &b) != 4) {
        return RV_FAILURE;
    }
    if (idx < 0 || idx >= RV_NUM_COLORS) {
        return RV_FAILURE;
    }
    rgb->r = (int16_t)r;
    rgb->g = (int16_t)g;
    rgb->b = (int16_t)b;
    *color_idx = idx;
    return RV_SUCCESS;
}

int rv_parse_layout_assignment(const char *line, int *layout) {
    char layout_name[16] = {0};

    if (!line || !layout) {
        return RV_FAILURE;
    }

    if (sscanf(line, "layout %15s", layout_name) != 1) {
        return RV_FAILURE;
    }

    if (strcmp(layout_name, "ansi") == 0) {
        *layout = RV_TOPO_ANSI;
        return RV_SUCCESS;
    }
    if (strcmp(layout_name, "iso") == 0) {
        *layout = RV_TOPO_ISO;
        return RV_SUCCESS;
    }
    return RV_FAILURE;
}

int rv_apply_preset_file(const char *path, int *mode, int *speed, int *layout, rv_rgb colors[RV_NUM_COLORS], rv_rgb *fixed[RV_NUM_KEYS]) {
    FILE *fp = fopen(path, "r");
    char line[256];

    if (!fp) {
        return RV_FAILURE;
    }

    if (mode) {
        *mode = RV_MODE_FX;
    }
    if (speed) {
        *speed = 6;
    }
    if (layout) {
        *layout = RV_TOPO_ISO;
    }
    if (fixed) {
        memset(fixed, 0, sizeof(rv_rgb *) * RV_NUM_KEYS);
    }

    while (fgets(line, sizeof(line), fp)) {
        char *trim = rv_trim_whitespace(line);

        if (*trim == '\0' || *trim == '#') {
            continue;
        }

        if (strncmp(trim, "mode", 4) == 0) {
            char mode_name[32] = {0};
            if (sscanf(trim, "mode %31s", mode_name) == 1 && mode) {
                if (strcmp(mode_name, "wave") == 0) {
                    *mode = RV_MODE_WAVE;
                }
                else if (strcmp(mode_name, "fx") == 0) {
                    *mode = RV_MODE_FX;
                }
            }
        }
        else if (strncmp(trim, "speed", 5) == 0) {
            int parsed = 0;
            if (sscanf(trim, "speed %d", &parsed) == 1 && speed) {
                *speed = parsed;
            }
        }
        else if (strncmp(trim, "layout", 6) == 0) {
            int parsed_layout = RV_TOPO_ISO;
            if (rv_parse_layout_assignment(trim, &parsed_layout) == RV_SUCCESS && layout) {
                *layout = parsed_layout;
            }
        }
        else if (strncmp(trim, "color", 5) == 0) {
            int idx = -1;
            rv_rgb rgb = {0};
            if (rv_parse_color_assignment(trim, &idx, &rgb) == RV_SUCCESS && colors) {
                colors[idx] = rgb;
            }
        }
    }

    if (fclose(fp) != 0) {
        return RV_FAILURE;
    }

    return RV_SUCCESS;
}
