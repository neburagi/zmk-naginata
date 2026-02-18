/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_to_layer_press

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <zmk/behavior.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/matrix.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_to_layer_press_data {
    bool forwarding;
    bool forwarded_positions[ZMK_KEYMAP_LEN];
};

static uint8_t behavior_event_source(const struct zmk_behavior_binding_event *event) {
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
    return event->source;
#else
    ARG_UNUSED(event);
    return ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL;
#endif
}

static int forward_position_state(struct behavior_to_layer_press_data *data,
                                  const struct zmk_behavior_binding_event *event, bool pressed) {
    if (data->forwarding) {
        return 0;
    }

    data->forwarding = true;
    int ret = zmk_keymap_position_state_changed(behavior_event_source(event), event->position,
                                                pressed, event->timestamp);
    data->forwarding = false;
    return ret;
}

static int on_to_layer_press_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_to_layer_press_data *data = dev->data;

    if (event.position >= ZMK_KEYMAP_LEN) {
        LOG_WRN("position %u out of range", event.position);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    if (data->forwarding) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    int ret = zmk_keymap_layer_to(binding->param1);
    if (ret < 0) {
        return ret;
    }

    ret = forward_position_state(data, &event, true);
    if (ret < 0) {
        return ret;
    }

    data->forwarded_positions[event.position] = true;
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_to_layer_press_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_to_layer_press_data *data = dev->data;

    ARG_UNUSED(binding);

    if (event.position >= ZMK_KEYMAP_LEN) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    if (data->forwarding || !data->forwarded_positions[event.position]) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    data->forwarded_positions[event.position] = false;
    int ret = forward_position_state(data, &event, false);
    if (ret < 0) {
        return ret;
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata param_values[] = {
    {
        .display_name = "Layer",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_LAYER_ID,
    },
};

static const struct behavior_parameter_metadata_set param_metadata_set[] = {{
    .param1_values = param_values,
    .param1_values_len = ARRAY_SIZE(param_values),
}};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = ARRAY_SIZE(param_metadata_set),
    .sets = param_metadata_set,
};

#endif

static const struct behavior_driver_api behavior_to_layer_press_driver_api = {
    .binding_pressed = on_to_layer_press_pressed,
    .binding_released = on_to_layer_press_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

#define TO_LAYER_PRESS_INST(n)                                                                     \
    static struct behavior_to_layer_press_data behavior_to_layer_press_data_##n = {};             \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, &behavior_to_layer_press_data_##n, NULL, POST_KERNEL,  \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_to_layer_press_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TO_LAYER_PRESS_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
