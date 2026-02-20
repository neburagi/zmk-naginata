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
#include <zmk/hid.h>
#include <zmk/keys.h>
#include <dt-bindings/zmk/keys.h>
#include <zmk_naginata/naginata_func.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_ng_off_shift_config {
    uint32_t tapping_term_ms;
    const char *caps_word_behavior_dev;
};

struct behavior_ng_off_shift_data {
    const struct device *dev;
    bool active;
    bool shift_pressed;
    bool arm_bypass_latch;
    bool caps_word_expected_active;
    bool saw_other_key_press;
    uint8_t active_other_keys_down;
    uint32_t position;
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
    uint8_t source;
#endif
};

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

static int toggle_caps_word_expected_state(const struct behavior_ng_off_shift_config *cfg,
                                           struct behavior_ng_off_shift_data *data,
                                           struct zmk_behavior_binding_event event) {
    int ret = tap_caps_word(cfg, event);
    if (ret < 0) {
        return ret;
    }

    data->caps_word_expected_active = !data->caps_word_expected_active;
    return 0;
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
        LOG_WRN("ng_off_lock already active for another position");
        return -ENOTSUP;
    }

    data->active = true;
    data->shift_pressed = false;
    // Default: single tap/alpha-only hold should keep bypass latch enabled.
    data->arm_bypass_latch = true;
    data->saw_other_key_press = false;
    data->active_other_keys_down = 0;
    data->position = event.position;
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
    data->source = event.source;
#endif

    ng_set_forced_bypass(1);
    int ret = raise_zmk_keycode_state_changed_from_encoded(LSHIFT, true, event.timestamp);
    if (ret < 0) {
        ng_set_forced_bypass(0);
        data->active = false;
        return ret;
    }

    data->shift_pressed = true;
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_ng_off_shift_released(struct zmk_behavior_binding *binding,
                                    struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_ng_off_shift_config *cfg = dev->config;
    struct behavior_ng_off_shift_data *data = dev->data;

    if (!data->active || !is_same_key_press(data, event)) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    if (data->shift_pressed) {
        raise_zmk_keycode_state_changed_from_encoded(LSHIFT, false, event.timestamp);
        data->shift_pressed = false;
    }
    ng_set_forced_bypass(0);
    if (data->arm_bypass_latch) {
        ng_arm_bypass_latch();
    }

    // tap: no other key press while held -> toggle ON/OFF
    if (!data->saw_other_key_press && data->active_other_keys_down == 0) {
        int ret = toggle_caps_word_expected_state(cfg, data, event);
        if (ret < 0) {
            LOG_WRN("caps word invocation failed: %d", ret);
        }
    // hold as shift while caps word active -> disable caps word on release
    } else if (data->caps_word_expected_active && data->saw_other_key_press &&
               data->active_other_keys_down == 0) {
        int ret = toggle_caps_word_expected_state(cfg, data, event);
        if (ret < 0) {
            LOG_WRN("caps word deactivation failed: %d", ret);
        }
    }

    data->active = false;
    data->arm_bypass_latch = false;
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
static int ng_off_shift_keycode_state_changed_listener(const zmk_event_t *eh);

static int behavior_ng_off_shift_init(const struct device *dev) {
    struct behavior_ng_off_shift_data *data = dev->data;
    data->dev = dev;
    data->arm_bypass_latch = false;
    data->caps_word_expected_active = false;
    data->saw_other_key_press = false;
    data->active_other_keys_down = 0;
    ng_set_forced_bypass(0);
    return 0;
}

ZMK_LISTENER(behavior_ng_off_shift, ng_off_shift_position_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_ng_off_shift, zmk_position_state_changed);

ZMK_LISTENER(behavior_ng_off_shift_caps_sync, ng_off_shift_keycode_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_ng_off_shift_caps_sync, zmk_keycode_state_changed);

static bool caps_word_is_alpha_usage(uint32_t usage_id) {
    return usage_id >= ZMK_HID_USAGE_ID(A) && usage_id <= ZMK_HID_USAGE_ID(Z);
}

static bool caps_word_is_numeric_usage(uint32_t usage_id) {
    return usage_id >= ZMK_HID_USAGE_ID(N1) && usage_id <= ZMK_HID_USAGE_ID(N0);
}

static bool caps_word_is_continue_usage(const struct zmk_keycode_state_changed *ev) {
    if (ev->usage_page != HID_USAGE_KEY) {
        return false;
    }

    switch (ev->keycode) {
    case ZMK_HID_USAGE_ID(BACKSPACE):
    case ZMK_HID_USAGE_ID(DELETE):
    case ZMK_HID_USAGE_ID(SPACE):
        return true;
    case ZMK_HID_USAGE_ID(MINUS): {
        zmk_mod_flags_t mods = ev->implicit_modifiers | zmk_hid_get_explicit_mods();
        return (mods & (MOD_LSFT | MOD_RSFT)) != 0;
    }
    default:
        return false;
    }
}

static int ng_off_shift_keycode_state_changed_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev == NULL || !ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    for (int i = 0; i < ARRAY_SIZE(devs); i++) {
        struct behavior_ng_off_shift_data *data = devs[i]->data;
        if (data->active && ev->usage_page == HID_USAGE_KEY) {
            bool is_alpha = caps_word_is_alpha_usage(ev->keycode);
            bool is_own_shift = ev->keycode == ZMK_HID_USAGE_ID(LSHIFT) ||
                                ev->keycode == ZMK_HID_USAGE_ID(RSHIFT);
            if (!is_alpha && !is_own_shift) {
                // If any non-letter key is pressed while held, do not latch bypass mode.
                data->arm_bypass_latch = false;
            }
        }

        if (!data->caps_word_expected_active) {
            continue;
        }

        if (caps_word_is_alpha_usage(ev->keycode) || caps_word_is_numeric_usage(ev->keycode) ||
            is_mod(ev->usage_page, ev->keycode) || caps_word_is_continue_usage(ev)) {
            continue;
        }

        // caps_word behavior auto-deactivates on these keys; keep local expectation in sync.
        data->caps_word_expected_active = false;
    }

    return ZMK_EV_EVENT_BUBBLE;
}

static int ng_off_shift_position_state_changed_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL) {
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

        if (ev->state) {
            data->saw_other_key_press = true;
            if (data->active_other_keys_down < UINT8_MAX) {
                data->active_other_keys_down++;
            }
        } else if (data->active_other_keys_down > 0) {
            data->active_other_keys_down--;
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

#define NG_OFF_SHIFT_INST(n)                                                                        \
    static struct behavior_ng_off_shift_data behavior_ng_off_shift_data_##n = {};                  \
    static const struct behavior_ng_off_shift_config behavior_ng_off_shift_config_##n = {          \
        .tapping_term_ms = DT_INST_PROP(n, tapping_term_ms),                                        \
        .caps_word_behavior_dev = DEVICE_DT_NAME(DT_INST_PHANDLE(n, caps_word_behavior)),          \
    };                                                                                               \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_ng_off_shift_init, NULL,                                    \
                            &behavior_ng_off_shift_data_##n,                                         \
                            &behavior_ng_off_shift_config_##n, POST_KERNEL,                         \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_ng_off_shift_driver_api);

DT_INST_FOREACH_STATUS_OKAY(NG_OFF_SHIFT_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
