/*
 * PATCH: panel_editions_ssd1306_fix
 *
 * Changes vs original:
 *  1. draw_canvas(): luna frame and sleep art blitted with lv_canvas_draw_img()
 *     BEFORE rotate_canvas() so they share the same portrait coordinate system.
 *  2. zmk_widget_screen_init(): removed lv_animimg creation for luna and
 *     lv_obj_create/lv_img_create for sleep_status.  Only listeners are
 *     registered; no LVGL child objects are attached to the canvas.
 *  3. zmk_widget_screen_all_redraw() added: called by luna.c's lv_anim_t
 *     timer on every frame transition.
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/usb.h>
#include <zmk/wpm.h>

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL)  || \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY) || \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)
#include <zmk/split/central.h>
#endif

#include <fonts.h>
#include "output.h"
#include "profile.h"
#include "screen.h"

#ifdef CONFIG_NICE_OLED_WIDGET_RAW_HID
#include <lvgl.h>
#include <raw_hid/hid.h>
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED)
#include <zmk/events/keycode_state_changed.h>
#include <zmk/hid.h>
#include <dt-bindings/zmk/modifiers.h>
#endif

#if !IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL)  && \
    !IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY) && \
    !IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)
#include "battery.h"
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL)  || \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY) || \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)
struct battery_state {
    uint8_t source;
    uint8_t level;
    bool    usb_present;
};

static void draw_battery_text(lv_obj_t *canvas, const struct status_state *state)
{
    char text[32] = "";
    lv_draw_label_dsc_t label_dsc;

#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono_16, LV_TEXT_ALIGN_LEFT);
#else
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_unscii_8, LV_TEXT_ALIGN_LEFT);
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL)
    char *p   = text;
    char *end = text + sizeof(text);
    for (int i = 0; i < CONFIG_NICE_OLED_SPLIT_TOTAL_DEVICES; i++) {
        int written = snprintf(p, end - p, "%d ", state->batteries[i].level);
        if (written > 0) p += written;
    }
    if (p > text) *(p - 1) = '\0';

#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY)
    char *p   = text;
    char *end = text + sizeof(text);
    for (int i = 1; i < CONFIG_NICE_OLED_SPLIT_TOTAL_DEVICES; i++) {
        int written = snprintf(p, end - p, "%d  ", state->batteries[i].level);
        if (written > 0) p += written;
    }
    if (p > text) *(p - 1) = '\0';

#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)
    if (CONFIG_NICE_OLED_SPLIT_TOTAL_DEVICES >= 2) {
        snprintf(text, sizeof(text), "%d  %d",
                 state->batteries[0].level, state->batteries[1].level);
    } else {
        snprintf(text, sizeof(text), "%d", state->batteries[0].level);
    }
#endif

    lv_canvas_draw_text(canvas, 0, 19, lv_obj_get_width(canvas), &label_dsc, text);
}
#endif /* CENTRAL_SHOW_BATTERY_PERIPHERAL variants */

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER)
#include "layer.h"
#endif
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM)
#include "wpm.h"
#endif

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/* Forward declaration */
static void draw_canvas(lv_obj_t *widget, lv_color_t cbuf[],
                        const struct status_state *state);

/* ---- sleep status ------------------------------------------------- */
#if IS_ENABLED(CONFIG_NICE_OLED_SHOW_SLEEP_ART_ON_IDLE) || \
    IS_ENABLED(CONFIG_NICE_OLED_SHOW_SLEEP_ART_ON_SLEEP)
#include "sleep_status.h"
static struct zmk_widget_sleep_status sleep_status_widget;
#endif

/* ---- luna --------------------------------------------------------- */
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM_LUNA)
#include "luna.h"
static struct zmk_widget_luna luna_widget;
#endif

/* ---- bongo cat ---------------------------------------------------- */
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM_BONGO_CAT)
#include "bongo_cat.h"
static struct zmk_widget_wpm_bongo_cat wpm_bongo_cat_widget;
#endif

/* ---- responsive bongo cat ----------------------------------------- */
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RESPONSIVE_BONGO_CAT)
#include "responsive_bongo_cat.h"
static struct zmk_widget_responsive_bongo_cat responsive_bongo_cat_widget;
#endif

