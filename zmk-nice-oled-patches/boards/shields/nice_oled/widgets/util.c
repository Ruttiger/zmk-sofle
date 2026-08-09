#include "util.h"

#include <ctype.h>
#include <string.h>
#include <zephyr/kernel.h>

void to_uppercase(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        str[i] = toupper((unsigned char)str[i]);
    }
}

static void rotate_canvas_to(lv_obj_t *canvas, lv_color_t cbuf[], int16_t angle,
                             int16_t pivot_x, int16_t pivot_y) {
    static lv_color_t cbuf_tmp[CANVAS_HEIGHT * CANVAS_HEIGHT];
    memcpy(cbuf_tmp, cbuf, sizeof(cbuf_tmp));

    lv_img_dsc_t img;
    img.data = (void *)cbuf_tmp;
    img.header.cf = LV_IMG_CF_TRUE_COLOR;
    img.header.w = CANVAS_HEIGHT;
    img.header.h = CANVAS_HEIGHT;

    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);
    lv_canvas_transform(canvas, &img, angle, LV_IMG_ZOOM_NONE, pivot_x, pivot_y,
                        CANVAS_HEIGHT / 2, CANVAS_HEIGHT / 2, false);
}

void rotate_canvas(lv_obj_t *canvas, lv_color_t cbuf[]) {
    rotate_canvas_to(canvas, cbuf, 900, -1, 0);
}

void rotate_canvas_270(lv_obj_t *canvas, lv_color_t cbuf[]) {
    rotate_canvas_to(canvas, cbuf, 2700, 0, 0);
}

void draw_background(lv_obj_t *canvas) {
    lv_draw_rect_dsc_t rect_dsc;
    init_rect_dsc(&rect_dsc, LVGL_BACKGROUND);
    lv_canvas_draw_rect(canvas, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT, &rect_dsc);
}

void init_label_dsc(lv_draw_label_dsc_t *label_dsc, lv_color_t color,
                    const lv_font_t *font, lv_text_align_t align) {
    lv_draw_label_dsc_init(label_dsc);
    label_dsc->color = color;
    label_dsc->font = font;
    label_dsc->align = align;
}

void init_rect_dsc(lv_draw_rect_dsc_t *rect_dsc, lv_color_t bg_color) {
    lv_draw_rect_dsc_init(rect_dsc);
    rect_dsc->bg_color = bg_color;
}

void init_line_dsc(lv_draw_line_dsc_t *line_dsc, lv_color_t color, uint8_t width) {
    lv_draw_line_dsc_init(line_dsc);
    line_dsc->color = color;
    line_dsc->width = width;
}

void draw_test_pattern(lv_obj_t *canvas) {
    lv_draw_rect_dsc_t white;
    lv_draw_rect_dsc_init(&white);
    white.bg_color = lv_color_white();
    white.bg_opa = LV_OPA_COVER;

    lv_canvas_draw_rect(canvas, 0, 0, 12, 12, &white);
    lv_canvas_draw_rect(canvas, 52, 0, 12, 2, &white);
    lv_canvas_draw_rect(canvas, 52, 10, 12, 2, &white);
    lv_canvas_draw_rect(canvas, 52, 0, 2, 12, &white);
    lv_canvas_draw_rect(canvas, 62, 0, 2, 12, &white);
}
