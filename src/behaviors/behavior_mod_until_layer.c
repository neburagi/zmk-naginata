/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_mod_until_layer

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#include <dt-bindings/zmk/keys.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_mod_until_layer_data {
    bool active;
    uint32_t keycode;
    zmk_keymap_layer_id_t layer;
    uint32_t position;
    bool tab_pressed;
};

static int on_mod_until_layer_pressed(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_mod_until_layer_data *data = dev->data;

    if (data->active) {
        if (data->position == event.position) {
            if (data->tab_pressed) {
                return ZMK_BEHAVIOR_OPAQUE;
            }

            int ret = raise_zmk_keycode_state_changed_from_encoded(TAB, true, event.timestamp);
            if (ret < 0) {
                return ret;
            }
            data->tab_pressed = true;
            return ZMK_BEHAVIOR_OPAQUE;
        }
        LOG_WRN("Mod-until-layer already active for another position");
        return -ENOTSUP;
    }

    data->active = true;
    data->keycode = binding->param1;
    data->layer = binding->param2;
    data->position = event.position;
    data->tab_pressed = false;

    int ret = raise_zmk_keycode_state_changed_from_encoded(binding->param1, true, event.timestamp);
    if (ret < 0) {
        data->active = false;
        return ret;
    }

    ret = raise_zmk_keycode_state_changed_from_encoded(TAB, true, event.timestamp);
    if (ret < 0) {
        raise_zmk_keycode_state_changed_from_encoded(binding->param1, false, event.timestamp);
        data->active = false;
        return ret;
    }

    data->tab_pressed = true;
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_mod_until_layer_released(struct zmk_behavior_binding *binding,
                                       struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_mod_until_layer_data *data = dev->data;

    if (!data->active || data->position != event.position) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    if (data->tab_pressed) {
        raise_zmk_keycode_state_changed_from_encoded(TAB, false, event.timestamp);
        data->tab_pressed = false;
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_mod_until_layer_driver_api = {
    .binding_pressed = on_mod_until_layer_pressed,
    .binding_released = on_mod_until_layer_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

#define MOD_UNTIL_LAYER_INST(n)                                                                    \
    static struct behavior_mod_until_layer_data behavior_mod_until_layer_data_##n = {};            \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, &behavior_mod_until_layer_data_##n, NULL, POST_KERNEL,  \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                  \
                            &behavior_mod_until_layer_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MOD_UNTIL_LAYER_INST)

static void release_mod_if_needed(const struct device *dev, int64_t timestamp) {
    struct behavior_mod_until_layer_data *data = dev->data;

    if (!data->active) {
        return;
    }

    if (zmk_keymap_layer_active(data->layer)) {
        return;
    }

    if (data->tab_pressed) {
        raise_zmk_keycode_state_changed_from_encoded(TAB, false, timestamp);
        data->tab_pressed = false;
    }

    raise_zmk_keycode_state_changed_from_encoded(data->keycode, false, timestamp);
    data->active = false;
}

#define MOD_UNTIL_LAYER_RELEASE(n) release_mod_if_needed(DEVICE_DT_INST_GET(n), ev->timestamp);

static int mod_until_layer_listener(const zmk_event_t *eh) {
    const struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    DT_INST_FOREACH_STATUS_OKAY(MOD_UNTIL_LAYER_RELEASE)

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(behavior_mod_until_layer, mod_until_layer_listener);
ZMK_SUBSCRIPTION(behavior_mod_until_layer, zmk_layer_state_changed);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