/* ---- modifiers (luna variant) ------------------------------------- */
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_LUNA)
#include "modifiers.h"
static struct zmk_widget_modifiers modifiers_widget;
#endif

/* ================================================================== */
/*  MODIFIERS FIXED (canvas-draw, portrait coords)                    */
/* ================================================================== */
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED)

struct mods_status_state { uint8_t mods; };

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_SYMBOL)
LV_IMG_DECLARE(control_0);      LV_IMG_DECLARE(control_white_0);
LV_IMG_DECLARE(shift_0);        LV_IMG_DECLARE(shift_white_0);
LV_IMG_DECLARE(opt_0);          LV_IMG_DECLARE(opt_white_0);
LV_IMG_DECLARE(alt_0);          LV_IMG_DECLARE(alt_white_0);
LV_IMG_DECLARE(cmd_0);          LV_IMG_DECLARE(cmd_white_0);
LV_IMG_DECLARE(win_0);          LV_IMG_DECLARE(win_white_0);

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_SYMBOL_WINDOWS)
static const lv_img_dsc_t *mod_imgs_normal[4] = {&control_0, &shift_0, &alt_0, &win_0};
static const lv_img_dsc_t *mod_imgs_active[4] = {&control_white_0, &shift_white_0,
                                                  &alt_white_0, &win_white_0};
#else /* macOS (default) */
static const lv_img_dsc_t *mod_imgs_normal[4] = {&control_0, &shift_0, &opt_0, &cmd_0};
static const lv_img_dsc_t *mod_imgs_active[4] = {&control_white_0, &shift_white_0,
                                                  &opt_white_0, &cmd_white_0};
#endif
#endif /* FIXED_SYMBOL */

static void draw_mods_status(lv_obj_t *canvas, const struct status_state *state)
{
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_SYMBOL)
    lv_draw_img_dsc_t img_dsc;
    lv_draw_img_dsc_init(&img_dsc);
    const int img_size = 14;
    const int spacing  = 2;

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_VER)
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_VER_ALIGN_RIGHT)
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    const int base_x = 68 - img_size - 2;
#else
    const int base_x = 128 - img_size - 2;
#endif
#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_VER_ALIGN_LEFT)
    const int base_x = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_X;
#else
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    const int base_x = (68 - img_size) / 2;
#else
    const int base_x = (128 - img_size) / 2;
#endif
#endif
    const int base_y = 62;
    for (int i = 0; i < 4; i++) {
        bool selected = (state->mod_state >> i) & 1 || (state->mod_state >> (i + 4)) & 1;
        int  cx       = base_x;
        int  cy       = base_y + i * (img_size + spacing);
        lv_canvas_draw_img(canvas, cx, cy,
                           selected ? mod_imgs_active[i] : mod_imgs_normal[i], &img_dsc);
    }
#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_HOR)
    const int base_x = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_X;
    const int base_y = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_Y;
    for (int i = 0; i < 4; i++) {
        bool selected = (state->mod_state >> i) & 1 || (state->mod_state >> (i + 4)) & 1;
        lv_canvas_draw_img(canvas,
                           base_x + i * (img_size + spacing), base_y,
                           selected ? mod_imgs_active[i] : mod_imgs_normal[i], &img_dsc);
    }
#else /* BOX (2×2) / default */
    const int base_x = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_X;
    const int base_y = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_Y;
    static const int offsets[4][2] = {
        {0,              0             },
        {img_size + spacing, 0         },
        {0,              img_size + spacing},
        {img_size + spacing, img_size + spacing},
    };
    for (int i = 0; i < 4; i++) {
        bool selected = (state->mod_state >> i) & 1 || (state->mod_state >> (i + 4)) & 1;
        lv_canvas_draw_img(canvas,
                           base_x + offsets[i][0], base_y + offsets[i][1],
                           selected ? mod_imgs_active[i] : mod_imgs_normal[i], &img_dsc);
    }
#endif

