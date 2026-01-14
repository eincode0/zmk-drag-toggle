/*
 * SPDX-License-Identifier: MIT
 */
#define DT_DRV_COMPAT zmk_behavior_drag_toggle

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include <zmk/behavior.h>

struct drag_toggle_data {
    bool locked;
};

static int drag_toggle_pressed(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
    (void)event;

    uint32_t button = binding->param1;

    const struct device *dev =
        zmk_behavior_get_binding(binding->behavior_dev);
    struct drag_toggle_data *data =
        (struct drag_toggle_data *)dev->data;

    /* ZMK標準の &mkp を呼び出す */
    struct zmk_behavior_binding mkp_binding = {
        .behavior_dev = DEVICE_DT_GET(DT_NODELABEL(mkp)),
        .param1 = button,
        .param2 = 0,
    };

    if (!data->locked) {
        zmk_behavior_queue_add(&mkp_binding, event, true);   // press
        data->locked = true;
    } else {
        zmk_behavior_queue_add(&mkp_binding, event, false);  // release
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
    struct drag_toggle_data *data = (struct drag_toggle_data *)dev->data;
    data->locked = false;
    return 0;
}

#define DRAG_TOGGLE_DEVICE(inst)                                   \
    static struct drag_toggle_data drag_toggle_data_##inst;        \
    DEVICE_DT_INST_DEFINE(inst, drag_toggle_init, NULL,            \
        &drag_toggle_data_##inst, NULL,                            \
        APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,          \
        &drag_toggle_api);

DT_INST_FOREACH_STATUS_OKAY(DRAG_TOGGLE_DEVICE)
