/*
 * One-shot raw SSD1306 128x64 baseline.
 *
 * The baseline draws only the validated border and center cross.
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(oled_raw_ssd1306_128x64, CONFIG_DISPLAY_LOG_LEVEL);

#define OLED_RAW_WIDTH 128U
#define OLED_RAW_HEIGHT 64U
#define OLED_RAW_PAGES (OLED_RAW_HEIGHT / 8U)
#define OLED_RAW_BUFFER_SIZE (OLED_RAW_WIDTH * OLED_RAW_PAGES)

static const char oled_raw_build_marker[] =
	"RUTTIGER_RAW_SSD1306_128X64_BASELINE_V1";
static const struct device *const oled_raw_display =
	DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static uint8_t oled_raw_framebuffer[OLED_RAW_BUFFER_SIZE];
static uint8_t oled_raw_clear_buffer[OLED_RAW_BUFFER_SIZE];

static const struct display_buffer_descriptor oled_raw_buffer_descriptor = {
	.buf_size = sizeof(oled_raw_framebuffer),
	.width = OLED_RAW_WIDTH,
	.height = OLED_RAW_HEIGHT,
	.pitch = OLED_RAW_WIDTH,
};

/* Zephyr v0.3 has no public display_clear() operation. */
static int oled_raw_clear(const struct device *display)
{
	memset(oled_raw_clear_buffer, 0, sizeof(oled_raw_clear_buffer));

	return display_write(display, 0, 0, &oled_raw_buffer_descriptor,
			    oled_raw_clear_buffer);
}

static void oled_raw_set_pixel(const struct display_capabilities *caps,
				       uint16_t x, uint16_t y)
{
	size_t index;
	uint8_t bit;

	if (x >= OLED_RAW_WIDTH || y >= OLED_RAW_HEIGHT) {
		return;
	}

	/* SSD1306 uses one byte per vertical eight-pixel tile. */
	index = ((size_t)y / 8U) * OLED_RAW_WIDTH + x;
	bit = (caps->screen_info & SCREEN_INFO_MONO_MSB_FIRST) ?
		(7U - (y & 7U)) : (y & 7U);

	/* MONO10 is 1=black, 0=white; inversion-on makes this physical white. */
	oled_raw_framebuffer[index] &= (uint8_t)~BIT(bit);
}

static void oled_raw_hline(const struct display_capabilities *caps,
				   uint16_t x0, uint16_t x1, uint16_t y)
{
	for (uint16_t x = x0; x <= x1; ++x) {
		oled_raw_set_pixel(caps, x, y);
	}
}

static void oled_raw_vline(const struct display_capabilities *caps,
				   uint16_t x, uint16_t y0, uint16_t y1)
{
	for (uint16_t y = y0; y <= y1; ++y) {
		oled_raw_set_pixel(caps, x, y);
	}
}

static void oled_raw_build_baseline(const struct display_capabilities *caps)
{
	/* MONO10 logical background is black (1); white pixels are 0. */
	memset(oled_raw_framebuffer, 0xff, sizeof(oled_raw_framebuffer));

	/* Four complete horizontal rows at each physical edge. */
	for (uint16_t y = 0; y <= 3U; ++y) {
		oled_raw_hline(caps, 0, OLED_RAW_WIDTH - 1U, y);
	}
	for (uint16_t y = OLED_RAW_HEIGHT - 4U;
	     y < OLED_RAW_HEIGHT; ++y) {
		oled_raw_hline(caps, 0, OLED_RAW_WIDTH - 1U, y);
	}

	/* Outer vertical edges. */
	oled_raw_vline(caps, 0, 0, OLED_RAW_HEIGHT - 1U);
	oled_raw_vline(caps, OLED_RAW_WIDTH - 1U, 0, OLED_RAW_HEIGHT - 1U);

	/* Physical midpoint cross. */
	oled_raw_vline(caps, OLED_RAW_WIDTH / 2U, 1, OLED_RAW_HEIGHT - 2U);
	oled_raw_hline(caps, 1, OLED_RAW_WIDTH - 2U, OLED_RAW_HEIGHT / 2U);
}

static int oled_raw_flush(const struct device *display)
{
	int ret = display_write(display, 0, 0, &oled_raw_buffer_descriptor,
				oled_raw_framebuffer);

	if (ret < 0) {
		return ret;
	}

	return display_blanking_off(display);
}

static void oled_raw_work_handler(struct k_work *work)
{
	struct display_capabilities caps;
	int ret;

	ARG_UNUSED(work);

	LOG_INF("build marker: %s", oled_raw_build_marker);

	if (!device_is_ready(oled_raw_display)) {
		LOG_ERR("display device %s is not ready; baseline aborted",
			oled_raw_display->name);
		return;
	}

	display_get_capabilities(oled_raw_display, &caps);
	LOG_INF("capabilities x=%u y=%u supported=0x%08x current=0x%x "
		"screen_info=0x%08x VTILED=%s MSB_FIRST=%s",
		caps.x_resolution, caps.y_resolution,
		caps.supported_pixel_formats, caps.current_pixel_format,
		caps.screen_info,
		(caps.screen_info & SCREEN_INFO_MONO_VTILED) ? "yes" : "no",
		(caps.screen_info & SCREEN_INFO_MONO_MSB_FIRST) ? "yes" : "no");

	if (caps.x_resolution != OLED_RAW_WIDTH ||
	    caps.y_resolution != OLED_RAW_HEIGHT ||
	    !(caps.supported_pixel_formats & PIXEL_FORMAT_MONO10) ||
	    caps.current_pixel_format != PIXEL_FORMAT_MONO10 ||
	    !(caps.screen_info & SCREEN_INFO_MONO_VTILED)) {
		LOG_ERR("unsupported SSD1306 capabilities; baseline aborted");
		return;
	}

	ret = oled_raw_clear(oled_raw_display);
	if (ret < 0) {
		LOG_ERR("oled_raw_clear failed: %d", ret);
		return;
	}

	oled_raw_build_baseline(&caps);
	ret = oled_raw_flush(oled_raw_display);
	if (ret < 0) {
		LOG_ERR("oled_raw_flush failed: %d", ret);
		return;
	}

	LOG_INF("raw SSD1306 128x64 baseline written once");
}

static K_WORK_DELAYABLE_DEFINE(oled_raw_work, oled_raw_work_handler);

static int oled_raw_schedule(void)
{
	int ret = k_work_schedule(&oled_raw_work, K_SECONDS(2));

	if (ret < 0) {
		LOG_ERR("could not schedule raw OLED baseline: %d", ret);
		return ret;
	}

	return 0;
}

SYS_INIT(oled_raw_schedule, APPLICATION, 0);
