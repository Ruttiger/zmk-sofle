/*
 * Minimal physical-resolution diagnostic for the SSD1306 display.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/devicetree.h>
#include <lvgl.h>

#define DISPLAY_NODE DT_CHOSEN(zephyr_display)
#define DISPLAY_WIDTH DT_PROP(DISPLAY_NODE, width)
#define DISPLAY_HEIGHT DT_PROP(DISPLAY_NODE, height)

static void diagnostic_rect(lv_obj_t *parent, int32_t x, int32_t y,
                            int32_t width, int32_t height) {
    lv_obj_t *rect = lv_obj_create(parent);

    lv_obj_set_pos(rect, x, y);
    lv_obj_set_size(rect, width, height);
    lv_obj_set_style_radius(rect, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(rect, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(rect, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(rect, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(rect, LV_OPA_COVER, LV_PART_MAIN);
}

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    lv_obj_set_size(screen, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    /* Complete two-pixel border. */
    diagnostic_rect(screen, 0, 0, DISPLAY_WIDTH, 2);
    diagnostic_rect(screen, 0, DISPLAY_HEIGHT - 2, DISPLAY_WIDTH, 2);
    diagnostic_rect(screen, 0, 0, 2, DISPLAY_HEIGHT);
    diagnostic_rect(screen, DISPLAY_WIDTH - 2, 0, 2, DISPLAY_HEIGHT);

    /* Physical midpoint divisions. */
    diagnostic_rect(screen, DISPLAY_WIDTH / 2, 2, 1, DISPLAY_HEIGHT - 4);
    diagnostic_rect(screen, 2, DISPLAY_HEIGHT / 2, DISPLAY_WIDTH - 4, 1);

    /* Top-left: solid 6x6 square. */
    diagnostic_rect(screen, 5, 5, 6, 6);

    /* Top-right: two unequal bars. */
    diagnostic_rect(screen, DISPLAY_WIDTH - 19, 5, 5, 5);
    diagnostic_rect(screen, DISPLAY_WIDTH - 10, 5, 5, 9);

    /* Bottom-left: three bars with distinct lengths. */
    diagnostic_rect(screen, 5, DISPLAY_HEIGHT - 16, 15, 2);
    diagnostic_rect(screen, 5, DISPLAY_HEIGHT - 11, 10, 2);
    diagnostic_rect(screen, 5, DISPLAY_HEIGHT - 6, 5, 2);

    /* Bottom-right: asymmetric L made from two rectangles. */
    diagnostic_rect(screen, DISPLAY_WIDTH - 20, DISPLAY_HEIGHT - 17, 3, 13);
    diagnostic_rect(screen, DISPLAY_WIDTH - 20, DISPLAY_HEIGHT - 7, 14, 3);

    return screen;
}
