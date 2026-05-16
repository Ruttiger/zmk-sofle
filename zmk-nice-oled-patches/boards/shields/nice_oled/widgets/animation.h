#pragma once

#include <lvgl.h>
#include "util.h"
#include "screen_peripheral.h"

/* Initialise peripheral animation (replaces old lv_animimg child approach).
 * canvas and widget parameters are kept for API compatibility but unused.
 * After this call the animation advances via an lv_anim_t timer and calls
 * zmk_widget_screen_all_redraw() on every frame change.                      */
void draw_animation(lv_obj_t *canvas, struct zmk_widget_screen *widget);

/* Return the image descriptor for the current animation frame, or NULL when
 * no animation has been initialised yet.  Call this inside draw_canvas()
 * before rotate_canvas() to blit the frame into the portrait canvas buffer.  */
const lv_img_dsc_t *zmk_animation_get_current_frame(void);
