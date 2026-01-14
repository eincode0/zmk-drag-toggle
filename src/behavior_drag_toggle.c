/*
 * SPDX-License-Identifier: MIT
 */
#define DT_DRV_COMPAT zmk_behavior_drag_toggle

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/behavior.h>

#include <zmk/behavior.h>

#if !DT_NODE_EXISTS(DT_NODELABEL(mkp))
#error "DT_NODELABEL(mkp) not found. Your ZMK devicetree has no node label 'mkp'."
#endif

struct drag_toggle_data {
    bool locked;
};

static int drag_toggle_pressed(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
    /* one_param.yaml 想定：binding->param1 に MB1 などが入る */
    uint32_t button = binding->param1;

    const struct device *dev = binding->behavior_dev;
    struct drag_toggle_data *data = (struct drag_toggle_data *)dev->data;

    /* ZMK標準の &mkp を“呼び出す”ための binding を作る */
    struct zmk_behavior_binding mkp_binding = {
        .behavior_dev = DEVICE_DT_GET(DT_NODELABEL(mkp)),
        .param1 = button,
        .param2 = 0,
    };

    if (!data->locked) {
        /* 押しっぱなし開始 = press */
        zmk_behavior_invoke_binding(&mkp_binding, event, true);
        data->locked = true;
    } else {
        /* 押しっぱなし終了 = release */
        zmk_behavior_invoke_binding(&mkp_binding, event, false);
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

#define DRAG_TOGGLE_DEVICE(inst)                                                     \
    static struct drag_toggle_data drag_toggle_data_##inst;                           \
    DEVICE_DT_INST_DEFINE(inst, drag_toggle_init, NULL,                               \
                          &drag_toggle_data_##inst, NULL,                             \
                          APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,           \
                          &drag_toggle_api);

DT_INST_FOREACH_STATUS_OKAY(DRAG_TOGGLE_DEVICE)
