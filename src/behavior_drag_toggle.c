/*
 * SPDX-License-Identifier: MIT
 */
#define DT_DRV_COMPAT zmk_behavior_drag_toggle

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/devicetree.h>
#include <drivers/behavior.h>

#include <zmk/behavior.h>

#ifndef DT_TAP_TERM_MS
#define DT_TAP_TERM_MS 250 /* ダブルタップ判定(ms) */
#endif

struct drag_toggle_data {
    bool locked;                 /* drag lock中か */
    bool pending_second;         /* 2回目待ち（窓オープン中） */
    bool ignore_next_release;    /* ロックON/OFF直後のreleaseを無視 */
    uint32_t pending_button;     /* MB1等 */
    struct k_work_delayable window_work; /* ダブルタップ窓を閉じるだけ */
};

/* &mkp を呼ぶ：behavior_dev は “文字列” が安全 */
static inline void invoke_mkp(uint32_t button,
                              struct zmk_behavior_binding_event event,
                              bool pressed) {
    struct zmk_behavior_binding mkp_binding = {
        .behavior_dev = DT_LABEL(DT_NODELABEL(mkp)),
        .param1 = button,
        .param2 = 0,
    };
    (void)zmk_behavior_invoke_binding(&mkp_binding, event, pressed);
}

static void close_window_work(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct drag_toggle_data *data =
        CONTAINER_OF(dwork, struct drag_toggle_data, window_work);

    data->pending_second = false;
    data->pending_button = 0;
}

/* data 取得（behavior インスタンスの data を取る） */
static inline struct drag_toggle_data *get_data_from_binding(struct zmk_behavior_binding *binding) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    return (struct drag_toggle_data *)dev->data;
}

static int drag_toggle_pressed(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
    uint32_t button = binding->param1;
    struct drag_toggle_data *data = get_data_from_binding(binding);

    /* ロック中：次の押下で解除（release送る） */
    if (data->locked) {
        invoke_mkp(button, event, false);
        data->locked = false;

        data->ignore_next_release = true;     /* この押下のreleaseはクリック化しない */
        data->pending_second = false;
        k_work_cancel_delayable(&data->window_work);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* 2回目が窓内＆同じボタンなら：ロックON（press保持） */
    if (data->pending_second && data->pending_button == button) {
        data->pending_second = false;
        k_work_cancel_delayable(&data->window_work);

        invoke_mkp(button, event, true);      /* press保持 */
        data->locked = true;

        data->ignore_next_release = true;     /* この押下のreleaseは無視（保持したいので） */
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* 1回目：ダブルタップ窓を開く（クリックはreleaseで即やる） */
    data->pending_second = true;
    data->pending_button = button;
    k_work_reschedule(&data->window_work, K_MSEC(DT_TAP_TERM_MS));

    return ZMK_BEHAVIOR_OPAQUE;
}

static int drag_toggle_released(struct zmk_behavior_binding *binding,
                                struct zmk_behavior_binding_event event) {
    uint32_t button = binding->param1;
    struct drag_toggle_data *data = get_data_from_binding(binding);

    /* ロックON/OFF直後のreleaseは無視（クリック化しない） */
    if (data->ignore_next_release) {
        data->ignore_next_release = false;
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* 非ロック時：単押しクリック（離した瞬間に click） */
    if (!data->locked) {
        invoke_mkp(button, event, true);
        invoke_mkp(button, event, false);
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
    data->pending_second = false;
    data->ignore_next_release = false;
    data->pending_button = 0;
    k_work_init_delayable(&data->window_work, close_window_work);
    return 0;
}

/* ZMK behavior として登録する */
#define DRAG_TOGGLE_DEVICE(inst)                                                   \
    static struct drag_toggle_data drag_toggle_data_##inst;                         \
    BEHAVIOR_DT_INST_DEFINE(inst, drag_toggle_init, NULL,                           \
                            &drag_toggle_data_##inst, NULL,                         \
                            APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,       \
                            &drag_toggle_api);

DT_INST_FOREACH_STATUS_OKAY(DRAG_TOGGLE_DEVICE)
