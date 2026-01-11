/*
 * SPDX-License-Identifier: MIT
 */
#define DT_DRV_COMPAT zmk_behavior_drag_toggle

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <drivers/behavior.h>

#include <zmk/behavior.h>
#include <zmk/pointing.h>

struct drag_toggle_data {
    bool active;
};

static int drag_toggle_pressed(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
    const struct device *dev = binding->behavior_dev;
    struct drag_toggle_data *data = (struct drag_toggle_data *)dev->data;

    data->active = !data->active;

    if (data->active) {
        zmk_pointing_button_press(POINTING_BUTTON_LEFT);
    } else {
        zmk_pointing_button_release(POINTING_BUTTON_LEFT);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int drag_toggle_released(struct zmk_behavior_binding *binding,
                                struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api drag_toggle_api = {
    .binding_pressed = drag_toggle_pressed,
    .binding_released = drag_toggle_released,
};

static int drag_toggle_init(const struct device *dev) {
    struct drag_toggle_data *data = (struct drag_toggle_data *)dev->data;
    data->active = false;
    return 0;
}

static struct drag_toggle_data drag_toggle_data_0;

DEVICE_DT_INST_DEFINE(0, drag_toggle_init, NULL,
                      &drag_toggle_data_0, NULL,
                      APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                      &drag_toggle_api);
