/*
 * Dynamic active-layer UI for the validated 32x128 LEFT viewport.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

#include "oled_gfx_portrait.h"
#include "oled_screen_content.h"
#include "oled_screen_controller.h"

static const char layer_abbreviations[][5] = {
	"WIN",
	"WFN",
	"MAC",
	"MFN",
	"GAL",
	"GFN",
	"UTIL",
};

static int oled_ui_centered_x(const char *text)
{
	size_t length = strlen(text);
	size_t width = length == 0U ? 0U : length * 6U - 1U;

	return width < OLED_GFX_WIDTH ?
		(OLED_GFX_WIDTH - (int)width) / 2 : 0;
}

static void oled_ui_format_layer_index(zmk_keymap_layer_index_t index,
				       char text[5])
{
	text[0] = 'L';
	if (index < 10U) {
		text[1] = (char)('0' + index);
		text[2] = '\0';
	} else if (index < 100U) {
		text[1] = (char)('0' + index / 10U);
		text[2] = (char)('0' + index % 10U);
		text[3] = '\0';
	} else {
		text[1] = (char)('0' + index / 100U);
		text[2] = (char)('0' + (index / 10U) % 10U);
		text[3] = (char)('0' + index % 10U);
		text[4] = '\0';
	}
}

void oled_screen_content_draw(void)
{
	zmk_keymap_layer_index_t index =
		zmk_keymap_highest_layer_active();
	char index_text[5];
	const char *label;

	oled_ui_format_layer_index(index, index_text);
	label = index < (sizeof(layer_abbreviations) /
			 sizeof(layer_abbreviations[0])) ?
		layer_abbreviations[index] : index_text;

	oled_gfx_draw_text(1, 0, "SOFLE", true);
	oled_gfx_draw_text(1, 16, "-----", true);
	oled_gfx_draw_text(1, 36, "LAYER", true);
	oled_gfx_draw_text(oled_ui_centered_x(label), 60, label, true);
	oled_gfx_draw_text(oled_ui_centered_x(index_text), 78,
			   index_text, true);
	oled_gfx_draw_text(4, 108, "LEFT", true);
}

const char *oled_screen_content_name(void)
{
	return "active layer OLED UI 32x128 V2";
}

static int oled_ui_layer_state_changed(const zmk_event_t *event)
{
	(void)event;
	oled_screen_request_redraw();
	return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(oled_ui_v2_layer, oled_ui_layer_state_changed);
ZMK_SUBSCRIPTION(oled_ui_v2_layer, zmk_layer_state_changed);
