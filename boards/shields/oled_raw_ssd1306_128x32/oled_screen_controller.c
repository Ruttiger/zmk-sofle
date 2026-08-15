/*
 * Deferred one-shot controller shared by the smoke and UI builds.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "oled_gfx_portrait.h"
#include "oled_raw_ssd1306_128x32.h"
#include "oled_screen_content.h"

LOG_MODULE_REGISTER(oled_screen_controller, CONFIG_DISPLAY_LOG_LEVEL);

static void oled_screen_work_handler(struct k_work *work)
{
	int ret;

	ARG_UNUSED(work);

	ret = oled_raw_prepare();
	if (ret < 0) {
		return;
	}

	oled_gfx_init();
	oled_gfx_clear();
	oled_screen_content_draw();

	ret = oled_gfx_flush();
	if (ret < 0) {
		LOG_ERR("OLED content flush failed: %d", ret);
		return;
	}

	LOG_INF("%s written once; framebuffer=%u bytes",
		oled_screen_content_name(), OLED_RAW_BUFFER_SIZE);
}

static K_WORK_DELAYABLE_DEFINE(oled_screen_work, oled_screen_work_handler);

static int oled_screen_schedule(void)
{
	int ret = k_work_schedule(&oled_screen_work, K_SECONDS(2));

	if (ret < 0) {
		LOG_ERR("could not schedule deferred OLED content: %d", ret);
	}

	/* OLED errors must never prevent the keyboard from booting. */
	return 0;
}

SYS_INIT(oled_screen_schedule, APPLICATION, 0);
