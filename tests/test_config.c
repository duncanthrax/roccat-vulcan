#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/roccat-vulcan.h"
#include "../src/config.h"

static int rv_test_main(void) {
    return 0;
}

int main(void) {
    rv_test_main();
    rv_rgb colors[RV_NUM_COLORS] = {0};
    rv_rgb *fixed[RV_NUM_KEYS] = {0};
    int mode = RV_MODE_FX;
    int speed = 6;
    int layout = RV_TOPO_ISO;
    int idx = -1;
    rv_rgb rgb = {0};
    char temp_path[] = "/tmp/roccat-vulcan-test-XXXXXX";
    int fd = mkstemp(temp_path);
    char payload[] = "mode wave\nspeed 8\nlayout ansi\ncolor 0 10 20 30\n";

    assert(fd >= 0);
    assert(write(fd, payload, strlen(payload)) == (ssize_t)strlen(payload));
    close(fd);

    assert(rv_parse_color_assignment("color 0 10 20 30", &idx, &rgb) == RV_SUCCESS);
    assert(idx == 0);
    assert(rgb.r == 10 && rgb.g == 20 && rgb.b == 30);

    assert(rv_parse_layout_assignment("layout ansi", &layout) == RV_SUCCESS);
    assert(layout == RV_TOPO_ANSI);

    assert(rv_apply_preset_file(temp_path, &mode, &speed, &layout, colors, fixed) == RV_SUCCESS);
    assert(mode == RV_MODE_WAVE);
    assert(speed == 8);
    assert(layout == RV_TOPO_ANSI);
    assert(colors[0].r == 10 && colors[0].g == 20 && colors[0].b == 30);

    unlink(temp_path);
    puts("config parser tests passed");
    return 0;
}