#else /* TEXT mode */
    const char *items[4] = {"C", "S", "A", "G"};
    lv_draw_rect_dsc_t  rect_black_dsc, rect_white_dsc;
    lv_draw_label_dsc_t mod_dsc, mod_dsc_black;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    init_rect_dsc(&rect_white_dsc, LVGL_FOREGROUND);
    init_label_dsc(&mod_dsc,       LVGL_FOREGROUND, &lv_font_unscii_8, LV_TEXT_ALIGN_CENTER);
    init_label_dsc(&mod_dsc_black, LVGL_BACKGROUND, &lv_font_unscii_8, LV_TEXT_ALIGN_CENTER);

    const int box_w   = 12, box_h   = 14;
    const int inner   = 2;
    const int inner_w = box_w - 2 * inner;
    const int inner_h = box_h - 2 * inner;
    const int ty      = 4;

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_VER)
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_VER_ALIGN_RIGHT)
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    const int base_x = 68 - box_w - 2;
#else
    const int base_x = 128 - box_w - 2;
#endif
#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_VER_ALIGN_LEFT)
    const int base_x = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_X;
#else
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    const int base_x = (68 - box_w) / 2;
#else
    const int base_x = (128 - box_w) / 2;
#endif
#endif
    const int base_y = 38;
    for (int i = 0; i < 4; i++) {
        bool selected = (state->mod_state >> i) & 1 || (state->mod_state >> (i + 4)) & 1;
        int cx = base_x, cy = base_y + i * (box_h + 2);
        lv_canvas_draw_rect(canvas, cx, cy, box_w, box_h, &rect_black_dsc);
        if (selected && inner_w > 0 && inner_h > 0)
            lv_canvas_draw_rect(canvas, cx + inner, cy + inner, inner_w, inner_h, &rect_white_dsc);
        lv_canvas_draw_text(canvas, cx, cy + ty, box_w,
                            (selected ? &mod_dsc_black : &mod_dsc), items[i]);
    }
#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_HOR)
    const int base_x = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_X;
    const int base_y = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_Y;
    for (int i = 0; i < 4; i++) {
        bool selected = (state->mod_state >> i) & 1 || (state->mod_state >> (i + 4)) & 1;
        int cx = base_x + i * (box_w + 2), cy = base_y;
        lv_canvas_draw_rect(canvas, cx, cy, box_w, box_h, &rect_black_dsc);
        if (selected && inner_w > 0 && inner_h > 0)
            lv_canvas_draw_rect(canvas, cx + inner, cy + inner, inner_w, inner_h, &rect_white_dsc);
        lv_canvas_draw_text(canvas, cx, cy + ty, box_w,
                            (selected ? &mod_dsc_black : &mod_dsc), items[i]);
    }
#else /* BOX / default */
    const int base_x = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_X;
    const int base_y = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_Y;
    static const int offsets[4][2] = {
        {0,          0         }, {box_w + 2, 0         },
        {0,          box_h + 2 }, {box_w + 2, box_h + 2 },
    };
    for (int i = 0; i < 4; i++) {
        bool selected = (state->mod_state >> i) & 1 || (state->mod_state >> (i + 4)) & 1;
        int cx = base_x + offsets[i][0], cy = base_y + offsets[i][1];
        lv_canvas_draw_rect(canvas, cx, cy, box_w, box_h, &rect_black_dsc);
        if (selected && inner_w > 0 && inner_h > 0)
            lv_canvas_draw_rect(canvas, cx + inner, cy + inner, inner_w, inner_h, &rect_white_dsc);
        lv_canvas_draw_text(canvas, cx, cy + ty, box_w,
                            (selected ? &mod_dsc_black : &mod_dsc), items[i]);
    }
#endif
#endif /* FIXED_SYMBOL / TEXT */
}

static void set_mods_status(struct zmk_widget_screen *widget,
                            struct mods_status_state state)
{
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    widget->state.mod_state = zmk_hid_get_explicit_mods();
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
#endif
    (void)state;
}

static void mods_status_update_cb(struct mods_status_state state)
{
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_mods_status(widget, state);
    }
}

static struct mods_status_state mods_status_get_state(const zmk_event_t *eh)
{
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    return (struct mods_status_state){ .mods = zmk_hid_get_explicit_mods() };
#else
    return (struct mods_status_state){ .mods = 0 };
#endif
    (void)eh;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_mods_status, struct mods_status_state,
                            mods_status_update_cb, mods_status_get_state)
