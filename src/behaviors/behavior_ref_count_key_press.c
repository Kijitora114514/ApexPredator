/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_ref_count_key_press

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/events/keycode_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define REF_COUNT_KEY_MAX_ACTIVE 32

struct active_key {
    uint32_t keycode;
    uint32_t position;
    bool pressed;
};

static struct active_key active_keys[REF_COUNT_KEY_MAX_ACTIVE];
static struct k_mutex active_keys_lock;

static uint8_t active_count_for_keycode(uint32_t keycode) {
    uint8_t count = 0;

    for (int i = 0; i < REF_COUNT_KEY_MAX_ACTIVE; i++) {
        if (active_keys[i].pressed && active_keys[i].keycode == keycode) {
            count++;
        }
    }

    return count;
}

static int find_active_slot(uint32_t keycode, uint32_t position) {
    for (int i = 0; i < REF_COUNT_KEY_MAX_ACTIVE; i++) {
        if (active_keys[i].pressed && active_keys[i].keycode == keycode &&
            active_keys[i].position == position) {
            return i;
        }
    }

    return -1;
}

static int find_free_slot(void) {
    for (int i = 0; i < REF_COUNT_KEY_MAX_ACTIVE; i++) {
        if (!active_keys[i].pressed) {
            return i;
        }
    }

    return -1;
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const uint32_t keycode = binding->param1;
    bool should_press = false;
    int err = 0;

    k_mutex_lock(&active_keys_lock, K_FOREVER);

    if (find_active_slot(keycode, event.position) >= 0) {
        k_mutex_unlock(&active_keys_lock);
        return 0;
    }

    const uint8_t active_count = active_count_for_keycode(keycode);
    const int slot = find_free_slot();

    if (slot < 0) {
        err = -ENOMEM;
    } else {
        active_keys[slot].keycode = keycode;
        active_keys[slot].position = event.position;
        active_keys[slot].pressed = true;
        should_press = active_count == 0;
    }

    k_mutex_unlock(&active_keys_lock);

    if (err < 0) {
        LOG_ERR("No free ref-count key slots for keycode 0x%02X", keycode);
        return err;
    }

    if (!should_press) {
        return 0;
    }

    LOG_DBG("press keycode 0x%02X", keycode);
    return raise_zmk_keycode_state_changed_from_encoded(keycode, true, event.timestamp);
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    const uint32_t keycode = binding->param1;
    bool should_release = false;

    k_mutex_lock(&active_keys_lock, K_FOREVER);

    const int slot = find_active_slot(keycode, event.position);
    if (slot < 0) {
        k_mutex_unlock(&active_keys_lock);
        return 0;
    }

    active_keys[slot].pressed = false;
    should_release = active_count_for_keycode(keycode) == 0;

    k_mutex_unlock(&active_keys_lock);

    if (!should_release) {
        return 0;
    }

    LOG_DBG("release keycode 0x%02X", keycode);
    return raise_zmk_keycode_state_changed_from_encoded(keycode, false, event.timestamp);
}

static int behavior_ref_count_key_press_init(const struct device *dev) {
    ARG_UNUSED(dev);

    k_mutex_init(&active_keys_lock);
    return 0;
}

static const struct behavior_driver_api behavior_ref_count_key_press_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

#define REF_COUNT_KEY_PRESS_INST(n)                                                                \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_ref_count_key_press_init, NULL, NULL, NULL, POST_KERNEL,   \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                    \
                            &behavior_ref_count_key_press_driver_api);

DT_INST_FOREACH_STATUS_OKAY(REF_COUNT_KEY_PRESS_INST)
