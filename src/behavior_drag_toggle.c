/*
 * SPDX-License-Identifier: MIT
 */
#define DT_DRV_COMPAT zmk_behavior_drag_toggle

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include <zmk/behavior.h>
#include <zmk/hid.h>
#include <dt-bindings/zmk/mouse.h>

#ifndef DT_TAP_TERM_MS
#define DT_TAP_TERM_MS 250 /* ダブルクリック判定(ms) */
#endif

struct drag_toggle_data {
    bool locked;                     /* ドラッグロック中か */
    bool pending_single;             /* 単押し(クリック)待ち */
    uint32_t pending_button;         /* 待ってるボタン(MB1等) */
    struct k_work_delayable single_work;
};

static void do_click_work(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct drag_toggle_data *data =
        CONTAINER_OF(dwork, struct drag_toggle_data, single_work);

    if (!data->pending_single) {
        return;
    }

    uint32_t btn = data->pending_button;
    data->pending_single = false;

    /* 単押し = click (press -> release) */
    zmk_hid_mouse_button_press(btn);
    zmk_hid_mouse_button_release(btn);
}

static int drag_toggle_pressed(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
    (void)event;

    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct drag_toggle_data *data = (struct drag_toggle_data *)dev->data;

    /* one_param.yaml 想定：binding->param1 に MB1/MB2... */
    uint32_t button = binding->param1;

    /* ロック中なら、押した瞬間に解除 */
    if (data->locked) {
        zmk_hid_mouse_button_release(button);
        data->locked = false;
        data->pending_single = false;
        k_work_cancel_delayable(&data->single_work);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* すでに単押し待ちがある = 2回目(ダブル) */
    if (data->pending_single && data->pending_button == button) {
        data->pending_single = false;
        k_work_cancel_delayable(&data->single_work);

        /* ダブル押し = ロックON（pressしたまま） */
        zmk_hid_mouse_button_press(button);
        data->locked = true;
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* 1回目：ダブル判定待ちを開始（期限まで2回目が来なければ click） */
    data->pending_single = true;
    data->pending_button = button;
    k_work_reschedule(&data->single_work, K_MSEC(DT_TAP_TERM_MS));

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
    data->pending_single = false;
    data->pending_button = MB1;
    k_work_init_delayable(&data->single_work, do_click_work);
    return 0;
}

#define DRAG_TOGGLE_DEVICE(inst)                                                     \
    static struct drag_toggle_data drag_toggle_data_##inst;                           \
    DEVICE_DT_INST_DEFINE(inst, drag_toggle_init, NULL,                               \
                          &drag_toggle_data_##inst, NULL,                             \
                          APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,           \
                          &drag_toggle_api);

DT_INST_FOREACH_STATUS_OKAY(DRAG_TOGGLE_DEVICE)
