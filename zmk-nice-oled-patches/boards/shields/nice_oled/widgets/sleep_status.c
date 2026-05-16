/*
 * Copyright (c) 2020 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * PATCH: panel_editions_ssd1306_fix
 *
 * Root-cause fix: removed lv_obj_create / lv_img_create LVGL objects.
 * The sleep status widget now only tracks activity state in a static
 * variable.  draw_canvas() in screen.c / screen_peripheral.c reads it
 * via zmk_widget_sleep_status_get_activity() and blits vim_32x128 into
 * the canvas buffer before rotate_canvas().
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>
#include <zmk/events/activity_state_changed.h>
#include "sleep_status.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/* Module-level activity state, read by draw_canvas(). */
static zmk_activity_state_t current_activity = ZMK_ACTIVITY_ACTIVE;

zmk_activity_state_t zmk_widget_sleep_status_get_activity(void)
{
    return current_activity;
}

static void set_sleep_state(struct zmk_widget_sleep_status *widget,
                            struct zmk_activity_state_changed ev)
{
    (void)widget;
    current_activity = ev.state;
    LOG_DBG("sleep_status: activity → %d", (int)ev.state);
    /* No LVGL show/hide: the canvas redraw triggered by other events
     * (or by the animation timer) will pick up the new state.        */
}

static void sleep_status_update_cb(struct zmk_activity_state_changed ev)
{
    struct zmk_widget_sleep_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_sleep_state(widget, ev);
    }
}

static struct zmk_activity_state_changed sleep_status_get_state(const zmk_event_t *eh)
{
    struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    return *ev;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_sleep_status, struct zmk_activity_state_changed,
                            sleep_status_update_cb, sleep_status_get_state)

ZMK_SUBSCRIPTION(widget_sleep_status, zmk_activity_state_changed);

int zmk_widget_sleep_status_init(struct zmk_widget_sleep_status *widget,
                                 lv_obj_t *parent)
{
    (void)parent;   /* No LVGL objects created */
    sys_slist_append(&widgets, &widget->node);
    widget_sleep_status_init();
    return 0;
}
