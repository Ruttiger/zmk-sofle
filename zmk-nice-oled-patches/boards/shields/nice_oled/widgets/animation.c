/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * PATCH: panel_editions_ssd1306_fix
 *
 * Root-cause fix for animation rendering on SSD1306 128x64 OLED.
 *
 * Original code created an lv_animimg as an LVGL child of the canvas object.
 * LVGL renders that child in canvas-space (unrotated) while the canvas buffer
 * already contains 90°-rotated content, so the animation and the text widgets
 * lived in different coordinate systems.
 *
 * Fix: track the current frame index with a plain lv_anim_t (no LVGL object
 * is created).  On every frame change zmk_widget_screen_all_redraw() is called
 * so draw_canvas() blits the new frame via lv_canvas_draw_img() BEFORE
 * rotate_canvas().  All content now passes through the same rotation.
 */

#include "animation.h"

#if !IS_ENABLED(CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_SMART_BATTERY)
#include <stdlib.h>
#include <zephyr/kernel.h>

/* Forward declaration – implemented in screen_peripheral.c */
extern void zmk_widget_screen_all_redraw(void);

/* ------------------------------------------------------------------ */
/*  Crystal frames (always compiled as fallback)                        */
/* ------------------------------------------------------------------ */
LV_IMG_DECLARE(crystal_01);
LV_IMG_DECLARE(crystal_02);
LV_IMG_DECLARE(crystal_03);
LV_IMG_DECLARE(crystal_04);
LV_IMG_DECLARE(crystal_05);
LV_IMG_DECLARE(crystal_06);
LV_IMG_DECLARE(crystal_07);
LV_IMG_DECLARE(crystal_08);
LV_IMG_DECLARE(crystal_09);
LV_IMG_DECLARE(crystal_10);
LV_IMG_DECLARE(crystal_11);
LV_IMG_DECLARE(crystal_12);
LV_IMG_DECLARE(crystal_13);
LV_IMG_DECLARE(crystal_14);
LV_IMG_DECLARE(crystal_15);
LV_IMG_DECLARE(crystal_16);

static const lv_img_dsc_t *crystal_imgs[] = {
    &crystal_01, &crystal_02, &crystal_03, &crystal_04,
    &crystal_05, &crystal_06, &crystal_07, &crystal_08,
    &crystal_09, &crystal_10, &crystal_11, &crystal_12,
    &crystal_13, &crystal_14, &crystal_15, &crystal_16,
};

/* ------------------------------------------------------------------ */
/*  Animation / static-image frame arrays                              */
/* ------------------------------------------------------------------ */
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL)

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_HEAD)
LV_IMG_DECLARE(head_00); LV_IMG_DECLARE(head_01); LV_IMG_DECLARE(head_02);
LV_IMG_DECLARE(head_03); LV_IMG_DECLARE(head_04); LV_IMG_DECLARE(head_05);
LV_IMG_DECLARE(head_06); LV_IMG_DECLARE(head_07); LV_IMG_DECLARE(head_08);
LV_IMG_DECLARE(head_09); LV_IMG_DECLARE(head_10); LV_IMG_DECLARE(head_11);
LV_IMG_DECLARE(head_12); LV_IMG_DECLARE(head_13); LV_IMG_DECLARE(head_14);
LV_IMG_DECLARE(head_15);
static const lv_img_dsc_t *head_imgs[] = {
    &head_00, &head_01, &head_02, &head_03, &head_04, &head_05,
    &head_06, &head_07, &head_08, &head_09, &head_10, &head_11,
    &head_12, &head_13, &head_14, &head_15,
};

#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_CAT)
LV_IMG_DECLARE(cat_0); LV_IMG_DECLARE(cat_1); LV_IMG_DECLARE(cat_2);
LV_IMG_DECLARE(cat_3); LV_IMG_DECLARE(cat_4); LV_IMG_DECLARE(cat_5);
LV_IMG_DECLARE(cat_6); LV_IMG_DECLARE(cat_7);
static const lv_img_dsc_t *cat_imgs[] = {
    &cat_0, &cat_1, &cat_2, &cat_3, &cat_4, &cat_5, &cat_6, &cat_7,
};

