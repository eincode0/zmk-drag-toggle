/*
 * SPDX-License-Identifier: MIT
 */
/*
 * SPDX-License-Identifier: MIT
 */
#define DT_DRV_COMPAT zmk_behavior_drag_toggle

#include <zephyr/kernel.h>
#include <zephyr/device.h>

static int drag_toggle_pressed(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
    (void)event;

    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct drag_toggle_data *data = (struct drag_toggle_data *)dev->data;

    uint32_t button = binding->param1;

    if (!data->locked) {
        zmk_hid_mouse_button_press(button);
        data->locked = true;
    } else {
        zmk_hid_mouse_button_release(button);
        data->locked = false;
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

struct drag_toggle_data {
    bool locked;               // ドラッグロック中か
    int64_t last_tap_ms;       // 前回タップ時刻
    bool pending_lock;         // 次のreleaseでロックに入る（=ダブルタップ成立）
};

static int drag_toggle_pressed(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
    (void)event;

    const struct device *dev = binding->behavior_dev;
    struct drag_toggle_data *data = dev->data;

    uint32_t button = binding->param1;

    /* ロック中なら、この押下で解除する（1回押すだけでOFF） */
    if (data->locked) {
        zmk_mouse_button_release(button);
        data->locked = false;
        data->pending_lock = false;
        data->last_tap_ms = 0;
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* 通常は押下＝press */
    zmk_mouse_button_press(button);

    /* ダブルタップ判定：前回タップから一定時間以内なら “releaseでロック” */
    const int64_t now = k_uptime_get();
    const int64_t term = 250; /* ダブルタップ猶予(ms) 好みで200-350 */

    if (data->last_tap_ms != 0 && (now - data->last_tap_ms) <= term) {
        data->pending_lock = true;
        data->last_tap_ms = 0; /* 連続判定をここで消す */
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int drag_toggle_released(struct zmk_behavior_binding *binding,
                                struct zmk_behavior_binding_event event) {
    (void)event;

    const struct device *dev = binding->behavior_dev;
    struct drag_toggle_data *data = dev->data;

    uint32_t button = binding->param1;

    /* ロック予定なら release を抑止してロックON（押しっぱなし維持） */
    if (data->pending_lock) {
        data->pending_lock = false;
        data->locked = true;
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* 通常クリック：release */
    zmk_mouse_button_release(button);

    /* 次のダブルタップ判定のために時刻を記録 */
    data->last_tap_ms = k_uptime_get();

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api drag_toggle_api = {
    .binding_pressed = drag_toggle_pressed,
    .binding_released = drag_toggle_released,
};

static int drag_toggle_init(const struct device *dev) {
    struct drag_toggle_data *data = dev->data;
    data->locked = false;
    data->last_tap_ms = 0;
    data->pending_lock = false;
    return 0;
}

#define DRAG_TOGGLE_DEVICE(inst)                                                     \
    static struct drag_toggle_data drag_toggle_data_##inst;                           \
    DEVICE_DT_INST_DEFINE(inst, drag_toggle_init, NULL,                               \
                          &drag_toggle_data_##inst, NULL,                             \
                          APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,           \
                          &drag_toggle_api);

DT_INST_FOREACH_STATUS_OKAY(DRAG_TOGGLE_DEVICE)