ZMK_SUBSCRIPTION(widget_mods_status, zmk_keycode_state_changed);

#endif /* CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED */

/* ================================================================== */
/*  RAW HID                                                           */
/* ================================================================== */
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID)

#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
#define DRAW_HID_STATUS_FONTS &lv_font_montserrat_14
#else
#define DRAW_HID_STATUS_FONTS &pixel_operator_mono_12
#endif

static void draw_hid_status(lv_obj_t *canvas, const struct status_state *state)
{
    lv_draw_rect_dsc_t  rect_black_dsc;
    lv_draw_label_dsc_t label_time, label_layout, label_volume;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    init_label_dsc(&label_time,   LVGL_FOREGROUND, DRAW_HID_STATUS_FONTS, LV_TEXT_ALIGN_LEFT);
    init_label_dsc(&label_layout, LVGL_FOREGROUND, DRAW_HID_STATUS_FONTS, LV_TEXT_ALIGN_LEFT);
    init_label_dsc(&label_volume, LVGL_FOREGROUND, DRAW_HID_STATUS_FONTS, LV_TEXT_ALIGN_LEFT);

    int hid_x = CONFIG_NICE_OLED_WIDGET_RAW_HID_CUSTOM_X;
    int hid_y = CONFIG_NICE_OLED_WIDGET_RAW_HID_CUSTOM_Y;
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    int hid_w = 68;
#else
    int hid_w = 32;
#endif

    lv_coord_t current_y = hid_y;
    lv_point_t text_size;
    const lv_coord_t line_gap = 0;

    if (state->is_connected) {
        char buf[20];

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_WEATHER)
        sprintf(buf, "%dC", state->temperature);
        lv_canvas_draw_text(canvas, CONFIG_NICE_OLED_WIDGET_RAW_HID_WEATHER_CUSTOM_X,
                            CONFIG_NICE_OLED_WIDGET_RAW_HID_WEATHER_CUSTOM_Y,
                            hid_w, &label_volume, buf);
#endif
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_TIME)
        sprintf(buf, "%02i:%02i", state->hour, state->minute);
        lv_canvas_draw_text(canvas, CONFIG_NICE_OLED_WIDGET_RAW_HID_TIME_CUSTOM_X,
                            CONFIG_NICE_OLED_WIDGET_RAW_HID_TIME_CUSTOM_Y,
                            hid_w, &label_time, buf);
#endif
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT)
        char layout_str[10] = {};
#ifdef CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT_LIST
        char layouts_cfg[sizeof(CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT_LIST)];
        strcpy(layouts_cfg, CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT_LIST);
        char *tok = strtok(layouts_cfg, ",");
        size_t idx = 0;
        while (tok && idx < state->layout) { idx++; tok = strtok(NULL, ","); }
        if (tok) snprintf(layout_str, sizeof(layout_str), "%s", tok);
        else     snprintf(layout_str, sizeof(layout_str), "%i", state->layout);
#else
        snprintf(layout_str, sizeof(layout_str), "L%i", state->layout);
#endif
        lv_canvas_draw_text(canvas, CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT_CUSTOM_X,
                            CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT_CUSTOM_Y,
                            hid_w, &label_layout, layout_str);
#endif
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_VOLUME)
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
        sprintf(buf, "Vol: %i", state->volume);
#else
        sprintf(buf, "V:%i", state->volume);
#endif
        lv_canvas_draw_text(canvas, CONFIG_NICE_OLED_WIDGET_RAW_HID_VOLUME_CUSTOM_X,
                            CONFIG_NICE_OLED_WIDGET_RAW_HID_VOLUME_CUSTOM_Y,
                            hid_w, &label_volume, buf);
#endif
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_MEDIA_PLAYER_SPOTIFY_MACOS)
        lv_canvas_draw_text(canvas, CONFIG_NICE_OLED_WIDGET_RAW_HID_MEDIA_PLAYER_CUSTOM_X,
                            CONFIG_NICE_OLED_WIDGET_RAW_HID_MEDIA_PLAYER_CUSTOM_Y,
                            hid_w, &label_volume, state->media_player);