#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_SPACEMAN)
LV_IMG_DECLARE(spaceman_00); LV_IMG_DECLARE(spaceman_01); LV_IMG_DECLARE(spaceman_02);
LV_IMG_DECLARE(spaceman_03); LV_IMG_DECLARE(spaceman_04); LV_IMG_DECLARE(spaceman_05);
LV_IMG_DECLARE(spaceman_06); LV_IMG_DECLARE(spaceman_07); LV_IMG_DECLARE(spaceman_08);
LV_IMG_DECLARE(spaceman_09); LV_IMG_DECLARE(spaceman_10); LV_IMG_DECLARE(spaceman_11);
LV_IMG_DECLARE(spaceman_12); LV_IMG_DECLARE(spaceman_13); LV_IMG_DECLARE(spaceman_14);
LV_IMG_DECLARE(spaceman_15); LV_IMG_DECLARE(spaceman_16); LV_IMG_DECLARE(spaceman_17);
LV_IMG_DECLARE(spaceman_18); LV_IMG_DECLARE(spaceman_19);
static const lv_img_dsc_t *spaceman_imgs[] = {
    &spaceman_00, &spaceman_01, &spaceman_02, &spaceman_03, &spaceman_04,
    &spaceman_05, &spaceman_06, &spaceman_07, &spaceman_08, &spaceman_09,
    &spaceman_10, &spaceman_11, &spaceman_12, &spaceman_13, &spaceman_14,
    &spaceman_15, &spaceman_16, &spaceman_17, &spaceman_18, &spaceman_19,
};

#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_POKEMON)
LV_IMG_DECLARE(pokemon_00); LV_IMG_DECLARE(pokemon_01); LV_IMG_DECLARE(pokemon_02);
LV_IMG_DECLARE(pokemon_03); LV_IMG_DECLARE(pokemon_04); LV_IMG_DECLARE(pokemon_05);
LV_IMG_DECLARE(pokemon_06); LV_IMG_DECLARE(pokemon_07); LV_IMG_DECLARE(pokemon_08);
LV_IMG_DECLARE(pokemon_09); LV_IMG_DECLARE(pokemon_10); LV_IMG_DECLARE(pokemon_11);
LV_IMG_DECLARE(pokemon_12); LV_IMG_DECLARE(pokemon_13); LV_IMG_DECLARE(pokemon_14);
LV_IMG_DECLARE(pokemon_15); LV_IMG_DECLARE(pokemon_16); LV_IMG_DECLARE(pokemon_17);
LV_IMG_DECLARE(pokemon_18); LV_IMG_DECLARE(pokemon_19); LV_IMG_DECLARE(pokemon_20);
LV_IMG_DECLARE(pokemon_21); LV_IMG_DECLARE(pokemon_22); LV_IMG_DECLARE(pokemon_23);
LV_IMG_DECLARE(pokemon_24); LV_IMG_DECLARE(pokemon_25); LV_IMG_DECLARE(pokemon_26);
LV_IMG_DECLARE(pokemon_27); LV_IMG_DECLARE(pokemon_28); LV_IMG_DECLARE(pokemon_29);
LV_IMG_DECLARE(pokemon_30); LV_IMG_DECLARE(pokemon_31); LV_IMG_DECLARE(pokemon_32);
LV_IMG_DECLARE(pokemon_33); LV_IMG_DECLARE(pokemon_34); LV_IMG_DECLARE(pokemon_35);
LV_IMG_DECLARE(pokemon_36); LV_IMG_DECLARE(pokemon_37); LV_IMG_DECLARE(pokemon_38);
LV_IMG_DECLARE(pokemon_39); LV_IMG_DECLARE(pokemon_40); LV_IMG_DECLARE(pokemon_41);
LV_IMG_DECLARE(pokemon_42); LV_IMG_DECLARE(pokemon_43); LV_IMG_DECLARE(pokemon_44);
LV_IMG_DECLARE(pokemon_45); LV_IMG_DECLARE(pokemon_46); LV_IMG_DECLARE(pokemon_47);
static const lv_img_dsc_t *pokemon_imgs[] = {
    &pokemon_00, &pokemon_01, &pokemon_02, &pokemon_03, &pokemon_04, &pokemon_05,
    &pokemon_06, &pokemon_07, &pokemon_08, &pokemon_09, &pokemon_10, &pokemon_11,
    &pokemon_12, &pokemon_13, &pokemon_14, &pokemon_15, &pokemon_16, &pokemon_17,
    &pokemon_18, &pokemon_19, &pokemon_20, &pokemon_21, &pokemon_22, &pokemon_23,
    &pokemon_24, &pokemon_25, &pokemon_26, &pokemon_27, &pokemon_28, &pokemon_29,
    &pokemon_30, &pokemon_31, &pokemon_32, &pokemon_33, &pokemon_34, &pokemon_35,
    &pokemon_36, &pokemon_37, &pokemon_38, &pokemon_39, &pokemon_40, &pokemon_41,
    &pokemon_42, &pokemon_43, &pokemon_44, &pokemon_45, &pokemon_46, &pokemon_47,
};
#endif /* animation type */

