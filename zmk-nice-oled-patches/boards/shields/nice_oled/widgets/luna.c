/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * PATCH: panel_editions_ssd1306_fix
 *
 * Root-cause fix: removed lv_animimg_create(parent) LVGL child object.
 * Frame cycling is now driven by a plain lv_anim_t timer.  On every
 * frame transition zmk_widget_screen_all_redraw() is called so draw_canvas()
 * blits the new frame via lv_canvas_draw_img() BEFORE rotate_canvas().
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/wpm.h>

#include "luna.h"

/* Forward declaration – implemented in screen.c (central-only build). */
extern void zmk_widget_screen_all_redraw(void);

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/* ---- Frame arrays (pre-rotated 90° variants for portrait canvas) ---- */
LV_IMG_DECLARE(dog_sit1_90);
LV_IMG_DECLARE(dog_sit2_90);
LV_IMG_DECLARE(dog_walk1_90);
LV_IMG_DECLARE(dog_walk2_90);
LV_IMG_DECLARE(dog_run1_90);
LV_IMG_DECLARE(dog_run2_90);
LV_IMG_DECLARE(dog_sneak1_90);
LV_IMG_DECLARE(dog_sneak2_90);

#define ANIMATION_SPEED_IDLE 960
static const lv_img_dsc_t *idle_imgs[] = { &dog_sit1_90,  &dog_sit2_90  };

#define ANIMATION_SPEED_SLOW 200
static const lv_img_dsc_t *slow_imgs[] = { &dog_walk1_90, &dog_walk2_90 };

#define ANIMATION_SPEED_MID  200
static const lv_img_dsc_t *mid_imgs[]  = { &dog_walk1_90, &dog_walk2_90 };

#define ANIMATION_SPEED_FAST 200
static const lv_img_dsc_t *fast_imgs[] = { &dog_run1_90,  &dog_run2_90  };

/* ---- Frame tracker state ------------------------------------------ */
static const lv_img_dsc_t **active_frames     = NULL;
static uint8_t              active_num_frames = 2;
static uint8_t              luna_frame_idx   = 0;
static uint32_t             luna_anim_ms     = ANIMATION_SPEED_IDLE;

enum anim_state {
    anim_state_none,
    anim_state_idle,
    anim_state_slow,
    anim_state_mid,
    anim_state_fast
} current_anim_state;

const lv_img_dsc_t *zmk_widget_luna_get_current_frame(void)
{
    if (!active_frames || active_num_frames == 0) {
        return NULL;
    }
    return active_frames[luna_frame_idx % active_num_frames];
}

/* lv_anim_t exec callback: advances frame index and triggers canvas redraw. */
static void luna_anim_exec_cb(void *var, int32_t val)
{
    (void)var;
    uint8_t new_frame = (uint8_t)((uint32_t)val % active_num_frames);
    if (new_frame != luna_frame_idx) {
        luna_frame_idx = new_frame;
        zmk_widget_screen_all_redraw();
    }
}

static void restart_luna_anim(void)
{
    /* Delete any existing luna animation first. */
    lv_anim_del(NULL, luna_anim_exec_cb);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, NULL);
    lv_anim_set_values(&a, 0, (int32_t)(active_num_frames - 1));
    lv_anim_set_time(&a, luna_anim_ms);
    lv_anim_set_exec_cb(&a, luna_anim_exec_cb);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

struct luna_wpm_status_state {
    uint8_t wpm;
};

static void set_animation(struct luna_wpm_status_state state)
{
    if (state.wpm < 15) {
        if (current_anim_state != anim_state_idle) {
            active_frames     = idle_imgs;
            active_num_frames = 2;
            luna_anim_ms      = ANIMATION_SPEED_IDLE;
            luna_frame_idx    = 0;
            current_anim_state = anim_state_idle;
            restart_luna_anim();
        }
    } else if (state.wpm < 30) {
        if (current_anim_state != anim_state_slow) {
            active_frames     = slow_imgs;
            active_num_frames = 2;
            luna_anim_ms      = ANIMATION_SPEED_SLOW;
            luna_frame_idx    = 0;
            current_anim_state = anim_state_slow;
            restart_luna_anim();
        }
    } else if (state.wpm < 70) {
        if (current_anim_state != anim_state_mid) {
            active_frames     = mid_imgs;
            active_num_frames = 2;
            luna_anim_ms      = ANIMATION_SPEED_MID;
            luna_frame_idx    = 0;
            current_anim_state = anim_state_mid;
            restart_luna_anim();
        }
    } else {
        if (current_anim_state != anim_state_fast) {
            active_frames     = fast_imgs;
            active_num_frames = 2;
            luna_anim_ms      = ANIMATION_SPEED_FAST;
            luna_frame_idx    = 0;
            current_anim_state = anim_state_fast;
            restart_luna_anim();
        }
    }
}

struct luna_wpm_status_state luna_wpm_status_get_state(const zmk_event_t *eh)
{
    struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(eh);
    return (struct luna_wpm_status_state){ .wpm = ev->state };
}

void luna_wpm_status_update_cb(struct luna_wpm_status_state state)
{
    struct zmk_widget_luna *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        (void)widget; /* stateless – call set_animation once */
        set_animation(state);
        break;
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_luna, struct luna_wpm_status_state,
                            luna_wpm_status_update_cb, luna_wpm_status_get_state)

ZMK_SUBSCRIPTION(widget_luna, zmk_wpm_state_changed);

int zmk_widget_luna_init(struct zmk_widget_luna *widget, lv_obj_t *parent)
{
    (void)parent;   /* No LVGL object created */
    sys_slist_append(&widgets, &widget->node);

    /* Start with idle animation immediately. */
    active_frames      = idle_imgs;
    active_num_frames  = 2;
    luna_anim_ms       = ANIMATION_SPEED_IDLE;
    current_anim_state = anim_state_idle;
    restart_luna_anim();

    widget_luna_init();
    return 0;
}
