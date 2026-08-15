/*
 * Static LEFT screen for RAW OLED UI V1.
 *
 * No ZMK display events, battery, BLE, USB, modifiers, or animation are
 * connected in this iteration. The screen is intentionally deterministic so
 * orientation, primitives, font, and static text can be validated first.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "oled_gfx.h"
#include "oled_raw_ssd1306_128x64.h"

LOG_MODULE_REGISTER(oled_ui_left, CONFIG_DISPLAY_LOG_LEVEL);

static void oled_ui_draw_corner_markers(void)
{
	/* TL: outline square. */
	oled_gfx_rect(0, 0, 6, 6, true);

	/* TR: filled square. */
	oled_gfx_fill_rect(58, 0, 6, 6, true);

	/* BL: X. */
	for (int i = 0; i < 6; ++i) {
		oled_gfx_set_pixel(i, 122 + i, true);
		oled_gfx_set_pixel(5 - i, 122 + i, true);
	}

	/* BR: plus. */
	oled_gfx_hline(58, 63, 124, true);
	oled_gfx_vline(60, 122, 127, true);
}

static void oled_ui_draw_static_screen(void)
{
	oled_gfx_clear();
	oled_ui_draw_corner_markers();

	/* Corner labels make the orientation result unambiguous. */
	oled_gfx_draw_text(8, 0, "TL", true);
	oled_gfx_draw_text(44, 0, "TR", true);
	oled_gfx_draw_text(8, 120, "BL", true);
	oled_gfx_draw_text(44, 120, "BR", true);

	oled_gfx_draw_text(0, 24, "SOFLE", true);
	oled_gfx_draw_text(0, 40, "RAW UI", true);
	oled_gfx_draw_text(0, 56, "V1", true);
	oled_gfx_draw_text(0, 72, "64x128", true);
}

static void oled_ui_work_handler(struct k_work *work)
{
	int ret;

	ARG_UNUSED(work);

	ret = oled_raw_prepare();
	if (ret < 0) {
		return;
	}

	oled_gfx_init(oled_raw_framebuffer(), oled_raw_capabilities());
	oled_ui_draw_static_screen();
	ret = oled_gfx_flush();
	if (ret < 0) {
		LOG_ERR("OLED UI V1 flush failed: %d", ret);
		return;
	}

	LOG_INF("RAW OLED UI V1 static screen written");
}

static K_WORK_DELAYABLE_DEFINE(oled_ui_work, oled_ui_work_handler);

static int oled_ui_schedule(void)
{
	int ret = k_work_schedule(&oled_ui_work, K_SECONDS(2));

	if (ret < 0) {
		LOG_ERR("could not schedule RAW OLED UI V1: %d", ret);
		return ret;
	}

	return 0;
}

/* Keep initialization deferred until after the display stack is ready. */
SYS_INIT(oled_ui_schedule, APPLICATION, 0);