#endif
    } else {
        lv_txt_get_size(&text_size, "HID", label_time.font, label_time.letter_space,
                        label_time.line_space, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_canvas_draw_text(canvas, hid_x, current_y, hid_w, &label_time, "HID");
        current_y += text_size.y + line_gap;

        lv_txt_get_size(&text_size, "not", label_layout.font, label_layout.letter_space,
                        label_layout.line_space, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_canvas_draw_text(canvas, hid_x, current_y, hid_w, &label_layout, "not");
        current_y += text_size.y + line_gap;

        lv_txt_get_size(&text_size, "found", label_volume.font, label_volume.letter_space,
                        label_volume.line_space, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_canvas_draw_text(canvas, hid_x, current_y, hid_w, &label_volume, "found");
    }
}

static struct is_connected_notification get_is_hid_connected(const zmk_event_t *eh)
{
    struct is_connected_notification *n = as_is_connected_notification(eh);
    if (n) return *n;
    return (struct is_connected_notification){ .value = false };
}

static void hid_is_connected_update_cb(struct is_connected_notification is_connected)
{
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->state.is_connected = is_connected.value;
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
}
ZMK_DISPLAY_WIDGET_LISTENER(widget_is_connected, struct is_connected_notification,
                            hid_is_connected_update_cb, get_is_hid_connected);
ZMK_SUBSCRIPTION(widget_is_connected, is_connected_notification);

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_TIME)
static struct time_notification get_time(const zmk_event_t *eh)
{
    struct time_notification *n = as_time_notification(eh);
    if (n) return *n;
    return (struct time_notification){ .hour = 0, .minute = 0 };
}
static void hid_time_update_cb(struct time_notification time)
{
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->state.hour   = time.hour;
        widget->state.minute = time.minute;
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
}
ZMK_DISPLAY_WIDGET_LISTENER(widget_time, struct time_notification,
                            hid_time_update_cb, get_time);
ZMK_SUBSCRIPTION(widget_time, time_notification);
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_VOLUME)
static struct volume_notification get_volume(const zmk_event_t *eh)
{
    struct volume_notification *n = as_volume_notification(eh);
    if (n) return *n;
    return (struct volume_notification){ .value = 0 };
}
static void hid_volume_update_cb(struct volume_notification volume)
{
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->state.volume = volume.value;
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
}
ZMK_DISPLAY_WIDGET_LISTENER(widget_volume, struct volume_notification,
                            hid_volume_update_cb, get_volume);
ZMK_SUBSCRIPTION(widget_volume, volume_notification);
#endif

#ifdef CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT
static struct layout_notification get_layout(const zmk_event_t *eh)
{
    struct layout_notification *n = as_layout_notification(eh);
    if (n) return *n;
    return (struct layout_notification){ .value = 0 };
}
static void hid_layout_update_cb(struct layout_notification layout)
{
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->state.layout = layout.value;
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
}
ZMK_DISPLAY_WIDGET_LISTENER(widget_layout, struct layout_notification,
                            hid_layout_update_cb, get_layout);
ZMK_SUBSCRIPTION(widget_layout, layout_notification);
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_WEATHER)
static void weather_status_update_cb(struct weather_notification weather)
{
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->state.temperature = weather.temperature;
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
}
static struct weather_notification weather_status_get_state(const zmk_event_t *eh)
{
    const struct weather_notification *ev = as_weather_notification(eh);
    if (!ev) return (struct weather_notification){ .temperature = 127 };
    return *ev;
}
ZMK_DISPLAY_WIDGET_LISTENER(widget_weather_status, struct weather_notification,
                            weather_status_update_cb, weather_status_get_state);
ZMK_SUBSCRIPTION(widget_weather_status, weather_notification);
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_MEDIA_PLAYER_SPOTIFY_MACOS)
static void spotify_status_update_cb(struct spotify_notification spotify)
{
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        memcpy(widget->state.media_player, spotify.media_player,
               sizeof(widget->state.media_player));
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
}
static struct spotify_notification spotify_status_get_state(const zmk_event_t *eh)
{
    const struct spotify_notification *ev = as_spotify_notification(eh);
    if (!ev) return (struct spotify_notification){ .media_player = "" };
    return *ev;
}
ZMK_DISPLAY_WIDGET_LISTENER(widget_spotify_status, struct spotify_notification,
                            spotify_status_update_cb, spotify_status_get_state);
