/*
 * SPDX-License-Identifier: MIT
 */
#define DT_DRV_COMPAT zmk_behavior_drag_toggle

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/behavior.h>

#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>

struct drag_toggle_data {
    bool locked;
};

static int drag_toggle_pressed(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
    (void)event;

    const struct device *dev = binding->behavior_dev;
    struct drag_toggle_data *data = dev->data;

    struct zmk_behavior_binding mb1 = {
        .behavior_dev = DEVICE_DT_GET(DT_NODELABEL(mkp)),
        .param1 = binding->param1, /* MB1 */
    };

    if (!data->locked) {
        zmk_behavior_invoke_binding(&mb1, true);
        data->locked = true;
    } else {
        zmk_behavior_invoke_binding(&mb1, false);
        data->locked = false;
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int drag_toggle_released(struct zmk_behavior_binding *binding,
                                struct zmk_behavior_binding_event event) {
    (void)binding;
    (void)event;
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api drag_toggle_api = {
    .binding_pressed = drag_toggle_pressed,
    .binding_released = drag_toggle_released,
};

static int drag_toggle_init(const struct device *dev) {
    struct drag_toggle_data *data = dev->data;
    data->locked = false;
    return 0;
}

#define DRAG_TOGGLE_DEVICE(inst)                                                     \
    static struct drag_toggle_data drag_toggle_data_##inst;                           \
    DEVICE_DT_INST_DEFINE(inst, drag_toggle_init, NULL,                               \
                          &drag_toggle_data_##inst, NULL,                             \
                          APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,           \
                          &drag_toggle_api);

DT_INST_FOREACH_STATUS_OKAY(DRAG_TOGGLE_DEVICE)
