/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * PATCH: panel_editions_ssd1306_fix
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zmk/events/activity_state_changed.h>

struct zmk_widget_sleep_status {
    sys_snode_t node;
    /* obj and art removed: no LVGL objects created.
     * Activity state is tracked in a module-level variable and
     * consumed by draw_canvas() before rotate_canvas().           */
};

int zmk_widget_sleep_status_init(struct zmk_widget_sleep_status *widget,
                                 lv_obj_t *parent);

/* Return the most recently received activity state.
 * ZMK_ACTIVITY_ACTIVE / ZMK_ACTIVITY_IDLE / ZMK_ACTIVITY_SLEEP.   */
zmk_activity_state_t zmk_widget_sleep_status_get_activity(void);