ZMK_SUBSCRIPTION(widget_spotify_status, spotify_notification);
#endif

#endif /* CONFIG_NICE_OLED_WIDGET_RAW_HID */

/* ---- HID indicators ----------------------------------------------- */
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_HID_INDICATORS)
#include "hid_indicators.h"
static struct zmk_widget_hid_indicators hid_indicators_widget;
#endif

/* ================================================================== */
/*  draw_canvas                                                        */
/*                                                                     */
/*  All content drawn into portrait canvas buffer BEFORE rotate_canvas */
/*  so images and text widgets share the same coordinate system.       */
/*                                                                     */
/*  Coordinate convention:                                             */
/*    portrait (sx, sy) → physical (127-sy, sx) after rotation        */
/*    CONFIG_NICE_OLED_WIDGET_*_CUSTOM_{X,Y} are portrait coords      */
/* ================================================================== */
static void draw_canvas(lv_obj_t *widget, lv_color_t cbuf[],
                        const struct status_state *state)
{
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);

    draw_background(canvas);
    draw_output_status(canvas, state);

#if !IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL)  && \
    !IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY) && \
    !IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)
    draw_battery_status(canvas, state);
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL)  || \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY) || \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)
    draw_battery_text(canvas, state);
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM)
    draw_wpm_status(canvas, state);
#endif

    draw_profile_status(canvas, state);

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER)
    draw_layer_status(canvas, state);
#endif

#ifdef CONFIG_NICE_OLED_WIDGET_RAW_HID
    draw_hid_status(canvas, state);
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED)
    draw_mods_status(canvas, state);
#endif

    /* --- Luna animation frame (portrait coords, pre-rotation) ---------- */
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM_LUNA)
    {
        const lv_img_dsc_t *luna_frame = zmk_widget_luna_get_current_frame();
        if (luna_frame) {
            lv_draw_img_dsc_t img_dsc;
            lv_draw_img_dsc_init(&img_dsc);
            lv_canvas_draw_img(canvas,
                CONFIG_NICE_OLED_WIDGET_LUNA_CUSTOM_X,
                CONFIG_NICE_OLED_WIDGET_LUNA_CUSTOM_Y,
                luna_frame, &img_dsc);
        }
    }
#endif

    /* --- Sleep art overlay (portrait coords, drawn last) --------------- */
#if IS_ENABLED(CONFIG_NICE_OLED_SHOW_SLEEP_ART_ON_IDLE) || \
    IS_ENABLED(CONFIG_NICE_OLED_SHOW_SLEEP_ART_ON_SLEEP)
    {
        zmk_activity_state_t _act = zmk_widget_sleep_status_get_activity();
        bool _show = false;
#if IS_ENABLED(CONFIG_NICE_OLED_SHOW_SLEEP_ART_ON_IDLE)
        if (_act == ZMK_ACTIVITY_IDLE)  _show = true;
#endif
#if IS_ENABLED(CONFIG_NICE_OLED_SHOW_SLEEP_ART_ON_SLEEP)
        if (_act == ZMK_ACTIVITY_SLEEP) _show = true;
#endif
        if (_show) {
            extern const lv_img_dsc_t vim_32x128;
            lv_draw_img_dsc_t img_dsc;
            lv_draw_img_dsc_init(&img_dsc);
            lv_canvas_draw_img(canvas,
                CONFIG_NICE_OLED_WIDGET_SLEEP_STATUS_CUSTOM_X,
                CONFIG_NICE_OLED_WIDGET_SLEEP_STATUS_CUSTOM_Y,
                &vim_32x128, &img_dsc);
        }
    }
#endif

    /* Rotate portrait 64×128 → physical 128×64 */
    rotate_canvas(canvas, cbuf);
}

