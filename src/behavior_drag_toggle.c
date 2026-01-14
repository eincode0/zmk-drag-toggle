/*
 * SPDX-License-Identifier: MIT
 */
#define DT_DRV_COMPAT zmk_behavior_drag_toggle

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/devicetree.h>

#include <zmk/behavior.h>
#include <dt-bindings/zmk/mouse.h>

/* ダブルクリック判定(ms) */
#ifndef DT_TAP_TERM_MS
#define DT_TAP_TERM_MS 250
#endif

/* ZMK標準の &mkp を呼ぶための “device名(文字列)” を取る */
#define MKP_DEV_NAME DT_LABEL(DT_NODELABEL(mkp))

struct drag_toggle_data {
    bool locked;                     /* ドラッグロック中か */
    bool pending_single;             /* 単押し(クリック)待ち */
    uint32_t pending_button;         /* 待ってるボタン(MB1等) */
    struct zmk_behavior_binding_event pending_event;
    struct k_work_delayable single_work;
};

static void invoke_mkp(uint32_t button, struct zmk_behavior_binding_event ev, bool pressed) {
    struct zmk_behavior_binding mkp = {
        .behavior_dev = MKP_DEV_NAME,
        .param1 = button,
        .param2 = 0,
    };
    zmk_behavior_invoke_binding(&mkp, ev, pressed);
}

static void do_click_work(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct drag_toggle_data *data =
        CONTAINER_OF(dwork, struct drag_toggle_data, single_work);

    if (!data->pending_single) {
        return;
    }

    data->pending_single = false;

    /* 単押し = click（press → release） */
    invoke_mkp(data->pending_button, data->pending_event, true);
    invoke_mkp(data->pending_button, data->pending_event, false);
}

static int drag_toggle_pressed(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct drag_toggle_data *data = (struct drag_toggle_data *)dev->data;

    /* one_param.yaml：param1 に MB1/MB2... が入る */
    uint32_t button = binding->param1;

    /* ロック中なら、押した瞬間に解除（release） */
    if (data->locked) {
        invoke_mkp(button, event, false);
        data->locked = false;
        data->pending_single = false;
        k_work_cancel_delayable(&data->single_work);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* 2回目（同じボタン）＝ダブルクリック成立 → ロックON（pressしたまま） */
    if (data->pending_single && data->pending_button == button) {
        data->pending_single = false;
        k_work_cancel_delayable(&data->single_work);

        invoke_mkp(button, event, true); /* press保持 */
        data->locked = true;
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* 1回目：ダブル判定待ち開始。期限まで2回目が来なければ click を実行 */
    data->pending_single = true;
    data->pending_button = button;
    data->pending_event = event;
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
