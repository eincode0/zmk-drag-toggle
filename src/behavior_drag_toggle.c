/*
 * SPDX-License-Identifier: MIT
 */
#define DT_DRV_COMPAT zmk_behavior_drag_toggle

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <drivers/behavior.h>

#include <zmk/behavior.h>

#define DOUBLE_TAP_MS 250

struct drag_toggle_data {
    bool locked;
    bool pending_release;
    int64_t last_tap_ms;
};

static int drag_toggle_pressed(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
    uint32_t button = binding->param1;

    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct drag_toggle_data *data = (struct drag_toggle_data *)dev->data;

    struct zmk_behavior_binding mkp_binding = {
        .behavior_dev = "mkp",
        .param1 = button,
        .param2 = 0,
    };

    /* ロック中：押した瞬間に解除（余計なクリックなし） */
    if (data->locked) {
        zmk_behavior_invoke_binding(&mkp_binding, event, false); /* release */
        data->locked = false;
        data->pending_release = false;
        data->last_tap_ms = 0;
        return ZMK_BEHAVIOR_OPAQUE;
    }

    int64_t now = k_uptime_get();
    bool is_double = (data->last_tap_ms != 0) && ((now - data->last_tap_ms) <= DOUBLE_TAP_MS);

    if (is_double) {
        /* 2回目：ドラッグON（押しっぱなし開始） */
        zmk_behavior_invoke_binding(&mkp_binding, event, true); /* press */
        data->locked = true;
        data->pending_release = false;
        data->last_tap_ms = 0;
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* 1回目：普通クリック（release は released 側で行う） */
    data->last_tap_ms = now;
    data->pending_release = true;
    zmk_behavior_invoke_binding(&mkp_binding, event, true); /* press */

    return ZMK_BEHAVIOR_OPAQUE;
}

static int drag_toggle_released(struct zmk_behavior_binding *binding,
                                struct zmk_behavior_binding_event event) {
    uint32_t button = binding->param1;

    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct drag_toggle_data *data = (struct drag_toggle_data *)dev->data;

    struct zmk_behavior_binding mkp_binding = {
        .behavior_dev = "mkp",
        .param1 = button,
        .param2 = 0,
    };

    /* ロック中は release しない（押しっぱなし維持） */
    if (data->locked) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* クリックの release */
    if (data->pending_release) {
        zmk_behavior_invoke_binding(&mkp_binding, event, false); /* release */
        data->pending_release = false;
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api drag_toggle_api = {
    .binding_pressed = drag_toggle_pressed,
    .binding_released = drag_toggle_released,
};

static int drag_toggle_init(const struct device *dev) {
    struct drag_toggle_data *data = (struct drag_toggle_data *)dev->data;
    data->locked = false;
    data->pending_release = false;
    data->last_tap_ms = 0;
    return 0;
}

#define DRAG_TOGGLE_DEVICE(inst)                                                     \
    static struct drag_toggle_data drag_toggle_data_##inst;                           \
    DEVICE_DT_INST_DEFINE(inst, drag_toggle_init, NULL,                               \
                          &drag_toggle_data_##inst, NULL,                             \
                          APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,           \
                          &drag_toggle_api);

DT_INST_FOREACH_STATUS_OKAY(DRAG_TOGGLE_DEVICE)
