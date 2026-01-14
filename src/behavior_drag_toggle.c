/*
 * SPDX-License-Identifier: MIT
 */
#define DT_DRV_COMPAT zmk_behavior_drag_toggle

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <drivers/behavior.h>

#include <zmk/behavior.h>

#ifndef DT_TAP_TERM_MS
#define DT_TAP_TERM_MS 250 /* ダブルタップ判定(ms) */
#endif

struct drag_toggle_data {
    bool locked;                 /* drag lock中か */
    bool pending_single;         /* 単押し確定待ち */
    uint32_t pending_button;     /* MB1等 */
    struct zmk_behavior_binding_event pending_event; /* 単押し用にeventを保存 */
    struct k_work_delayable single_work;
};

/* &mkp を呼ぶ（あなたの環境では behavior_dev が「const char*」系なので NAME_GET を使う） */
static inline void invoke_mkp(uint32_t button,
                              struct zmk_behavior_binding_event event,
                              bool pressed) {
    struct zmk_behavior_binding mkp_binding = {
    .behavior_dev = DEVICE_DT_NAME_GET(DT_NODELABEL(mouse_key_press)),
    .param1 = button,
    .param2 = 0,
    };

    (void)zmk_behavior_invoke_binding(&mkp_binding, event, pressed);
}

static void do_click_work(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct drag_toggle_data *data =
        CONTAINER_OF(dwork, struct drag_toggle_data, single_work);

    if (!data->pending_single) {
        return;
    }

    data->pending_single = false;

    /* 単押し確定 = click（press→release） */
    invoke_mkp(data->pending_button, data->pending_event, true);
    invoke_mkp(data->pending_button, data->pending_event, false);
}

static int drag_toggle_pressed(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
    /* one_param.yaml 想定：binding->param1 に MB1/MB2... */
    uint32_t button = binding->param1;

    /* 自分(=drag_toggle)の状態領域を取得 */
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct drag_toggle_data *data = (struct drag_toggle_data *)dev->data;

    /* すでにロック中なら、次の押下で解除（release） */
    if (data->locked) {
        invoke_mkp(button, event, false);
        data->locked = false;

        data->pending_single = false;
        k_work_cancel_delayable(&data->single_work);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* 単押し待ちが同じボタンで来た = ダブルタップ成立 → ロックON（pressしっぱなし） */
    if (data->pending_single && data->pending_button == button) {
        data->pending_single = false;
        k_work_cancel_delayable(&data->single_work);

        invoke_mkp(button, event, true); /* press保持 */
        data->locked = true;
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* 1回目：期限まで2回目が来なければ click にする */
    data->pending_single = true;
    data->pending_button = button;
    data->pending_event = event; /* eventを保存 */
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
    data->pending_button = 0;
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
