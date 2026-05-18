/*
 * Copyright (c) 2024 eyelash_sofle contributors
 * SPDX-License-Identifier: MIT
 *
 * Panel selector — thin wrapper that delegates to the panel chosen by
 * CONFIG_EYELASH_PANEL in your .conf file (config/eyelash_sofle.conf).
 *
 * Available panels:
 *   CONFIG_EYELASH_PANEL_PLACEHOLDER=y   Full-screen animation + battery (default)
 *   CONFIG_EYELASH_PANEL_LAYER_BATTERY=y Layer name + battery, no assets needed
 *
 * To add a new panel:
 *   1. Create boards/shields/eyelash_sofle_animation/panels/<name>/panel.[ch]
 *   2. Implement  lv_obj_t *eyelash_panel_screen(void)  in panel.c
 *   3. Add a new  config EYELASH_PANEL_<NAME>  entry in Kconfig.defconfig
 *   4. Add the corresponding  zephyr_library_sources()  call in CMakeLists.txt
 *   5. Add the matching  #include  branch below
 */

#include <lvgl.h>
#include <zmk/display.h>

#if defined(CONFIG_EYELASH_PANEL_PLACEHOLDER)
#  include "panels/placeholder/panel.h"
#elif defined(CONFIG_EYELASH_PANEL_LAYER_BATTERY)
#  include "panels/layer_battery/panel.h"
#else
#  error "No EYELASH_PANEL selected. Set CONFIG_EYELASH_PANEL_PLACEHOLDER=y (or another panel) in config/eyelash_sofle.conf"
#endif

lv_obj_t *zmk_display_status_screen(void) {
    return eyelash_panel_screen();
}
