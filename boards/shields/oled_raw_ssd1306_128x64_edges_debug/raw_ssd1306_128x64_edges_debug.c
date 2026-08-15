/*
 * One-shot raw SSD1306 128x64 edge debug diagnostic.
 *
 * This is an independent diagnostic variant. The known-good RAW 128x64
 * implementation is not modified.
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

LOG_MODULE_REGISTER(raw_ssd1306_128x64_edges_debug, CONFIG_DISPLAY_LOG_LEVEL);

#define RAW_WIDTH 128U
#define RAW_HEIGHT 64U
#define RAW_PAGES (RAW_HEIGHT / 8U)
#define RAW_BUFFER_SIZE (RAW_WIDTH * RAW_PAGES)

static const char raw_build_marker[] =
	"RUTTIGER_RAW_SSD1306_128X64_EDGES_DEBUG_V1";
static const struct device *const raw_display =
	DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static uint8_t raw_framebuffer[RAW_BUFFER_SIZE];
static uint8_t raw_clear_buffer[RAW_BUFFER_SIZE];

static const struct display_buffer_descriptor raw_buffer_descriptor = {
	.buf_size = sizeof(raw_framebuffer),
	.width = RAW_WIDTH,
	.height = RAW_HEIGHT,
	.pitch = RAW_WIDTH,
};

/*
 * Zephyr v0.3's public display.h has no display_clear() operation. Keep the
 * required clear step explicit and implement it with the public display_write
 * operation used by this SSD1306 driver.
 */
static int display_clear(const struct device *display)
{
	memset(raw_clear_buffer, 0, sizeof(raw_clear_buffer));

	return display_write(display, 0, 0, &raw_buffer_descriptor,
			    raw_clear_buffer);
}

static void raw_set_white_pixel(const struct display_capabilities *caps,
				uint16_t x, uint16_t y)
{
	size_t index;
	uint8_t bit;

	if (x >= RAW_WIDTH || y >= RAW_HEIGHT) {
		return;
	}

	/* SSD1306 v0.3: one byte is one vertical 8-pixel tile. */
	index = ((size_t)y / 8U) * RAW_WIDTH + x;
	bit = (caps->screen_info & SCREEN_INFO_MONO_MSB_FIRST) ?
		(7U - (y & 7U)) : (y & 7U);

	/* MONO10 is 1=black, 0=white; inversion-on makes this physical white. */
	raw_framebuffer[index] &= (uint8_t)~BIT(bit);
}

static void raw_hline(const struct display_capabilities *caps,
			      uint16_t x0, uint16_t x1, uint16_t y)
{
	for (uint16_t x = x0; x <= x1; ++x) {
		raw_set_white_pixel(caps, x, y);
	}
}

static void raw_vline(const struct display_capabilities *caps,
			      uint16_t x, uint16_t y0, uint16_t y1)
{
	for (uint16_t y = y0; y <= y1; ++y) {
		raw_set_white_pixel(caps, x, y);
	}
}

static void raw_build_pattern(const struct display_capabilities *caps)
{
	/* MONO10 logical background is black (1); white pattern pixels are 0. */
	memset(raw_framebuffer, 0xff, sizeof(raw_framebuffer));

	/* Four complete horizontal rows at each physical edge. */
	for (uint16_t y = 0; y <= 3; ++y) {
		raw_hline(caps, 0, RAW_WIDTH - 1U, y);
	}
	for (uint16_t y = RAW_HEIGHT - 4U; y < RAW_HEIGHT; ++y) {
		raw_hline(caps, 0, RAW_WIDTH - 1U, y);
	}

	/* Outer vertical edges. */
	raw_vline(caps, 0, 0, RAW_HEIGHT - 1U);
	raw_vline(caps, RAW_WIDTH - 1U, 0, RAW_HEIGHT - 1U);

	/* Physical midpoint cross. */
	raw_vline(caps, RAW_WIDTH / 2U, 1, RAW_HEIGHT - 2U);
	raw_hline(caps, 1, RAW_WIDTH - 2U, RAW_HEIGHT / 2U);
}

static void raw_ssd1306_work_handler(struct k_work *work)
{
	struct display_capabilities caps;
	int ret;

	ARG_UNUSED(work);

	LOG_INF("build marker: %s", raw_build_marker);

	if (!device_is_ready(raw_display)) {
		LOG_ERR("display device %s is not ready; diagnostic aborted",
			raw_display->name);
		return;
	}

	display_get_capabilities(raw_display, &caps);
	LOG_INF("capabilities x=%u y=%u supported=0x%08x current=0x%x "
		"screen_info=0x%08x VTILED=%s MSB_FIRST=%s",
		caps.x_resolution, caps.y_resolution,
		caps.supported_pixel_formats, caps.current_pixel_format,
		caps.screen_info,
		(caps.screen_info & SCREEN_INFO_MONO_VTILED) ? "yes" : "no",
		(caps.screen_info & SCREEN_INFO_MONO_MSB_FIRST) ? "yes" : "no");

	if (caps.x_resolution != RAW_WIDTH || caps.y_resolution != RAW_HEIGHT ||
	    !(caps.supported_pixel_formats & PIXEL_FORMAT_MONO10) ||
	    caps.current_pixel_format != PIXEL_FORMAT_MONO10 ||
	    !(caps.screen_info & SCREEN_INFO_MONO_VTILED)) {
		LOG_ERR("unsupported SSD1306 capabilities; diagnostic aborted");
		return;
	}

	ret = display_clear(raw_display);
	if (ret < 0) {
		LOG_ERR("display_clear failed: %d", ret);
		return;
	}

	raw_build_pattern(&caps);
	ret = display_write(raw_display, 0, 0, &raw_buffer_descriptor,
			    raw_framebuffer);
	if (ret < 0) {
		LOG_ERR("display_write failed: %d", ret);
		return;
	}

	ret = display_blanking_off(raw_display);
	if (ret < 0) {
		LOG_ERR("display_blanking_off failed: %d", ret);
		return;
	}

	LOG_INF("raw SSD1306 128x64 edge debug pattern written once");
}

static K_WORK_DELAYABLE_DEFINE(raw_ssd1306_work, raw_ssd1306_work_handler);

static int raw_ssd1306_schedule(void)
{
	int ret = k_work_schedule(&raw_ssd1306_work, K_SECONDS(2));

	if (ret < 0) {
		LOG_ERR("could not schedule display diagnostic: %d", ret);
		return ret;
	}

	return 0;
}

SYS_INIT(raw_ssd1306_schedule, APPLICATION, 0);