#else /* IS_ENABLED(CONFIG_NICE_OLED_WIDGET_STATIC_IMAGE_PERIPHERAL) */

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_STATIC_IMAGE_PERIPHERAL_VIM)
LV_IMG_DECLARE(vim);
static const lv_img_dsc_t *static_vim_arr[] = { &vim };

#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_STATIC_IMAGE_PERIPHERAL_VIP_MARCOS)
LV_IMG_DECLARE(vip_marcos);
static const lv_img_dsc_t *static_marcos_arr[] = { &vip_marcos };

#endif
#endif /* animation / static */

/* ------------------------------------------------------------------ */
/*  Frame tracker state (replaces lv_animimg LVGL child object)       */
/* ------------------------------------------------------------------ */
static const lv_img_dsc_t **anim_frames     = NULL;
static uint16_t             anim_num_frames = 0;
static uint16_t             anim_frame_idx  = 0;

const lv_img_dsc_t *zmk_animation_get_current_frame(void)
{
    if (!anim_frames || anim_num_frames == 0) {
        return NULL;
    }
    return anim_frames[anim_frame_idx % anim_num_frames];
}

/* lv_anim_t exec callback: fires at the LVGL tick rate, advances frame
 * index only when the interpolated value crosses a new integer, then
 * triggers a full canvas redraw so the new frame is blitted before rotation. */
static void anim_exec_cb(void *var, int32_t val)
{
    (void)var;
    uint16_t new_frame = (uint16_t)((uint32_t)val % anim_num_frames);
    if (new_frame != anim_frame_idx) {
        anim_frame_idx = new_frame;
        zmk_widget_screen_all_redraw();
    }
}

/* ------------------------------------------------------------------ */
/*  Public init – replaces old draw_animation() that created animimg  */
/* ------------------------------------------------------------------ */
void draw_animation(lv_obj_t *canvas, struct zmk_widget_screen *widget)
{
    /* Parameters kept for API compatibility; the canvas buffer is now
     * written by draw_canvas() → lv_canvas_draw_img() before rotation.     */
    (void)canvas;
    (void)widget;

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL)

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_HEAD)
    anim_frames     = head_imgs;
    anim_num_frames = 16;
#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_CAT)
    anim_frames     = cat_imgs;
    anim_num_frames = 8;
#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_SPACEMAN)
    anim_frames     = spaceman_imgs;
    anim_num_frames = 20;
#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_POKEMON)
    anim_frames     = pokemon_imgs;
    anim_num_frames = 48;
#else
    anim_frames     = crystal_imgs;
    anim_num_frames = 16;
#endif

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, NULL);
    lv_anim_set_values(&a, 0, (int32_t)(anim_num_frames - 1));
    lv_anim_set_time(&a, CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_MS);
    lv_anim_set_exec_cb(&a, anim_exec_cb);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

#else /* static image – no cycling timer needed */

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_STATIC_IMAGE_PERIPHERAL_VIM)
    anim_frames     = static_vim_arr;
    anim_num_frames = 1;

#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_STATIC_IMAGE_PERIPHERAL_VIP_MARCOS)
    anim_frames     = static_marcos_arr;
    anim_num_frames = 1;

#else
    /* Pick one random crystal frame at boot; no cycling. */
    int length = (int)(sizeof(crystal_imgs) / sizeof(crystal_imgs[0]));
    srand(k_uptime_get_32());
    anim_frame_idx  = (uint16_t)(rand() % length);
    anim_frames     = crystal_imgs;
    anim_num_frames = (uint16_t)length;
#endif

#endif /* animation / static */
}

#endif /* !CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_SMART_BATTERY */
