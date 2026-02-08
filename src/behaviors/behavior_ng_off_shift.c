/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_ng_off_shift

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>
#include <dt-bindings/zmk/keys.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_ng_off_shift_config {
    uint32_t tapping_term_ms;
    const char *caps_word_behavior_dev;
};

struct behavior_ng_off_shift_data {
    const struct device *dev;
    bool active;
    bool interrupted;
    bool shift_pressed;
    uint32_t position;
    int64_t pressed_at;
    struct zmk_behavior_binding_event caps_word_event;
    struct k_work_delayable caps_word_work;
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
    uint8_t source;
#endif
};

static int tap_keycode(uint32_t keycode, int64_t timestamp) {
    int ret = raise_zmk_keycode_state_changed_from_encoded(keycode, true, timestamp);
    if (ret < 0) {
        return ret;
    }

    return raise_zmk_keycode_state_changed_from_encoded(keycode, false, timestamp);
}

static int tap_caps_word(const struct behavior_ng_off_shift_config *cfg,
                         struct zmk_behavior_binding_event event) {
    if (zmk_behavior_get_binding(cfg->caps_word_behavior_dev) == NULL) {
        return -ENODEV;
    }

    struct zmk_behavior_binding caps_word_binding = {.behavior_dev = cfg->caps_word_behavior_dev,
                                                     .param1 = 0,
                                                     .param2 = 0};

    int ret = zmk_behavior_invoke_binding(&caps_word_binding, event, true);
    if (ret < 0) {
        return ret;
    }

    return zmk_behavior_invoke_binding(&caps_word_binding, event, false);
}

static void ng_off_shift_caps_word_work_handler(struct k_work *item) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(item);
    struct behavior_ng_off_shift_data *data =
        CONTAINER_OF(dwork, struct behavior_ng_off_shift_data, caps_word_work);
    const struct behavior_ng_off_shift_config *cfg = data->dev->config;

    int ret = tap_caps_word(cfg, data->caps_word_event);
    if (ret < 0) {
        LOG_WRN("caps word invocation failed: %d", ret);
    }
}

static bool is_same_key_press(const struct behavior_ng_off_shift_data *data,
                              struct zmk_behavior_binding_event event) {
    if (data->position != event.position) {
        return false;
    }

#if IS_ENABLED(CONFIG_ZMK_SPLIT)
    if (data->source != event.source) {
        return false;
    }
#endif

    return true;
}

static int on_ng_off_shift_pressed(struct zmk_behavior_binding *binding,
                                   struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_ng_off_shift_data *data = dev->data;

    if (data->active) {
        if (is_same_key_press(data, event)) {
            return ZMK_BEHAVIOR_OPAQUE;
        }
        LOG_WRN("ng-off-shift already active for another position");
        return -ENOTSUP;
    }

    data->active = true;
    data->interrupted = false;
    data->shift_pressed = false;
    data->position = event.position;
    data->pressed_at = event.timestamp;
    k_work_cancel_delayable(&data->caps_word_work);
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
    data->source = event.source;
#endif

    tap_keycode(LANGUAGE_2, event.timestamp);
    tap_keycode(INTERNATIONAL_5, event.timestamp);
    zmk_keymap_layer_to(0);

    int ret = raise_zmk_keycode_state_changed_from_encoded(LSHIFT, true, event.timestamp);
    if (ret < 0) {
        data->active = false;
        return ret;
    }

    data->shift_pressed = true;
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_ng_off_shift_released(struct zmk_behavior_binding *binding,
                                    struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_ng_off_shift_data *data = dev->data;

    if (!data->active || !is_same_key_press(data, event)) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    if (data->shift_pressed) {
        raise_zmk_keycode_state_changed_from_encoded(LSHIFT, false, event.timestamp);
        data->shift_pressed = false;
    }

    if (!data->interrupted) {
        data->caps_word_event = event;
        k_work_reschedule(&data->caps_word_work, K_MSEC(1));
    }

    data->active = false;
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_ng_off_shift_driver_api = {
    .binding_pressed = on_ng_off_shift_pressed,
    .binding_released = on_ng_off_shift_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

#define GET_DEV(inst) DEVICE_DT_INST_GET(inst),
static const struct device *devs[] = {DT_INST_FOREACH_STATUS_OKAY(GET_DEV)};

static int ng_off_shift_position_state_changed_listener(const zmk_event_t *eh);

static int behavior_ng_off_shift_init(const struct device *dev) {
    struct behavior_ng_off_shift_data *data = dev->data;
    data->dev = dev;
    k_work_init_delayable(&data->caps_word_work, ng_off_shift_caps_word_work_handler);
    return 0;
}

ZMK_LISTENER(behavior_ng_off_shift, ng_off_shift_position_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_ng_off_shift, zmk_position_state_changed);

static int ng_off_shift_position_state_changed_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL || !ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    for (int i = 0; i < ARRAY_SIZE(devs); i++) {
        struct behavior_ng_off_shift_data *data = devs[i]->data;
        if (!data->active) {
            continue;
        }

        if (data->position == ev->position
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
            && data->source == ev->source
#endif
        ) {
            continue;
        }

        data->interrupted = true;
    }

    return ZMK_EV_EVENT_BUBBLE;
}

#define NG_OFF_SHIFT_INST(n)                                                                        \
    static struct behavior_ng_off_shift_data behavior_ng_off_shift_data_##n = {};                  \
    static const struct behavior_ng_off_shift_config behavior_ng_off_shift_config_##n = {           \
        .tapping_term_ms = DT_INST_PROP(n, tapping_term_ms),                                       \
        .caps_word_behavior_dev = DEVICE_DT_NAME(DT_INST_PHANDLE(n, caps_word_behavior)),           \
    };                                                                                               \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_ng_off_shift_init, NULL,                                    \
                            &behavior_ng_off_shift_data_##n,                                         \
                            &behavior_ng_off_shift_config_##n, POST_KERNEL,                         \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_ng_off_shift_driver_api);

DT_INST_FOREACH_STATUS_OKAY(NG_OFF_SHIFT_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
