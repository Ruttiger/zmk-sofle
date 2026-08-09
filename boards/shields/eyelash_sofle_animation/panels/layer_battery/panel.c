/*
 * Panel: layer_battery
 *
 * SPDX-License-Identifier: MIT
 *
 * Displays the active layer name in the centre of the screen and the battery
 * percentage at the bottom. No static animation assets are required, so this
 * panel compiles quickly and is useful for testing display connectivity.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <lvgl.h>
#include <zmk/display.h>
#include <zmk/display/widgets/battery_status.h>
#include <zmk/display/widgets/layer_status.h>

#include "panel.h"

static struct zmk_widget_battery_status battery_widget;
static struct zmk_widget_layer_status   layer_widget;

lv_obj_t *eyelash_panel_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);

    /* Active layer name — centred, slightly above middle */
    zmk_widget_layer_status_init(&layer_widget, screen);
    lv_obj_align(zmk_widget_layer_status_obj(&layer_widget),
                 LV_ALIGN_CENTER, 0, -8);

    /* Battery percentage — bottom centre */
    zmk_widget_battery_status_init(&battery_widget, screen);
    lv_obj_align(zmk_widget_battery_status_obj(&battery_widget),
                 LV_ALIGN_BOTTOM_MID, 0, -4);

    return screen;
}
