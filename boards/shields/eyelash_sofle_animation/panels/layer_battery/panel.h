/*
 * Panel: layer_battery
 * Shows active layer name (centre) and battery status (bottom).
 * No animation assets required — ideal for testing or minimal builds.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <lvgl.h>

/** Create and return the LVGL screen object for this panel. */
lv_obj_t *eyelash_panel_screen(void);
