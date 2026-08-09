/*
 * Panel: placeholder — full-screen frame animation + battery widget.
 *
 * SPDX-License-Identifier: MIT
 *
 * To swap the placeholder animation for a real GIF:
 *   1. Run: python3 scripts/gif-convert.py your.gif --no-register -W 160 -H 68
 *   2. Drop the generated .c/.h pair into assets/
 *   3. Update CMakeLists.txt: replace assets/placeholder.c with assets/<name>.c
 *   4. Swap the two marked lines below to point at your new header/array.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <lvgl.h>
#include <zmk/display.h>
#include <zmk/display/widgets/battery_status.h>

#include "panel.h"

/* ---- SWAP THESE TWO LINES when you replace the animation asset ---- */
#include "../../assets/placeholder.h"
#define ANIM_FRAMES     placeholder_images
#define ANIM_NUM_FRAMES PLACEHOLDER_IMAGES_NUM
/* ------------------------------------------------------------------- */

static struct zmk_widget_battery_status battery_widget;

lv_obj_t *eyelash_panel_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    /* Remove default LVGL padding/border so the image fills edge-to-edge */
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);

    /* --- Full-screen animation --- */
    lv_obj_t *anim_img = lv_animimg_create(screen);
    lv_obj_set_size(anim_img, 160, 68);
    lv_obj_align(anim_img, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_animimg_set_src(anim_img, (const lv_img_dsc_t **)ANIM_FRAMES, ANIM_NUM_FRAMES);
    lv_animimg_set_duration(anim_img, CONFIG_EYELASH_ANIM_MS);
    lv_animimg_set_repeat_count(anim_img, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(anim_img);

    /* --- Battery label (top-right, uses ZMK's built-in battery widget) --- */
    zmk_widget_battery_status_init(&battery_widget, screen);
    lv_obj_align(zmk_widget_battery_status_obj(&battery_widget),
                 LV_ALIGN_TOP_RIGHT, -2, 2);

    return screen;
}
