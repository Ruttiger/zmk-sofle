/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * PATCH: panel_editions_ssd1306_fix
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_luna {
    sys_snode_t node;
    /* obj removed: no lv_animimg LVGL object is created.
     * Frame cycling is driven by an lv_anim_t timer.
     * The current frame is retrieved via zmk_widget_luna_get_current_frame(). */
};

int zmk_widget_luna_init(struct zmk_widget_luna *widget, lv_obj_t *parent);

/* Return the image descriptor for the current Luna animation frame.
 * Call this from draw_canvas() before rotate_canvas() and blit via
 * lv_canvas_draw_img().  Returns NULL until the WPM listener fires once. */
const lv_img_dsc_t *zmk_widget_luna_get_current_frame(void);
