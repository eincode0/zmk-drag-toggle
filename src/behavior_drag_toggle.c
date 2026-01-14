/*
 * SPDX-License-Identifier: MIT
 */
#define DT_DRV_COMPAT zmk_behavior_drag_toggle

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include <drivers/behavior.h>   // ← これが必要（incomplete対策）
#include <zmk/behavior.h>
#include <zmk/hid.h>            // zmk_hid_mouse_buttons_press/release
#include <dt-bindings/zmk/mouse.h>

#ifndef DT_TAP_TERM_MS
#define DT_TAP_TERM_MS 250  // 2回押し判定(ms)
#endif

struct drag_toggle_data {
    bool locked;

    uint32_t button;
    uint8_t tap_count;                 // 0 or 1（2回目は押下中に判定する）
    struct k_work_delayable reset_work;
};

static void reset_tap_work(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct drag_toggle_data *data =
        CONTAINER_OF(dwork, struct drag_toggle_data, reset_work);

    data->tap_count = 0;
}

static int drag_toggle_pressed(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
    (void)event;

    const struct device *dev = binding->behavior_dev;
    struct drag_toggle_data *data = (struct drag_toggle_data *)dev->data;

    uint32_t button = binding->param1;

    /* ロック中：押した瞬間に解除 */
    if (data->locked) {
        zmk_hid_mouse_buttons_release(button);
        data->locked = false;
        data->tap_count = 0;
        k_work_cancel_delayable(&data->reset_work);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* 1回目が終わっていて、期限内に2回目が来た → 2回目のpress */
    if (data->tap_count == 1 && data->button == button) {
        k_work_cancel_delayable(&data->reset_work);

        /* 2回目：まず押す（この後、releaseでロックにする） */
        zmk_hid_mouse_buttons_press(button);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* 1回目：通常クリック開始（press） */
    data->button = button;
    data->tap_count = 1;
    zmk_hid_mouse_buttons_press(button);

    return ZMK_BEHAVIOR_OPAQUE;
}

static int drag_toggle_released(struct zmk_behavior_binding *binding,
                                struct zmk_behavior_binding_event event) {
    (void)event;

    const struct device *dev = binding->behavior_dev;
    struct drag_toggle_data *data = (struct drag_toggle_data *)dev->data;

    uint32_t button = binding->param1;

    if (data->locked) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* 1回目のrelease：いったん普通に離す（クリック成立）＋2回目待ちタイマー開始 */
    if (data->tap_count == 1 && data->button == button) {
        zmk_hid_mouse_buttons_release(button);
        k_work_reschedule(&data->reset_work, K_MSEC(DT_TAP_TERM_MS));
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* 2回目のrelease：ここで“離さない”＝ロック完成（押しっぱなしにする） */
    if (data->tap_count == 1 && data->button == button) {
        /* この分岐には通常来ません（pressed側で処理済み） */
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* 「2回目のpress後のrelease」判定：tap_count==1のままなので、タイマーが止まってるならロック */
    if (data->button == button && k_work_delayable_is_pending(&data->reset_work) == false) {
        data->locked = true;
        data->tap_count = 0;
        return ZMK_BEHAVIOR_OPAQUE;
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
    data->button = MB1;
    data->tap_count = 0;
    k_work_init_delayable(&data->reset_work, reset_tap_work);
    return 0;
}

#define DRAG_TOGGLE_DEVICE(inst)                                                     \
    static struct drag_toggle_data drag_toggle_data_##inst;                           \
    DEVICE_DT_INST_DEFINE(inst, drag_toggle_init, NULL,                               \
                          &drag_toggle_data_##inst, NULL,                             \
                          APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,           \
                          &drag_toggle_api);

DT_INST_FOREACH_STATUS_OKAY(DRAG_TOGGLE_DEVICE)
