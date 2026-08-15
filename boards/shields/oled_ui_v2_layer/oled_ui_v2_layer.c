/*
 * RAW OLED UI V2: active ZMK layer.
 *
 * The display path remains the validated RAW SSD1306 framebuffer with a
 * portrait 64x128 drawing coordinate system. Layer events only submit work;
 * the work handler owns all display access.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

#include "oled_gfx.h"
#include "oled_raw_ssd1306_128x64.h"

LOG_MODULE_REGISTER(oled_ui_v2_layer, CONFIG_DISPLAY_LOG_LEVEL);

#define OLED_UI_LAYER_MAX_CHARS 10U

static void oled_ui_draw_layer_name(zmk_keymap_layer_index_t index)
{
	char layer_name[OLED_UI_LAYER_MAX_CHARS + 1U];
	size_t i = 0U;
	const char *configured_name = zmk_keymap_layer_name(
		zmk_keymap_layer_index_to_id(index));

	if (configured_name == NULL) {
		configured_name = "";
	}

	/* Font width is 5 pixels plus 1 pixel spacing: 10 chars fit in 64 px. */
	for (; i < OLED_UI_LAYER_MAX_CHARS && configured_name[i] != '\0'; ++i) {
		layer_name[i] = configured_name[i];
	}
	layer_name[i] = '\0';

	oled_gfx_draw_text(0, 48, layer_name, true);
}

static void oled_ui_draw_screen(void)
{
	char index_text[8];
	zmk_keymap_layer_index_t index = zmk_keymap_highest_layer_active();

	oled_gfx_clear();
	oled_gfx_draw_text(0, 0, "SOFLE", true);
	oled_gfx_draw_text(0, 12, "-----", true);
	oled_gfx_draw_text(0, 32, "LAYER", true);
	oled_ui_draw_layer_name(index);
	snprintf(index_text, sizeof(index_text), "#%u", (unsigned int)index);
	oled_gfx_draw_text(0, 64, index_text, true);
}

static void oled_ui_work_handler(struct k_work *work)
{
	int ret;

	ARG_UNUSED(work);

	ret = oled_raw_prepare();
	if (ret < 0) {
		LOG_ERR("OLED UI V2 prepare failed: %d", ret);
		return;
	}

	oled_gfx_init(oled_raw_framebuffer(), oled_raw_capabilities());
	oled_ui_draw_screen();
	ret = oled_gfx_flush();
	if (ret < 0) {
		LOG_ERR("OLED UI V2 flush failed: %d", ret);
		return;
	}

	LOG_INF("RAW OLED UI V2 layer screen written");
}

static K_WORK_DELAYABLE_DEFINE(oled_ui_work, oled_ui_work_handler);

static int oled_ui_layer_state_changed(const zmk_event_t *eh)
{
	int ret;

	ARG_UNUSED(eh);

	ret = k_work_reschedule(&oled_ui_work, K_NO_WAIT);
	if (ret < 0) {
		LOG_ERR("could not schedule RAW OLED UI V2 layer update: %d", ret);
	}

	return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(oled_ui_layer, oled_ui_layer_state_changed);
ZMK_SUBSCRIPTION(oled_ui_layer, zmk_layer_state_changed);

static int oled_ui_schedule(void)
{
	int ret = k_work_schedule(&oled_ui_work, K_SECONDS(2));

	if (ret < 0) {
		LOG_ERR("could not schedule RAW OLED UI V2 startup: %d", ret);
		return ret;
	}

	return 0;
}

/* Keep initialization deferred until after the display stack is ready. */
SYS_INIT(oled_ui_schedule, APPLICATION, 0);