/* ================================================================== */
/*  zmk_widget_screen_all_redraw                                       */
/*  Called by luna.c lv_anim_t timer on each frame transition.        */
/* ================================================================== */
void zmk_widget_screen_all_redraw(void)
{
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
}

/* ================================================================== */
/*  Event listeners                                                    */
/* ================================================================== */

/* ---- Battery ------------------------------------------------------ */
#if !IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL)  && \
    !IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY) && \
    !IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)

static void set_battery_status(struct zmk_widget_screen *widget,
                               struct battery_status_state state)
{
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif
    widget->state.battery = state.level;
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}
static void battery_status_update_cb(struct battery_status_state state)
{
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_battery_status(widget, state);
    }
}
static struct battery_status_state battery_status_get_state(const zmk_event_t *eh)
{
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    return (struct battery_status_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif
    };
}
ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state);
ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif

#endif /* !CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL */

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL)  || \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY) || \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)

static void set_battery_status(struct zmk_widget_screen *widget, struct battery_state state)
{
    if (state.source >= CONFIG_NICE_OLED_SPLIT_TOTAL_DEVICES) return;
    widget->state.batteries[state.source].level       = state.level;
    widget->state.batteries[state.source].usb_present = state.usb_present;
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}
void battery_status_update_cb(struct battery_state state)
{
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_battery_status(widget, state);
    }
}

static struct battery_state peripheral_battery_status_get_state(const zmk_event_t *eh)
{
    const struct zmk_peripheral_battery_state_changed *ev =
        as_zmk_peripheral_battery_state_changed(eh);
    return (struct battery_state){ .source = ev->source + 1, .level = ev->state_of_charge };
}
static struct battery_state central_battery_status_get_state(const zmk_event_t *eh)
{
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    return (struct battery_state){
        .source = 0,
        .level  = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif
    };
}
static struct battery_state battery_status_get_state(const zmk_event_t *eh)
{
    if (as_zmk_peripheral_battery_state_changed(eh) != NULL)
        return peripheral_battery_status_get_state(eh);
    return central_battery_status_get_state(eh);
}
ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_state,
                            battery_status_update_cb, battery_status_get_state)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_peripheral_battery_state_changed);
ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif /* CENTRAL_SHOW_BATTERY_PERIPHERAL variants */

/* ---- Layer -------------------------------------------------------- */
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER)
static void set_layer_status(struct zmk_widget_screen *widget,
                             struct layer_status_state state)
{
    widget->state.layer_index = state.index;
    widget->state.layer_label = state.label;
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}
static void layer_status_update_cb(struct layer_status_state state)
{
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_layer_status(widget, state);
    }
}
static struct layer_status_state layer_status_get_state(const zmk_event_t *eh)
{
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_status_state){ .index = index, .label = zmk_keymap_layer_name(index) };
}
ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state,
                            layer_status_update_cb, layer_status_get_state)
ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);
#endif

/* ---- Output ------------------------------------------------------- */
static void set_output_status(struct zmk_widget_screen *widget,
                              const struct output_status_state *state)
{
    widget->state.selected_endpoint          = state->selected_endpoint;
    widget->state.active_profile_index       = state->active_profile_index;
    widget->state.active_profile_connected   = state->active_profile_connected;
    widget->state.active_profile_bonded      = state->active_profile_bonded;
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}
static void output_status_update_cb(struct output_status_state state)
{
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_output_status(widget, &state);
    }
}
static struct output_status_state output_status_get_state(const zmk_event_t *_eh)
{
    return (struct output_status_state){
        .selected_endpoint        = zmk_endpoints_selected(),
        .active_profile_index     = zmk_ble_active_profile_index(),
        .active_profile_connected = zmk_ble_active_profile_is_connected(),
        .active_profile_bonded    = !zmk_ble_active_profile_is_open(),
    };
}
ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, output_status_get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);
#endif
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif

/* ---- WPM ---------------------------------------------------------- */
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM)
static void set_wpm_status(struct zmk_widget_screen *widget,
                           struct wpm_status_state state)
{
    for (int i = 0; i < 9; i++) widget->state.wpm[i] = widget->state.wpm[i + 1];
    widget->state.wpm[9] = state.wpm;
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}
static void wpm_status_update_cb(struct wpm_status_state state)
{
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_wpm_status(widget, state);
    }
}
struct wpm_status_state wpm_status_get_state(const zmk_event_t *eh)
{
    return (struct wpm_status_state){ .wpm = zmk_wpm_get_state() };
}
ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm_status, struct wpm_status_state,
                            wpm_status_update_cb, wpm_status_get_state)
