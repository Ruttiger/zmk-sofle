/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * PATCH: panel_editions_ssd1306_fix
 *
 * zmk_widget_screen_all_redraw() is added so that luna.c and animation.c can
 * trigger a canvas redraw from their lv_anim_t timer callbacks without needing
 * a back-reference to the widget instance.
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>
#include "util.h"

struct zmk_widget_screen {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_color_t cbuf[CANVAS_HEIGHT * CANVAS_HEIGHT];
    struct status_state state;
};

int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget);

/* Redraw all registered screen widgets (called from animation / luna timers). */
void zmk_widget_screen_all_redraw(void);
