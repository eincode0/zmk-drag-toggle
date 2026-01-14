/*
 * SPDX-License-Identifier: MIT
 */
#define DT_DRV_COMPAT zmk_behavior_drag_toggle

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <drivers/behavior.h>

#include <zmk/behavior.h>

/* ZMKの &mkp の実体(behavior)名 */
#define MKP_BEHAVIOR_NAME "mouse_key_press"

/* ダブルタップ判定(ms) */
#ifndef DT_TAP_TERM_MS
#define DT_TAP_TERM_MS 250
#endif

struct drag_toggle_data {
    bool locked;
    int64_t last_tap_ms;
};

static inline void invoke_mkp(uint32_t button, struct zmk_behavior_binding_event event, bool pressed) {
    struct zmk_behavior_binding mkp = {
        .behavior_dev = MKP_BEHAVIOR_NAME,
        .param1 = button,
        .param2 = 0,
    };
    zmk_behavior_invoke_binding(&mkp, event, pressed);
}

static int drag_toggle_pressed(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
    const struct device *dev = binding->behavior_dev;
    struct drag_toggle_data *data = (struct drag_toggle_data *)dev->data;

    uint32_t button = binding->param1;
    int64_t now = k_uptime_get();

    /* ロック中なら押した瞬間に解除 */
    if (data->locked) {
        invoke_mkp(button, event, false); /* release */
        data->locked = false;
        data->last_tap_ms = 0;
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* ダブルタップならロックON（press だけ送る） */
    if (data->last_tap_ms != 0 && (now - data->last_tap_ms) <= DT_TAP_TERM_MS) {
        invoke_mkp(button, event, true); /* press */
        data->locked = true;
        data->last_tap_ms = 0;
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* シングルタップは即クリック（press→release） */
    invoke_mkp(button, event, true);
    invoke_mkp(button, event, false);

    data->last_tap_ms = now;
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