ZMK_SUBSCRIPTION(widget_wpm_status, zmk_wpm_state_changed);
#endif

/* ================================================================== */
/*  Initialization                                                     */
/* ================================================================== */
int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent)
{
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, CANVAS_HEIGHT, CANVAS_WIDTH);

    lv_obj_t *canvas = lv_canvas_create(widget->obj);
    lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_canvas_set_buffer(canvas, widget->cbuf, CANVAS_HEIGHT, CANVAS_HEIGHT,
                         LV_IMG_CF_TRUE_COLOR);

    sys_slist_append(&widgets, &widget->node);

    widget_battery_status_init();

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER)
    widget_layer_status_init();
#endif
    widget_output_status_init();
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM)
    widget_wpm_status_init();
#endif

    /* Luna: listener-only init; frame drawn via lv_canvas_draw_img in draw_canvas(). */
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM)
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM_LUNA)
    zmk_widget_luna_init(&luna_widget, canvas);
    /* No lv_obj_align: luna is now drawn into the canvas buffer, not as an LVGL child. */
#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM_BONGO_CAT)
    zmk_widget_wpm_bongo_cat_init(&wpm_bongo_cat_widget, canvas);
    lv_obj_align(zmk_widget_wpm_bongo_cat_obj(&wpm_bongo_cat_widget), LV_ALIGN_TOP_LEFT,
                 CONFIG_NICE_OLED_WIDGET_BONGO_CAT_CUSTOM_X,
                 CONFIG_NICE_OLED_WIDGET_BONGO_CAT_CUSTOM_Y);
#endif
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RESPONSIVE_BONGO_CAT)
    zmk_widget_responsive_bongo_cat_init(&responsive_bongo_cat_widget, canvas);
    lv_obj_align(zmk_widget_responsive_bongo_cat_obj(&responsive_bongo_cat_widget),
                 LV_ALIGN_TOP_LEFT,
                 CONFIG_NICE_OLED_WIDGET_RESPONSIVE_BONGO_CAT_CUSTOM_X,
                 CONFIG_NICE_OLED_WIDGET_RESPONSIVE_BONGO_CAT_CUSTOM_Y);
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_HID_INDICATORS)
    zmk_widget_hid_indicators_init(&hid_indicators_widget, canvas);
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_LUNA)
    zmk_widget_modifiers_init(&modifiers_widget, canvas);
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED)
    widget_mods_status_init();
#endif

#ifdef CONFIG_NICE_OLED_WIDGET_RAW_HID
    widget_is_connected_init();
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_TIME)
    widget_time_init();
#endif
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_VOLUME)
    widget_volume_init();
#endif
#ifdef CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT
    widget_layout_init();
#endif
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_WEATHER)
    widget_weather_status_init();
#endif
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_MEDIA_PLAYER_SPOTIFY_MACOS)
    widget_spotify_status_init();
#endif
    struct zmk_widget_screen *w;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, w, node) {
        w->state.is_connected = false;
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_WEATHER)
        w->state.temperature = 127;
#endif
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_MEDIA_PLAYER_SPOTIFY_MACOS)
        w->state.media_player[0] = '\0';
#endif
    }
#endif /* CONFIG_NICE_OLED_WIDGET_RAW_HID */

    /* Sleep status: listener-only, no LVGL objects.
     * Activity state consumed by draw_canvas() before rotate_canvas().      */
#if IS_ENABLED(CONFIG_NICE_OLED_SHOW_SLEEP_ART_ON_IDLE) || \
    IS_ENABLED(CONFIG_NICE_OLED_SHOW_SLEEP_ART_ON_SLEEP)
    zmk_widget_sleep_status_init(&sleep_status_widget, NULL);
    /* No lv_obj_align: sleep art drawn directly into canvas buffer.          */
#endif

    return 0;
}

lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget)
{
    return widget->obj;
}
