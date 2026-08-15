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

static void oled_portrait_set_pixel(int x, int y)
{
	oled_gfx_set_pixel(x, y, true);
}

static void oled_ui_draw_geometry(void)
{
	/* Complete logical portrait border: width 64, height 128. */
	for (int x = 0; x < OLED_GFX_WIDTH; ++x) {
		oled_portrait_set_pixel(x, 0);
		oled_portrait_set_pixel(x, OLED_GFX_HEIGHT - 1);
	}
	for (int y = 0; y < OLED_GFX_HEIGHT; ++y) {
		oled_portrait_set_pixel(0, y);
		oled_portrait_set_pixel(OLED_GFX_WIDTH - 1, y);
	}

	/* Asymmetric crosshair in logical portrait coordinates. */
	for (int y = 0; y < OLED_GFX_HEIGHT; ++y) {
		oled_portrait_set_pixel(32, y);
	}
	for (int x = 0; x < OLED_GFX_WIDTH; ++x) {
		oled_portrait_set_pixel(x, 64);
	}

	/* TOP LEFT: solid 5x5 square. */
	for (int y = 4; y < 9; ++y) {
		for (int x = 4; x < 9; ++x) {
			oled_portrait_set_pixel(x, y);
		}
	}

	/* TOP RIGHT: two horizontal bars. */
	for (int x = 52; x < 60; ++x) {
		oled_portrait_set_pixel(x, 4);
		oled_portrait_set_pixel(x, 8);
	}

	/* BOTTOM LEFT: three horizontal bars. */
	for (int x = 4; x < 14; ++x) {
		oled_portrait_set_pixel(x, 116);
		oled_portrait_set_pixel(x, 121);
		oled_portrait_set_pixel(x, 126);
	}

	/* BOTTOM RIGHT: large L. */
	for (int y = 112; y < 125; ++y) {
		oled_portrait_set_pixel(54, y);
	}
	for (int x = 54; x < 62; ++x) {
		oled_portrait_set_pixel(x, 124);
	}
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
	oled_gfx_clear();
	oled_ui_draw_geometry();
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
