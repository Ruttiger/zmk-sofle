#include <lvgl.h>
#include <string.h>
#include <zephyr/sys/util.h>

#define CANVAS_WIDTH CONFIG_NICE_OLED_CUSTOM_CANVAS_WIDTH
#define CANVAS_HEIGHT CONFIG_NICE_OLED_CUSTOM_CANVAS_HEIGHT

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_INVERTED)
#define LVGL_BACKGROUND lv_color_black()
#else
#define LVGL_BACKGROUND lv_color_white()
#endif

/*
 * nice!oled renders a portrait 64x128 canvas and rotates it before the
 * SSD1306 update. Upstream currently describes that source as a 128x128
 * image, so a 128x64 display receives only half of the rotated result.
 * Keep the upstream screen implementations and correct only the rectangular
 * source geometry here.
 */
void __wrap_rotate_canvas(lv_obj_t *canvas, lv_color_t cbuf[]) {
  static lv_color_t cbuf_tmp[CANVAS_WIDTH * CANVAS_HEIGHT];

  for (int y = 0; y < CANVAS_HEIGHT; y++) {
    memcpy(&cbuf_tmp[y * CANVAS_WIDTH], &cbuf[y * CANVAS_HEIGHT],
           CANVAS_WIDTH * sizeof(lv_color_t));
  }

  lv_img_dsc_t img;
  img.data = (void *)cbuf_tmp;
  img.header.cf = LV_IMG_CF_TRUE_COLOR;
  img.header.w = CANVAS_WIDTH;
  img.header.h = CANVAS_HEIGHT;

  lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);
  lv_canvas_transform(canvas, &img, 900, LV_IMG_ZOOM_NONE, -1,
                      -(CANVAS_HEIGHT - CANVAS_WIDTH) / 2,
                      CANVAS_WIDTH / 2, CANVAS_HEIGHT / 2, false);
}
